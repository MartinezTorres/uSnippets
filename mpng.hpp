////////////////////////////////////////////////////////////////////////
// PNG coder with metadata support
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#include <uSnippets/object.hpp>
#include <fstream>
#include <opencv/cv.h>
#include <png.h>

namespace uSnippets {
class MPNG : public cv::Mat3b {
	
	static void user_error_fn( png_structp, png_const_charp) { exit(-1); };
	
	static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) { ((std::istream *)png_get_io_ptr(png_ptr))->read((char *)data, length); };
	static void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) { ((std::ostream *)png_get_io_ptr(png_ptr))->write((char *)data, length); };
	static void user_flush_data(png_structp png_ptr) { ((std::ostream *)png_get_io_ptr(png_ptr))->flush(); };
public:

	// Ugliness demands a comment. This is a C++03 version of constructor inheritance, the C++11 version is still not very well supported.
	template<class T1>
	MPNG(T1 t1) : cv::Mat3b(t1) {}
	template<class T1, class T2> 
	MPNG(T1 t1, T2 t2) : cv::Mat3b(t1, t2) {}
	template<class T1, class T2, class T3> 
	MPNG(T1 t1, T2 t2, T3 t3) : cv::Mat3b(t1, t2, t3) {}
	
	
	MPNG() {};
	MPNG(std::string filename) { read(filename); }
	MPNG(const char *filename) { read(filename); }
	MPNG &operator=(cv::Mat3b img) { MPNG png(img); png.metadata = metadata; png.usedFilename = usedFilename; *this = png; return *this; }
	
	~MPNG() {};
	
	std::vector<std::pair<std::string, object> > metadata;
	std::string usedFilename;

	void read(std::string filename = "") {
		
		if (filename=="") filename = usedFilename;
		usedFilename = filename;
		std::ifstream in(filename.c_str(), std::ifstream::binary);
		in >> *this;
	}

	void write(std::string filename = "") {

		if (filename=="") filename = usedFilename;
		usedFilename = filename;
		std::ofstream out(filename.c_str(), std::ofstream::binary | std::ofstream::trunc);
		out << *this;
	}

	friend std::istream &operator>>(std::istream &in, MPNG &img) {
		
		png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, user_error_fn, NULL); assert (png_ptr);
		png_infop info_ptr = png_create_info_struct(png_ptr); assert (info_ptr);

		png_set_read_fn(png_ptr, (void *)&in, user_read_data);

		png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR, NULL);

		png_text *text_ptr;
		img.metadata.resize( png_get_text(png_ptr, info_ptr, &text_ptr, NULL) ); 
		for (size_t i=0; i<img.metadata.size(); i++)
			img.metadata[i] = std::make_pair( std::string(text_ptr[i].key), object(std::string( text_ptr[i].text, text_ptr[i].text_length)));

		png_uint_32 width, height; int bit_depth, color_type;
		png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
		assert (color_type == PNG_COLOR_TYPE_RGB or color_type == PNG_COLOR_TYPE_GRAY);
		
		cv::Mat tmp(width, height, color_type == PNG_COLOR_TYPE_RGB ? CV_8UC3 : CV_8UC1);
		png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
		for (int i = 0; i < tmp.rows; i++) 
			memcpy((uint8_t *)tmp.ptr(i), row_pointers[i], tmp.cols*tmp.elemSize());

		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	
		if (color_type == PNG_COLOR_TYPE_GRAY) cvtColor(tmp, tmp, CV_GRAY2BGR);
		img = cv::Mat3b(tmp);
		
		return in;
	}


	friend std::ostream &operator<<(std::ostream &out, const MPNG &img) {
		
		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, user_error_fn, NULL); assert (png_ptr);
		png_infop info_ptr = png_create_info_struct(png_ptr); assert (info_ptr);

		png_set_write_fn(png_ptr, (void *)&out, user_write_data, user_flush_data);
		
		png_set_IHDR(png_ptr, info_ptr, img.cols, img.rows, 8,
				PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
				PNG_FILTER_TYPE_BASE);
				
		std::vector<png_text> text_ptr(img.metadata.size());
		memset(&text_ptr[0], 0, sizeof(png_text) * img.metadata.size() );
		for (size_t i=0; i<img.metadata.size(); i++) {
			
			text_ptr[i].compression = PNG_TEXT_COMPRESSION_zTXt;
			text_ptr[i].key  = (char *)img.metadata[i].first.c_str();
			text_ptr[i].text = (char *)img.metadata[i].second.c_str();
			text_ptr[i].text_length = img.metadata[i].second.size();
		}
		png_set_text(png_ptr, info_ptr, &text_ptr[0], img.metadata.size());

		std::vector<uint8_t *> row_pointers(img.rows);
		for (int i=0; i<img.rows; ++i)
			row_pointers[i] = (uint8_t *)img.ptr(i);
		png_set_rows(png_ptr, info_ptr, &row_pointers[0]);
		
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		return out;
	}
	
	object &operator[](const std::string s) {
		
		std::vector<std::pair<std::string, object> >::iterator it=metadata.begin();
		while (it!=metadata.end() and it->first!=s) it++;
		if (it!=metadata.end()) return it->second;
		metadata.push_back(std::make_pair(s,object()));
		return metadata.back().second;
	}
};
}
