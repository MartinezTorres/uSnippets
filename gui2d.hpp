////////////////////////////////////////////////////////////////////////
// basic 2D GUI
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include <queue> 
#include <opencv/cv.h>

namespace uSnippets {
struct Gui2D {
	
	enum MouseAction { MOVED, PRESSED, RELEASED, DBLCLK };
	typedef std::function<void(MouseAction)> MCB;
	typedef std::function<void()> KCB;

	virtual bool isPressed() { Lock l(mtx); return pressed; }
	virtual cv::Point getLastPressed() { Lock l(mtx); return lastPressed; }
	virtual cv::Point getPos() { Lock l(mtx); return currentPosition; }
	virtual cv::Rect getSelection() { 
		
		Lock l(mtx);
		if (not pressed) return cv::Rect();
		cv::Point p1 = lastPressed, p2 = currentPosition;
		return cv::Rect(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x-p2.x)+1, std::abs(p1.y-p2.y)+1);
	}	
	virtual void mouseCB(MCB cb = {}) { Lock l(mtx); mcb = cb; }
	
	virtual char getKey() { //ASCII if possible
		
		Lock l(mtx); 
		if (keys.empty()) return 0; 
		char k = keys.front(); 
		keys.pop();
		return k;
	}

	virtual void keyCB(uint8_t key, KCB cb = {}) { Lock l(mtx); kcb[key%256] = cb; } 
	
	virtual void img( const cv::Mat3b &img )  { Lock l(mtx); showImg = img; }
	
	virtual void wait_for( const std::chrono::milliseconds& rel_time ) { std::unique_lock<std::mutex> l(updatemtx); cv.wait_for(l, rel_time); }

	virtual void wait() { std::unique_lock<std::mutex> l(updatemtx); cv.wait(l); }

	std::unique_lock<std::recursive_mutex> getLock() { return Lock(mtx); }

	void eventMouseMoved(cv::Point2f pos) {
		
		Lock l(mtx);
		currentPosition = cv::Point2d(showImg.cols*pos.x, showImg.rows*pos.y);
		if (mcb) mcb(MOVED);
		updated();
	};

	void eventMousePressed(cv::Point2f pos) {
		
		Lock l(mtx);
		pressed = true;
		currentPosition = cv::Point2d(showImg.cols*pos.x, showImg.rows*pos.y);
		lastPressed = currentPosition;
		if (mcb) mcb(PRESSED);
		updated();
	};

	void eventMouseReleased(cv::Point2f pos) {

		Lock l(mtx);
		currentPosition = cv::Point2d(showImg.cols*pos.x, showImg.rows*pos.y);
		pressed = false;
		if (mcb) mcb(RELEASED);
		updated();
	};

	void eventDblclk(cv::Point2f pos) {
	
		Lock l(mtx);
		currentPosition = cv::Point2d(showImg.cols*pos.x, showImg.rows*pos.y);
		if (mcb) mcb(DBLCLK);
		updated();
	};

	
	virtual ~Gui2D() {};
	
protected:
	
	void updated() { std::unique_lock<std::mutex> l(updatemtx); cv.notify_all(); }
	
	typedef std::unique_lock<std::recursive_mutex> Lock;
	std::recursive_mutex mtx;
	std::condition_variable cv;
	std::mutex updatemtx;
	
	bool pressed;
	cv::Point lastPressed, currentPosition;
	MCB mcb;
	
	std::queue<char> keys;
	KCB kcb[256];
	
	cv::Mat3b showImg;
};
}
