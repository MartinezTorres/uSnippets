#pragma once

#include <opencv/cv.h>
#include <GL/gl.h>
#include <memory>

namespace uSnippets {
class GLTEX {

protected:
	class GCTexName {
		static std::vector<GLuint> &V() { static std::vector<GLuint> v; return v; }
	
	public:	
		GLuint i;
		~GCTexName() { V().push_back(i); };
		static void clean() { if (not V().empty()) { glDeleteTextures( V().size(), &V()[0] ); V().clear(); } }
	};
	
	std::shared_ptr<GCTexName> t;	
	cv::Mat img; 
	cv::Size size;
	bool genMipmaps;
	
	void init() {
		GCTexName::clean();
		if (t) return;

		t = std::make_shared<GCTexName>();
		glGenTextures( 1, &t->i );
		glBindTexture(GL_TEXTURE_2D, t->i);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, genMipmaps?GL_TRUE:GL_FALSE);
		
		if (genMipmaps)	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width, size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );

		if (img.empty()) return;
		
		if (img.channels()==1)
			cv::cvtColor(img, img, CV_GRAY2RGBA);
		if (img.channels()==3)
			cv::cvtColor(img, img, CV_BGR2RGBA);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.cols, img.rows, GL_RGBA, GL_UNSIGNED_BYTE, img.data );
	}
	
public:

	const cv::Size &getSize() const { return size; }

	GLTEX(cv::Size size = cv::Size(16,16), bool genMipmaps = false) : size(size), genMipmaps(genMipmaps) {}

	GLTEX(cv::Mat img, bool genMipmaps = true) : img(img), size(img.size()), genMipmaps(genMipmaps) {}
	
	~GLTEX() {}
	
	GLuint name() { init(); return t->i; }
		
	void operator()() { init(); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, t->i); }
};
}
