#pragma once 
#include <sstream>
#include <vector>

namespace uSnippets {
namespace Codecs {
/*	
template<typename D>
class Codec {
Codec();
public:

	template<typename T>
	static std::string encode(const std::vector<T> &in, uint8_t level = 5, uint16_t blockSize = 1024) {

		std::ostringstream os;

		std::vector<uint16_t> ind(in.size());
		std::unordered_map<T,uint16_t> c;
		for (uint i=0; i<in.size(); i++)
			ind[i] = c.insert({in[i], c.size()}).first->second;

		uint32_t N = c.size();
		std::vector<T> d(N);
		for (auto &ch : c) d[ch.second] = ch.first;
		
		os.write((char *)&N,sizeof(N));
		os.write((char *)&d[0],N*sizeof(d[0]));

		os << D::encode(ind, N, level, blockSize);
		
		return os.str();
	}
		

	template<typename T>
	static std::vector<T> decode(std::istringstream &is) {

		uint32_t N;
		is.read((char *)&N,sizeof(N));
		std::vector<T> d(N);
		is.read((char *)&d[0],N*sizeof(d[0]));		
		
		std::vector<uint16_t> outd = D::decode(is, N);
		
		std::vector<T> out(outd.size());
		for (uint i=0; i<out.size(); i++) out[i] = d[outd[i]];
		
		return out;
	}


	template<typename T> 
	static std::vector<T>        decode(const std::string &in,        uint8_t level = 5, uint16_t blockSize = 0 ) { 
		std::istringstream is(in); return decode<T>(is, level, blockSize); } 
	static std::vector<uint16_t> decode(const std::string &in, int N, uint8_t level = 5, uint16_t blockSize = 0 ) { 
		std::istringstream is(in); return decode(is, N, level, blockSize); }
};*/

class Range {

	static constexpr uint32_t TOP = 1ULL<<24;
	static constexpr uint32_t BOTTOM = 1ULL<<16;
	
	class FenwickTree {

		uint16_t M;
		std::vector<uint16_t> F;
		std::vector<uint16_t> T;
	public:
		
		FenwickTree(size_t N) : M(0), F(N), T(N) {
			for (uint i = 0 ; i<N; i++)
				add(i, 1);
		}
		
		~FenwickTree() {}
	 
		void add(int i, int delta) {

			if (M+delta>65535) {
				for (uint j=0; j<F.size(); j++)
					add(j,-(F[j]>>1));				
			}
										
			M += delta;
			F[i] += delta;
			for (; i < int(T.size()); i |= i + 1)
				T[i] += delta;
		}
	 
		uint16_t freq(int i) { return F[i]; }
	 
		uint16_t cum(int i) {
			uint16_t r = 0;
			while (i>=0) {
				r += T[i];
				i &= i + 1;
				i--;
			}
			return r;
		}
		
		uint16_t sum() { return M; }
		
		// Find largest index whose cummulative value is smaller or equal than val
		// uint32_t s = M; while (s and cum(s-1) > val) s--;
		uint16_t find(uint16_t val) {
			
			int l=-1;
			uint16_t r = T.size()-1;
			while (l<r-1) {
				uint16_t dl = 1;
				while (l+dl<r and T[l+dl]<=val) dl<<=1;
				r = std::min(uint16_t(l+dl),r);
				dl >>= 1;
				if (not dl) break;
				l += dl;
				val -= T[l];
			}
			return l+1;
		}
	};

	
public:

