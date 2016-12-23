////////////////////////////////////////////////////////////////////////
// opencv uGui2D backend
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once

#include <uSnippets/gui2d.hpp>
#include <opencv/highgui.h>

namespace uSnippets {
struct Gui2Dcv : public Gui2D {

	const std::string name;
	
	Gui2Dcv(const std::string &name = "gui", bool fullscreen=false) : name(name), fullscreen(fullscreen), good(true), t([&](){run();}) {}
	
	~Gui2Dcv() { good = false; t.join(); }
	
protected:

	bool fullscreen;
	bool good;
	std::thread t;

	void run() {
		
		cv::namedWindow( name, CV_WINDOW_NORMAL );
		if (fullscreen) {
			cv::setWindowProperty(name, CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
			cv::setWindowProperty(name, CV_WND_PROP_ASPECTRATIO, CV_WINDOW_KEEPRATIO);
		}

		cv::setMouseCallback( name, [](int event, int x, int y, int flags, void *data) {

			Gui2Dcv &gui2D = *(Gui2Dcv *)data;
			Lock l(gui2D.mtx);

			gui2D.updated();
			
			gui2D.currentPosition = cv::Point(x,y);
			gui2D.pressed = not not (flags & CV_EVENT_FLAG_LBUTTON);
			if (event == cv::EVENT_LBUTTONDOWN) gui2D.lastPressed = cv::Point(x,y);
			
			if (not gui2D.mcb) return;
			if (event == cv::EVENT_LBUTTONDOWN  ) gui2D.mcb(PRESSED);
			if (event == cv::EVENT_LBUTTONUP    ) gui2D.mcb(RELEASED);
			if (event == cv::EVENT_LBUTTONDBLCLK) gui2D.mcb(DBLCLK);
			if (event == cv::EVENT_MOUSEMOVE    ) gui2D.mcb(MOVED);
		
		}, this);

		
		while (good) {
			
			int k = cv::waitKey(33);

			Lock l(mtx);
			if (not showImg.empty()) cv::imshow(name, showImg);
			showImg = cv::Mat3b();
			
			if (k==-1) continue;
			
			std::cerr << (k&255) << std::endl;
					
			keys.push(k & 0xFF);
			if (kcb[k & 0xFF]) kcb[k & 0xFF]();

			updated();
		}	
	};
};
}
