////////////////////////////////////////////////////////////////////////
// turbo JPEG wrapper
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once

#include <turbojpeg.h>
#include <opencv/cv.h>
namespace uSnippets {
namespace TJ {
template<class T>
std::string code(const cv::Mat_<T> &img, int quality) {
	
	tjhandle jpegHandle = tjInitCompress();
	uint8_t *jpegBuff = nullptr;
	long unsigned int jpegSz = 0;
	quality = std::max(0,std::min(100,quality));

	assert(img.isContinuous());
	assert(img.channels()==1 or img.channels()==3);
	
	if (img.channels()==1)
		tjCompress2(jpegHandle, img.data, img.cols, 0, img.rows, TJPF_GRAY, &jpegBuff, &jpegSz, TJ_GRAYSCALE, quality, TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT);
	
	if (img.channels()==3)
		tjCompress2(jpegHandle, img.data, img.cols, 0, img.rows, TJPF_BGR, &jpegBuff, &jpegSz, TJSAMP_420, quality, TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT);
			
	std::string ret((char *)jpegBuff, jpegSz);		
	
	tjFree(jpegBuff);
	tjDestroy(jpegHandle);
	
	return ret;
}

template<class T>
cv::Mat_<T> decode(const std::string &s) {
	
	tjhandle jpegHandle = tjInitDecompress();

	int width, height, jpegSubsamp;
	tjDecompressHeader2(jpegHandle, (uint8_t*)&s[0], s.size(), &width, &height, &jpegSubsamp);

	cv::Mat_<T> img(height, width);
	tjDecompress2(jpegHandle, (uint8_t*)&s[0], s.size(), img.data, img.cols, 0, img.rows, img.elemSize()==1?TJPF_GRAY:TJPF_BGR, TJFLAG_FASTUPSAMPLE | TJFLAG_FASTDCT);

	tjDestroy(jpegHandle);
	return img;
}

//constexpr auto &decode1b = decode<uint8_t>;
//constexpr auto &decode3b = decode<cv::Vec3b>;
inline cv::Mat1b decode1b(const std::string &s) { return decode<uint8_t>(s); }
inline cv::Mat3b decode3b(const std::string &s) { return decode<cv::Vec3b>(s); }
}
}
// Thanks for R.T.F.C.
