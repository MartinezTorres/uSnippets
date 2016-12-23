#pragma once 
#include <lz4.h>
#include <lz4hc.h>
#include <string>

namespace uSnippets {
namespace Codecs {
namespace LZ4 {
static inline size_t encode( const std::string &in, std::string &out, uint level) {
	/*
	uint64_t sz = in.size(); std::memcpy((char *)out.data(), &sz, 8);

	out.resize(LZ4_compressBound(8+in.size()));
	if (level>5)
		out.resize(8+LZ4_compress_HC(in.data(), (char *)out.data()+8, in.size(), out.size(), level-2));
	else
		out.resize(8+LZ4_compress_fast(in.data(), (char *)out.data()+8, in.size(), out.size(), 6-level));
	*/return out.size();
}

static inline size_t decode( const std::string &in, std::string &out) {

	/*uint64_t sz = 0; std::memcpy(&sz, in.data(), 8);
	out.resize(sz);
	int ret = LZ4_decompress_fast(in.data()+8, (char *)out.data(), sz);
	if (ret<0) throw std::runtime_error("error decoding lz4 packet");
	*/return out.size();
}

static inline std::string encode( const std::string &in, uint level=5 ) { std::string out; encode(in, out, level); return out; }

static inline std::string decode( const std::string &in ) { std::string out; decode(in, out);	return out; }	

}
}
}