	template<typename T>
	static std::string encode(const std::vector<T> &in, uint N, uint8_t level=0) {

		std::ostringstream os;
		
		os.write((char *)&level,sizeof(level));		
						
		std::vector<FenwickTree> ocfreq(1<<level,FenwickTree(N+1));
		uint32_t low(0), range(0xFFFFFFFFU), hist(0);	
		for (uint i=0; i<=in.size(); i++) {

			uint s = (i!=in.size())?in[i]:N;
			auto &cfreq = ocfreq[hist];
			range /= cfreq.sum();
			low   += cfreq.cum(s-1)*range;
			range *= cfreq.freq(s);

			while ((low ^ (low+range))<TOP or (range<BOTTOM and ((range = -low & (BOTTOM - 1)),true))) {
				
				os.put(low>>24);
				range<<=8;
				low<<=8;
			}

			cfreq.add(s,8);
			hist = ((hist<<4)^(s&15))&((1<<level)-1);
		}

		for(int i=0;i<4;i++) {
			os.put(low>>24);
			low<<=8;
		}		
		return os.str();		
	}

//https://www.ffmpeg.org/doxygen/2.2/rangecoder_8c_source.html
	template<typename T>
	static std::string encodeBin(const std::vector<T> &in, uint8_t level=0) {

		std::ostringstream os;
		os.put(level);

		uint32_t size=in.size();
		for (int i=0; i<4; i++) { os.put(size>>24); size = size<<8; }
		
		std::vector<uint16_t> hfreq(1<<level,1);	 	
		std::vector<uint16_t> hfreq2(1<<level,1);	 	
		std::vector<uint16_t> hfreqsz(1<<level,2);		
		uint32_t low(0), range(0xFFFFU), hist(0);
		int outstanding_byte=-1, outstanding_count(0);
		
		auto renorm_encode = [&](){ 
			while (range < 0x100) {
				if (outstanding_byte < 0) {
					outstanding_byte = low >> 8;
				} else if (low <= 0xFF00) {
					os.put(outstanding_byte);
					for (; outstanding_count; outstanding_count--) os.put(char(0xFF));
					outstanding_byte = low >> 8;
				} else if (low >= 0x10000) {
					os.put(outstanding_byte + 1);
					for (; outstanding_count; outstanding_count--) os.put(0x00);
					outstanding_byte = (low >> 8) & 0xFF;
				} else {
					outstanding_count++;
				}
				low = (low & 0xFF) << 8;
				range <<= 8;
			}			
		};
			
		for (auto s :in) {
			s = !!s;

			uint32_t r;
			if (hfreqsz[hist]<(1<<8)) {
				r = (hfreq[hist]*range)/hfreqsz[hist];
			} else {
				
				if ((hfreqsz[hist] & 0xFF) == 0) {
					hfreq2[hist] = hfreq[hist];
					hfreqsz[hist] = 256 + 2;
					hfreq[hist] = 1;
				}
				
				r = (hfreq2[hist]*range)>>8;
			}
			if (not s) {
				range -= r;
			} else {
				low += range-r;
				range = r;
				hfreq[hist]++;
			}
			hfreqsz[hist]++;
			renorm_encode();
			
			hist = ((hist<<1)+s)&((1<<level)-1);
		}
		range=0xFF;
		low+=0xFF;
		renorm_encode();
		range=0xFF;
		renorm_encode();
		os.put(0x00);
		os.put(0x00);
		os.put(char(0xBE));
		os.put(char(0xEF));
		
		return os.str();	
	}
		
	static std::vector<uint16_t> decode(std::istringstream &is, uint N) {
		
		uint8_t level;
		is.read((char *)&level, sizeof(level));

		std::vector<FenwickTree> ocfreq(1<<level,FenwickTree(N+1));		
		std::vector<uint16_t> out;
		uint32_t low(0), range(0xFFFFFFFFU), code(0), hist(0);
		for (uint i=0; i<4; i++) code = (code<<8) | is.get();
		while (is) {
			
			auto &cfreq = ocfreq[hist];
			range /= cfreq.sum();			
			uint16_t s = cfreq.find((code-low)/range);
			if (s==N) break;
			out.push_back(s);
						
			low   += cfreq.cum(s-1)*range;
			range *= cfreq.freq(s);
						
			while ((low ^ (low+range))<TOP or (range<BOTTOM and ((range = -low & (BOTTOM - 1)),true))) {
			
				code = (code<<8) | is.get();
				range<<=8;
				low<<=8;
			}
			
			cfreq.add(s,8);
			hist = ((hist<<4)^(s&15))&((1<<level)-1);
		}
		return out;		
	}
	
	static std::vector<uint16_t> decode(const std::string &in, uint N) { std::istringstream is(in); return decode(is, N); } 

	static std::vector<uint8_t> decodeBin(std::istringstream &is) {

		uint8_t level;
		is.read((char *)&level, sizeof(level));

		uint32_t size=0;
		for (uint i=0; i<4; i++) size = (size<<8) | is.get();

		std::vector<uint8_t> out;
		std::vector<uint16_t> hfreq(1<<level,1);
		std::vector<uint16_t> hfreq2(1<<level,1);	 	
		std::vector<uint16_t> hfreqsz(1<<level,2);		
		uint32_t low(0), range(0xFFFFU), hist(0);
		for (uint i=0; i<2; i++) low = (low<<8) | is.get();
		while (is and out.size()<size) {

			if (range < 0x100) {
				range <<= 8;
				low   <<= 8;
				low += is.get();
			}

			uint32_t r;
			bool s = false;
			if (hfreqsz[hist]<(1<<8)) {
				r = (hfreq[hist]*range)/hfreqsz[hist];
			} else {
				
				if ((hfreqsz[hist] & 0xFF) == 0) {
					hfreq2[hist] = hfreq[hist];
					hfreqsz[hist] = 256 + 2;
					hfreq[hist] = 1;
				}
				
				r = (hfreq2[hist]*range)>>8;
			}			

			range -= r;
			if (low>=range) {
				low -= range;
				range = r;
				s = true;
				hfreq[hist]++;
			}
			hfreqsz[hist]++;

			out.push_back(s);	
			
			hist = ((hist<<1)+s)&((1<<level)-1);
		}
		{ int c=0,l=0; while (is and ((c=is.get())!=0xEF or l!=0xBE)) l=c;}
		return out;		
	}
	
	static std::vector<uint8_t> decodeBin(const std::string &in) { std::istringstream is(in); return decodeBin(is); } 
};

}
}
