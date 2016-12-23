#pragma once

#include <uSnippets/range.hpp>
#include <opencv2/opencv.hpp>

namespace uSnippets {
namespace Codecs {
namespace Disparity {
	
static inline std::string code(const std::vector<cv::Mat1s> &in) {
	
	const uint N = in.size();
	assert (N<256);
	std::string out;
	out.push_back(N);
	if (not N) return out;
	
	const uint rows = in.front().rows;
	const uint cols = in.front().cols;
	
	out.push_back(rows/256);out.push_back(rows%256);
	out.push_back(cols/256);out.push_back(cols%256);
	
	std::vector<uint16_t> data(rows*cols*N); uint16_t *d = &data[0];
	for (uint i=0; i<rows; i++) {
		const int16_t *I[256]; 
		for (uint n=0; n<N; n++) I[n] = &in[n](i,0); 
		for (uint j=0; j<cols; j+=2) {
			for (uint n=0; n<N; n++)
				*d++ = I[n][j];
			for (int n=N-1; n>=0; n--)
				*d++ = I[n][j+1];
		}		
	}
	
	std::vector<uint8_t> zeros, ones, signs;
	std::vector<uint16_t> fulldata;
	
	int lastsign=1;
	uint16_t last=0;	
	for (auto &i : data) {
		int c = i-last;
		last = i;
		
		zeros.push_back(!!c);
		if (c) {
			ones.push_back(c==lastsign);
			
			if (c!=lastsign) {
				signs.push_back(c>0);
				fulldata.push_back(abs(c));
			}
			if (c<0) lastsign = 1;
			if (c>0) lastsign = -1;
		}
	}
	out += uSnippets::Codecs::Range::encodeBin(zeros,8);
	out += uSnippets::Codecs::Range::encodeBin(ones,8);
	out += uSnippets::Codecs::Range::encodeBin(signs,8);
	out += uSnippets::Codecs::Range::encode(fulldata,2048,3);
	return out;
}

static inline std::vector<cv::Mat1s> decode(const std::string &in) {

	std::vector<cv::Mat1s> out;
	
	std::istringstream is(in);
	
	const uint N = is.get();
	assert(N<256);
	if (not N) return out;
	
	uint rows, cols;
	rows = is.get(); rows = rows*256+is.get();
	cols = is.get(); cols = cols*256+is.get();
	
	std::vector<uint8_t>  zeros    = uSnippets::Codecs::Range::decodeBin(is);
	assert(zeros.size() == rows*cols*N);
	std::vector<uint8_t>  ones     = uSnippets::Codecs::Range::decodeBin(is);
	std::vector<uint8_t>  signs    = uSnippets::Codecs::Range::decodeBin(is);
	std::vector<uint16_t> fulldata = uSnippets::Codecs::Range::decode(is,2048);
	uint8_t  *z = &zeros[0], *o = &ones[0], *s = &signs[0];
	uint16_t *f = &fulldata[0];
	
	std::vector<uint16_t> data(rows*cols*N);
	
	int lastsign=1;
	uint16_t last=0;	
	for (auto &i : data) {
		
		int c = 0;
		
		if (*z++) {
			
			if (*o++) {
				
				c = lastsign;
			} else {
				c = *f++ * (*s++?1:-1);
			}
			if (c<0) lastsign = 1;
			if (c>0) lastsign = -1;
		}
		
		
		last = i = c + last;
	}

	for (uint n=0; n<N; n++) out.push_back(cv::Mat1s(rows,cols));

	uint16_t *d = &data[0];
	for (uint i=0; i<rows; i++) {
		int16_t *I[256]; 
		for (uint n=0; n<N; n++) I[n] = &out[n](i,0); 
		for (uint j=0; j<cols; j+=2) {
			for (uint n=0; n<N; n++)
				I[n][j] = *d++;
			for (int n=N-1; n>=0; n--)
				I[n][j+1] = *d++;
		}		
	}

	return out;
}

}
}
}
