#include <sserialize/containers/ItemIndexPrivates/ItemIndexPrivateFoR.h>
#include <sserialize/stats/TimeMeasuerer.h>
#include <sserialize/utility/Bitpacking.h>
#include <sserialize/iterator/MultiBitBackInserter.h>

#include <blockpacking.h>

#include <iostream>
#include <iomanip>
#include <array>
/*
struct BencherPFoRBlock {
	BencherPFoRBlock(sserialize::ItemIndex::Types type) :
	m_type(type),
	dest(0, sserialize::MM_PROGRAM_MEMORY)
	{}
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
	sserialize::ItemIndex::Types m_type;
	sserialize::UByteArrayAdapter dest;
	std::vector<uint32_t> block;
};*/


struct BencherSserialize {
	BencherSserialize() : dest(0, sserialize::MM_PROGRAM_MEMORY) {
		for(uint32_t bits(1); bits < m_unpackers.size(); ++bits) {
			m_unpackers[bits] = sserialize::BitpackingInterface::instance(bits);
		}
	}
public:
	void pack(const std::vector<uint32_t> & src, uint32_t bits) {
		dest.resetPtrs();
		
		dest.resize(sserialize::CompactUintArray::minStorageBytes(bits, src.size()));
		dest.zero();
		auto memv = dest.asMemView();
		const uint32_t * sit = src.data();
		uint8_t * dit = memv.data();
		uint32_t count = src.size();
		m_unpackers.at(bits)->pack_blocks(sit, dit, count);
		if (count != 0) {
			throw std::runtime_error("BencherSserialize: blocksize not a multiple unpacker blocksize");
		}
		memv.flush();
// 		sserialize::MultiBitBackInserter inserter(dest);
// 		inserter.push_back(src.begin(), src.end(), bits);
	}
	void check(const std::vector<uint32_t> & src, uint32_t bits) {
		if (block.size() != src.size()) {
			throw std::runtime_error("BencherSserialize: block.size != src.size");
		}
		for(std::size_t i(0), s(src.size()); i < s; ++i) {
			if (src[i] != block[i]) {
				throw std::runtime_error(
					"BencherSserialize: bits=" + std::to_string(bits) + " i=" +
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
		m_unpackers.at(bits)->unpack_blocks(sit, dit, count);
		if (count != 0) {
			throw std::runtime_error("BencherSserialize: blocksize not a multiple unpacker blocksize");
		}
	}
public:
	std::array<std::unique_ptr<sserialize::BitpackingInterface>, 57> m_unpackers;
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
		uint64_t blockBytes = sserialize::CompactUintArray::minStorageBytes(bits, src.size());
		if (dest.tellPutPtr() != blockBytes) {
			throw std::runtime_error("Compressed size wrong: SHOULD=" + std::to_string(blockBytes) + "; IS=" + std::to_string(dest.tellPutPtr()));
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
	static constexpr std::size_t BlockSize = 16;
public:
	BencherFastPFoR() {}
public:
	void pack(const std::vector<uint32_t> & src, uint32_t bits) {
		uint64_t totalBits = src.size()*bits;
		dest.resize(4*src.size());
		std::size_t numBlocks = src.size()/BlockSize;
		auto out = dest.data();
		for(std::size_t i(0); i < numBlocks; ++i) {
			out = FastPForLib::fastunalignedpackwithoutmask_16(src.data() + i*BlockSize, out, bits);
		}
// 		packer.encodeArray(src.data(), src.size(), dest.data(), nvalue);
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
		if (src.size() % 32 != 0) {
			throw std::runtime_error("src.size not divisible by 32");
		}
		std::size_t numBlocks = src.size()/BlockSize;
		const uint8_t * in = dest.data();
		uint32_t * out = block.data();
		for(std::size_t i(0); i < numBlocks; ++i) {
			in = FastPForLib::fastunalignedunpack_16(in, out + i*BlockSize, bits);
		}
// 		packer.decodeArray(dest.data(), src.size(), block.data(), nvalue);
	}
public:
	FastPForLib::BinaryPacking<8> packer;
	std::vector<uint8_t> dest;
	std::vector<uint32_t> block;
	size_t nvalue;
};

typedef enum {BS_SSERIALIZE=1, BS_FORBLOCK=2, BS_FAST_PFOR=4} BenchSelector;


typedef enum { PT_TIMES, PT_THROUGHPUT} PrintType;

void bench(uint32_t bitsBegin, uint32_t bitsEnd, uint64_t blockSize, uint32_t runs, int benchSelector, int printType, char seperator) {
	sserialize::TimeMeasurer sserializeEncode, forBlockEncode, fastpforEncode;
	sserialize::TimeMeasurer sserializeDecode, forBlockDecode, fastpforDecode;
	
	BencherSserialize bs;
	BencherFoRBlock bb;
	BencherFastPFoR bf;

	std::vector<uint32_t> src(blockSize);
	
	std::stringstream headerss;
	std::string unitstr;
	if (printType == PT_TIMES) {
		unitstr = "[ms]";
	}
	else {
		unitstr = "[M/s]";
	}
	{
		bool hasPrev = false;
		if (benchSelector & BS_SSERIALIZE) {
			headerss << "sserialize";
			hasPrev = true;
		}
		if (benchSelector & BS_FORBLOCK) {
			if (hasPrev) {
				headerss << seperator;
			}
			std::cout << "forblock";
			hasPrev = true;
		}
		if (benchSelector & BS_FAST_PFOR) {
			if (hasPrev) {
				headerss << seperator;
			}
			headerss << "fastpfor";
			hasPrev = true;
		}
	}
	std::cout << "#pack, then unpack" << std::endl;
	std::cout << "#unit: " << unitstr << std::endl;
	std::cout << "bits" << seperator <<  headerss.str() << seperator << headerss.str() << std::endl;
	std::cout << std::setprecision(4);

	for(uint32_t bits(bitsBegin); bits <= bitsEnd; ++bits) {

		uint32_t mask = sserialize::createMask(bits);
		for(uint32_t i(0); i < blockSize; ++i) {
			src[i] = i & mask;
		}
		
		if (benchSelector & BS_SSERIALIZE && bits <= 56) { //sserialize
			bs.pack(src, bits);
			bs.unpack(src, bits);
			bs.check(src, bits);
			
			sserializeEncode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bs.pack(src, bits);
			}
			sserializeEncode.end();
			
			bs.warmup(src, bits);
			sserializeDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bs.unpack(src, bits);
			}
			sserializeDecode.end();
		} //end sserialize

		if (benchSelector & BS_FORBLOCK && bits <= 32) { //for block
			bb.pack(src, bits);			
			bb.unpack(src, bits);
			bb.check(src, bits);
			
			forBlockEncode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bb.pack(src, bits);
			}
			forBlockEncode.end();
			
			bb.warmup(src, bits);
			forBlockDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bb.unpack(src, bits);
			}
			forBlockDecode.end();
		} //end sserialize

		if (benchSelector & BS_FAST_PFOR && bits <= 32) { //FastPFoR
			bf.pack(src, bits);
			bf.unpack(src, bits);
			bf.check(src, bits);
			
			fastpforEncode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bf.pack(src, bits);
			}
			fastpforEncode.end();
			
			bf.warmup(src, bits);
			fastpforDecode.begin();
			for(uint32_t i(0); i < runs; ++i) {
				bf.unpack(src, bits);
			}
			fastpforDecode.end();
		} //end FastPFoR
		
