#include <iostream>
#include <iomanip>
#include <sserialize/containers/ItemIndexPrivates/ItemIndexPrivateFoR.h>
#include <sserialize/stats/TimeMeasuerer.h>
#include <blockpacking.h>


class BencherSserialize {
public:
	BencherSserialize() :
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
				throw std::runtime_error("BencherSserialize: block.at(" + std::to_string(i) + ")=" + std::to_string(block.at(i)) + "!=" + std::to_string(prev));
			}
		}
	}
	void warmup(const std::vector<uint32_t> & src, uint32_t bits) {
		block.update(dest, 0, src.size(), bits);
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
		block.resize(src.size());
		packer.decodeArray(dest.data(), src.size(), block.data(), nvalue);
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

void bench(uint64_t blockSize, uint32_t runs) {
	sserialize::TimeMeasurer sserializeEncode, sserializeDecode, fastpforEncode, fastpforDecode;
	BencherSserialize bs;
	BencherFastPFoR bf;

	std::vector<uint32_t> src(blockSize);

	std::cout << "bits\tpack sserialize:fastpfor[ms]\tunpack sserialize:fastpfor[ms]\tunpack sserialize:fastpfor[M/s]" << std::endl;
		
	std::cout << std::setprecision(4);

	for(uint32_t bits(1); bits < 32; ++bits) {

		uint32_t mask = sserialize::createMask(bits);
		for(uint32_t i(0); i < blockSize; ++i) {
			src[i] = i & mask;
		}
		
		{ //sserialize
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
		
		{ //FastPFoR
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
		std::cout << sserializeEncode.elapsedMilliSeconds() << ':' << fastpforEncode.elapsedMilliSeconds() << '\t';
		std::cout << sserializeDecode.elapsedMilliSeconds()/runs << ':' << fastpforDecode.elapsedMilliSeconds()/runs << '\t';
		std::cout << ((blockSize*runs)/(double(sserializeDecode.elapsedUseconds())/1000000))/1000000 << ':';
		std::cout << ((blockSize*runs)/(double(fastpforDecode.elapsedUseconds())/1000000))/1000000 << std::endl;
	}
}


int main() {
	uint64_t blockSize = 1 << 25;
	uint32_t runs = 16;
	bench(blockSize, runs);
	return 0;
}
