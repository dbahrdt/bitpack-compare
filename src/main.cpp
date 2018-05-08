#include <sserialize/containers/ItemIndexPrivates/ItemIndexPrivateFoR.h>
#include <sserialize/stats/TimeMeasuerer.h>
#include <sserialize/utility/Bitpacking.h>
#include <sserialize/iterator/MultiBitBackInserter.h>

#include <blockpacking.h>

#include <iostream>
#include <iomanip>
#include <array>


struct BencherSserialize {
	BencherSserialize() : dest(0, sserialize::MM_PROGRAM_MEMORY) {
		for(uint32_t bits(1); bits < m_unpackers.size(); ++bits) {
			m_unpackers[bits] = sserialize::BitunpackerInterface::unpacker(bits);
		}
	}
public:
	void pack(const std::vector<uint32_t> & src, uint32_t bits) {
		dest.resetPtrs();
		sserialize::MultiBitBackInserter inserter(dest);
		inserter.push_back(src.begin(), src.end(), bits);
	}
	void check(const std::vector<uint32_t> & src, uint32_t bits) {
		if (block.size() != src.size()) {
			throw std::runtime_error("BencherSserialize: block.size != src.size");
		}
		for(std::size_t i(0), s(src.size()); i < s; ++i) {
			if (src[i] != block[i]) {
				throw std::runtime_error(
					"BencherSserialize: with i=" +
					std::to_string(i) + " block[i]=" + std::to_string(block[i])
					+ " != " + "src[i]=" + std::to_string(src[i]));
			}
		}
	}
	void warmup(const std::vector<uint32_t> & src, uint32_t bits) {
		unpack(src, bits);
	}
	void unpack(const std::vector<uint32_t> & src, uint32_t bits) {
		block.resize(src.size());
		auto memv = dest.asMemView();
		const uint8_t * sit = memv.data();
		uint32_t * dit = block.data();
		uint32_t count = src.size();
		m_unpackers.at(bits)->unpack(sit, dit, count);
		if (count != 0) {
			throw std::runtime_error("BencherSserialize: blocksize not a multiple unpacker blocksize");
		}
	}
public:
	std::array<std::unique_ptr<sserialize::BitunpackerInterface>, 57> m_unpackers;
	sserialize::UByteArrayAdapter dest;
	std::vector<uint32_t> block;
};

class BencherFoRBlock {
public:
	BencherFoRBlock() :
	dest(0, sserialize::MM_PROGRAM_MEMORY),
	block(dest, 0, 0, 1)
	{}
public:
	void pack(const std::vector<uint32_t> & src, uint32_t bits) {
		dest.resetPtrs();
		sserialize::detail::ItemIndexImpl::FoRCreator::encodeBlock(dest, src.begin(), src.end(), bits);
		
	}
	void check(const std::vector<uint32_t> & src, uint32_t bits) {
		if (dest.tellPutPtr() != (bits*src.size()/8 + uint64_t((bits*src.size()%8)>0))) {
			throw std::runtime_error("Compressed size wrong: " + std::to_string(dest.tellGetPtr()));
		}
		for(uint32_t i(0), prev(0); i < src.size(); ++i) {
			prev += src[i];
			if (block.at(i) != prev) {
				throw std::runtime_error("BencherFoRBlock: block.at(" + std::to_string(i) + ")=" + std::to_string(block.at(i)) + "!=" + std::to_string(prev));
			}
		}
	}
	void warmup(const std::vector<uint32_t> & src, uint32_t bits) {
		unpack(src, bits);
	}
	void unpack(const std::vector<uint32_t> & src, uint32_t bits) {
		block.update(dest, 0, src.size(), bits);
	}
public:
	sserialize::UByteArrayAdapter dest;
	sserialize::detail::ItemIndexImpl::FoRBlock block;
};

struct BencherFastPFoR {
	BencherFastPFoR()
	{}
public:
	void pack(const std::vector<uint32_t> & src, uint32_t bits) {
		uint64_t totalBits = src.size()*bits;
		dest.resize(src.size() + packer.HowManyMiniBlocks); //totalBits/32 + uint32_t((totalBits%32)>0));
		packer.encodeArray(src.data(), src.size(), dest.data(), nvalue);
	}
	void check(const std::vector<uint32_t> & src, uint32_t bits) {
		if (block.size() != src.size()) {
			throw std::runtime_error("BencherFastPFoR: block.size != src.size");
		}
		for(std::size_t i(0), s(src.size()); i < s; ++i) {
			if (src[i] != block[i]) {
				throw std::runtime_error(
					"BencherFastPFoR: with i=" +
					std::to_string(i) + " block[i]=" + std::to_string(block[i])
					+ " != " + "src[i]=" + std::to_string(src[i]));
			}
		}
	}
	void warmup(const std::vector<uint32_t> & src, uint32_t bits) {
		unpack(src, bits);
	}
	void unpack(const std::vector<uint32_t> & src, uint32_t bits) {
		block.resize(src.size());
		packer.decodeArray(dest.data(), src.size(), block.data(), nvalue);
	}
public:
	FastPForLib::BinaryPacking<8> packer;
	std::vector<uint32_t> dest;
	std::vector<uint32_t> block;
	size_t nvalue;
};