		std::cout << bits;
		
		if (printType == PT_TIMES) {
			if (benchSelector & BS_SSERIALIZE) {
				std::cout << seperator << sserializeEncode.elapsedMilliSeconds()/runs;
			}
			if (benchSelector & BS_FORBLOCK) {
				std::cout << seperator << forBlockEncode.elapsedMilliSeconds()/runs;
			}
			if (benchSelector & BS_FAST_PFOR) {
				std::cout << seperator << forBlockEncode.elapsedMilliSeconds()/runs;
			}
			if (benchSelector & BS_SSERIALIZE) {
				std::cout << seperator << sserializeDecode.elapsedMilliSeconds()/runs;
			}
			if (benchSelector & BS_FORBLOCK) {
				std::cout << seperator<< forBlockDecode.elapsedMilliSeconds()/runs;
			}
			if (benchSelector & BS_FAST_PFOR) {
				std::cout << seperator << fastpforDecode.elapsedMilliSeconds()/runs;
			}
			std::cout << std::endl;
		}
		else {
			if (benchSelector & BS_SSERIALIZE) {
				std::cout << seperator << ((blockSize*runs)/(double(sserializeEncode.elapsedUseconds())/1000000))/1000000 ;
			}
			if (benchSelector & BS_FORBLOCK) {
				std::cout << seperator << ((blockSize*runs)/(double(forBlockEncode.elapsedUseconds())/1000000))/1000000;
			}
			if (benchSelector & BS_FAST_PFOR) {
				std::cout << seperator << ((blockSize*runs)/(double(fastpforEncode.elapsedUseconds())/1000000))/1000000;
			}
			
			if (benchSelector & BS_SSERIALIZE) {
				std::cout << seperator << ((blockSize*runs)/(double(sserializeDecode.elapsedUseconds())/1000000))/1000000;
			}
			if (benchSelector & BS_FORBLOCK) {
				std::cout << seperator << ((blockSize*runs)/(double(forBlockDecode.elapsedUseconds())/1000000))/1000000;
			}
			if (benchSelector & BS_FAST_PFOR) {
				std::cout << seperator << ((blockSize*runs)/(double(fastpforDecode.elapsedUseconds())/1000000))/1000000;
			}
			std::cout << std::endl;
		}
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
	int printType = PT_THROUGHPUT;
	char seperator = '\t';
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
		else if (token == "-t") {
			printType = PT_TIMES;
		}
		else if ((token == "--sep" || token == "-sep") && i+1 < argc) {
			seperator = argv[i+1][0];
			++i;
		}
	}
	
	if (!benchSelector) {
		benchSelector = std::numeric_limits<int>::max();
	}
	
	bench(bitsBegin, bitsEnd, blockSize, runs, benchSelector, printType, seperator);
	return 0;
}
