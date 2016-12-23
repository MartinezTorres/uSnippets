#pragma once 
#include <zlib.h>
#include <string>

namespace uSnippets {
namespace Codecs {
namespace GZip {
static inline int code( const std::string &in, std::string &out, int level=6) {
	
	
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	deflateInit(&strm, level);
	
	strm.avail_in = in.size();
	strm.next_in = (Bytef*)&in[0];

	int done = 0;
	do {
		out.resize( done + 1024*10 );
		
		strm.avail_out = out.size() - done;
		strm.next_out = (Bytef*)&out[done];

		deflate( &strm, Z_FINISH);
		
		done += 1024*10 - strm.avail_out;
	} while( strm.avail_out == 0 );
	out.resize( done );
	
	deflateEnd(&strm);

	return out.size();
}

static inline int decode( const std::string &in, std::string &out) {

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	inflateInit(&strm);
	
	strm.avail_in = in.size();
	strm.next_in = (Bytef*)&in[0];

	int done = 0;
	do {
		out.resize( done + 1024*100 );

		strm.avail_out = out.size() - done;
		strm.next_out = (Bytef*)&out[done];

		inflate( &strm, Z_NO_FLUSH);
	
		done += 1024*100 - strm.avail_out;
	} while( strm.avail_out == 0 );
	out.resize( done );
	
	inflateEnd(&strm);

	return out.size();
}

static inline std::string code( const std::string &in, int level=6) { std::string out; code (in, out, level); return out; }

static inline std::string decode( const std::string &in ) { std::string out; decode (in, out);	return out; }	

}
}
}