typedef enum {BS_SSERIALIZE=1, BS_FORBLOCK=2, BS_FAST_PFOR=4} BenchSelector;

void bench(uint32_t bitsBegin, uint32_t bitsEnd, uint64_t blockSize, uint32_t runs, int benchSelector) {
	sserialize::TimeMeasurer sserializeEncode, forBlockEncode, fastpforEncode;
	sserialize::TimeMeasurer sserializeDecode, forBlockDecode, fastpforDecode;
	
	BencherSserialize bs;
	BencherFoRBlock bb;
	BencherFastPFoR bf;

	std::vector<uint32_t> src(blockSize);

	std::cout << "bits\tpack forblock:fastpfor[ms]\tunpack forblock:fastpfor[ms]\tunpack forblock:fastpfor[M/s]" << std::endl;
		
	std::cout << std::setprecision(4);

	for(uint32_t bits(bitsBegin); bits <= bitsEnd; ++bits) {

		uint32_t mask = sserialize::createMask(bits);
		for(uint32_t i(0); i < blockSize; ++i) {
			src[i] = i & mask;
		}
		
		if (benchSelector & BS_SSERIALIZE && bits <= 56) { //sserialize
			sserializeEncode.begin();
			bs.pack(src, bits);
			sserializeEncode.end();
			
			bs.warmup(src, bits);
			bs.check(src, bits);
			bs.warmup(src, bits);
			
			sserializeDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bs.unpack(src, bits);
			}
			sserializeDecode.end();
		} //end sserialize

		if (benchSelector & BS_FORBLOCK && bits <= 32) { //for block
			forBlockEncode.begin();
			bb.pack(src, bits);
			forBlockEncode.end();
			
			bb.warmup(src, bits);
			bb.check(src, bits);
			bb.warmup(src, bits);
			
			forBlockDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bb.unpack(src, bits);
			}
			forBlockDecode.end();
		} //end sserialize

		if (benchSelector & BS_FAST_PFOR && bits <= 32) { //FastPFoR
			fastpforEncode.begin();
			bf.pack(src, bits);
			fastpforEncode.end();
			
			bf.warmup(src, bits);
			bf.check(src, bits);
			bf.warmup(src, bits);
			
			fastpforDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bf.unpack(src, bits);
			}
			fastpforDecode.end();
		} //end FastPFoR
		
		std::cout << bits << '\t';
		std::cout << sserializeEncode.elapsedMilliSeconds() << ':';
		std::cout << forBlockEncode.elapsedMilliSeconds() << ':';
		std::cout << fastpforEncode.elapsedMilliSeconds() << '\t';
		
		std::cout << sserializeDecode.elapsedMilliSeconds()/runs << ':';
		std::cout << forBlockDecode.elapsedMilliSeconds()/runs << ':';
		std::cout << fastpforDecode.elapsedMilliSeconds()/runs << '\t';
		
		std::cout << ((blockSize*runs)/(double(sserializeDecode.elapsedUseconds())/1000000))/1000000 << ':';
		std::cout << ((blockSize*runs)/(double(forBlockDecode.elapsedUseconds())/1000000))/1000000 << ':';
		std::cout << ((blockSize*runs)/(double(fastpforDecode.elapsedUseconds())/1000000))/1000000 << std::endl;
	}
}

void help() {
	std::cout << "prg -bb <bits begin> -be <bits end> -s <log_2(test size)> -r <test runs> [-b <test selection = sserialize|forblock|fastpfor>]*" << std::endl;
}

int main(int argc, char ** argv) {
	uint32_t bitsBegin = 1;
	uint32_t bitsEnd = 32;
	uint64_t blockSize = 1 << 25;
	uint32_t runs = 16;
	int benchSelector = 0;
	for(int i(0); i < argc; ++i) {
		std::string token(argv[i]);
		if (token == "-bb" && i+1 < argc) {
			bitsBegin = ::atoi(argv[i+1]);
			++i;
		}
		else if (token == "-bb" && i+1 < argc) {
			bitsBegin = ::atoi(argv[i+1]);
			++i;
		}
		else if (token == "-be" && i+1 < argc) {
			bitsEnd = ::atoi(argv[i+1]);
			++i;
		}
		else if (token == "-s" && i+1 < argc) {
			blockSize = 1 << ::atoi(argv[i+1]);
			++i;
		}
		else if (token == "-r" && i+1 < argc) {
			runs = ::atoi(argv[i+1]);
			++i;
		}
		else if (token == "-b" && i+1 < argc) {
			token = std::string(argv[i+1]);
			if (token == "ss" || token == "sserialize") {
				benchSelector |= BS_SSERIALIZE;
			}
			else if (token == "forblock" || token == "for") {
				benchSelector |= BS_FORBLOCK;
			}
			else if (token == "fastpfor" || token == "pfor") {
				benchSelector |= BS_FAST_PFOR;
			}
			else {
				std::cerr << "Unknown benchmark type: " << token << std::endl;
				help();
				return -1;
			}
		}
		else if (token == "-h" || token == "--help") {
			help();
			return 0;
		}
	}
	
	if (!benchSelector) {
		benchSelector = std::numeric_limits<int>::max();
	}
	
	bench(bitsBegin, bitsEnd, blockSize, runs, benchSelector);
	return 0;
}
