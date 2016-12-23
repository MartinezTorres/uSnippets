////////////////////////////////////////////////////////////////////////
// OpenNI very basic encapsulation
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3 

#pragma once

#include <uSnippets/disparityCodec.hpp>

#include <opencv/cv.h>
#include <OpenNI2/OpenNI.h>
#include <functional>
#include <iostream>
#include <list>
#include <uSnippets/serializer.hpp>
#include <mutex>
#include <condition_variable>
#include <queue>


using namespace std;

namespace uSnippets {
	
class OpenNI {

	std::mutex mtx;
	std::condition_variable cv;
	
	template<typename T>
	struct Listener : public openni::VideoStream::NewFrameListener { 
		
		std::mutex &mtx;
		std::condition_variable &cv;
		std::queue<std::pair<std::chrono::microseconds,T>> &imgs;
		
		std::function<void(T &)> cb;
		
		Listener(std::mutex &mtx, std::condition_variable &cv, std::queue<std::pair<std::chrono::microseconds,T>> &imgs) : mtx(mtx), cv(cv), imgs(imgs) {}
		
		virtual void onNewFrame(openni::VideoStream &stream) {

			std::chrono::microseconds timeStamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
			
			std::unique_lock<std::mutex> lock(mtx);
			
			openni::VideoFrameRef frame;
			stream.readFrame(&frame);
			
			T img(frame.getHeight(), frame.getWidth());
			for (int y = 0; y < img.rows; y++)
				memcpy(img.ptr(y),((uint8_t *)frame.getData()) + y*frame.getStrideInBytes(), img.cols*img.elemSize());
			

			imgs.push({timeStamp, img.clone()});
			if (imgs.size()>100) imgs.pop();
			
			if (cb) cb(img);
			
			lock.unlock();
			cv.notify_all();
		}
	};
	
	std::queue<std::pair<std::chrono::microseconds, cv::Mat3b>> colorQueue;
	Listener<cv::Mat3b> colorListener;
	std::queue<std::pair<std::chrono::microseconds, cv::Mat1s>> depthQueue;
	Listener<cv::Mat1s> depthListener;
	openni::Device device;
	openni::VideoStream depth, color;
public:
	
	OpenNI() : colorListener(mtx,cv,colorQueue), depthListener(mtx,cv,depthQueue) {
		openni::OpenNI::initialize();

		if (device.open(openni::ANY_DEVICE)) return;
//		device.setDepthColorSyncEnabled(true);
//		device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_OFF );		
//		device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR );
	};
	
	~OpenNI() {}
	
	// RGB functions
	void startColor() {
		if (color.isValid()) return;
		color.create(device, openni::SENSOR_COLOR);
		if (1) {
			auto &svm = color.getSensorInfo().getSupportedVideoModes();
			for (int i=0; i<svm.getSize(); i++) std::cerr << "Color supports: " << svm[i].getFps() << " " << svm[i].getPixelFormat() << " " << svm[i].getResolutionX() << " " << svm[i].getResolutionY() << std::endl;
		}
		color.addNewFrameListener(&colorListener);
		color.start();
	};
	
	void colorCB(std::function<void(cv::Mat3b)> cb) { startColor(); colorListener.cb = cb; }
	
	cv::Mat3b getColor(std::chrono::microseconds &timeStamp) {
		startColor();
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this]{return not colorQueue.empty();});
		timeStamp = colorQueue.front().first;
		cv::Mat3b ret = colorQueue.front().second;
		colorQueue.pop();
		return ret;
	}
	
	uint getColorQueueSize() { return colorQueue.size(); }


	// Disparity functions
	void startDepth() {
		if (depth.isValid()) return;
		depth.create(device, openni::SENSOR_DEPTH);
		if (0) {
			auto &svm = depth.getSensorInfo().getSupportedVideoModes();
			for (int i=0; i<svm.getSize(); i++) std::cerr << "Depth supports: " << svm[i].getFps() << " " << svm[i].getPixelFormat() << " " << svm[i].getResolutionX() << " " << svm[i].getResolutionY() << std::endl;
		}
		openni::VideoMode vm; vm.setPixelFormat(openni::PIXEL_FORMAT_SHIFT_9_2); vm.setFps(30); vm.setResolution(640,480);
		depth.setVideoMode(vm);
		depth.addNewFrameListener(&depthListener);
		depth.start();
	}
	
	void depthCB(std::function<void(cv::Mat1s)> cb) { startDepth(); depthListener.cb = cb; }
	
	cv::Mat1s getDepth(std::chrono::microseconds &timeStamp) {
		startDepth();
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this]{return not depthQueue.empty();});
		timeStamp = depthQueue.front().first;
		cv::Mat1s ret = depthQueue.front().second;
		depthQueue.pop();
		return ret;
	}
	
	uint getDepthQueueSize() { return depthQueue.size(); }
	

	operator bool() const { return device.isValid(); }
	
	static std::string depthPackVector(const std::vector<cv::Mat1s> &in, uint = 5) __attribute__ ((deprecated)) { return Codecs::Disparity::code(in); }
	static std::vector<cv::Mat1s> depthUnpackVector(const std::string &in, int=0, int=0) __attribute__ ((deprecated)) { return Codecs::Disparity::decode(in); }

};
}
