////////////////////////////////////////////////////////////////////////
// minimal SpaceNavigator driver
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// If you need root permissions do:
//    echo -e 'SUBSYSTEM=="usb", ATTR{idVendor}=="046d", ATTR{idProduct}=="c626", MODE="0666"' | sudo tee /etc/udev/rules.d/51-SpaceNavigator.rules
//
// license: LGPLv3

#pragma once

#include <opencv/cv.h>

#include <thread>
#include <mutex>
#include <fstream>

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

namespace uSnippets {
class SpaceNavigator {

	int dev;
	
	cv::Mat1f translation;
	cv::Mat1f rotation;

	bool leftButton, rightButton;
	double tSpeed;
	double rSpeed;

	bool good;
	
	std::mutex mtx;
	std::thread t;
	typedef std::lock_guard<std::mutex> Lock;


	void run() {

		sleep(1);
		struct pollfd pfd;
		pfd.fd = dev;
		pfd.events = POLLRDNORM;

		struct input_event ev;
		int sz = 0;
		while (good) {
			
			int retval = poll(&pfd, 1, 100);
			if (retval == -1) break;
			if (not retval) continue;

			{
				int r = read(dev, &((char *)&ev)[sz], sizeof(ev)-sz);
				if (r<0 and errno!=EAGAIN) break;
				if (r>int(sizeof(ev))) break;
				sz+=std::max(r,0);
			}

			if (sz!=sizeof(ev)) continue;
			sz=0;



			Lock l(mtx);
			if (abs(ev.value) < 400) { //Sometimes we get phony values larger than 400 
				if        (ev.code==0)   { translation(0,0) -= ev.value*tSpeed;
				} else if (ev.code==1)   { translation(0,2) -= ev.value*tSpeed;
				} else if (ev.code==2)   { translation(0,1) -= ev.value*tSpeed;
				} else if (ev.code<6)    {

						float c = cos(ev.value*rSpeed);
						float s = sin(ev.value*rSpeed);

						cv::Mat1f diffM = cv::Mat1f::eye(3,3);
						if (ev.code==4) diffM = (cv::Mat1f(3,3) << c, s, 0, -s, c, 0, 0, 0, 1); // roll
						if (ev.code==3) diffM = (cv::Mat1f(3,3) << 1, 0, 0, 0, c, s, 0, -s, c); // pitch
						if (ev.code==5) diffM = (cv::Mat1f(3,3) << c, 0, s, 0, 1, 0, -s, 0, c); // yaw

						cv::Mat rotM = cv::Mat1f::eye(3,3);
						cv::Rodrigues( rotation, rotM );
						rotM *= diffM;
						cv::Rodrigues( rotM, rotation );
					
				} else if (ev.code==256) { leftButton = ev.value;
				} else if (ev.code==257) { rightButton = ev.value;
				}
			}
		}
	}
	
public:
	
	SpaceNavigator() : 
		dev(open("/dev/input/by-id/usb-3Dconnexion_SpaceNavigator-event-if00", O_RDONLY)),
		translation(cv::Mat1f::zeros(1,3)),
		rotation(cv::Mat1f::zeros(1,3)),
		leftButton(false),
		rightButton(false),
		tSpeed(2./(350/0.016)), // We get aprox a message every 16msec with values ranging from -350 to +350
		rSpeed(2./(350/0.016)),
		good(true), 
		t([&, this](){this->run();}) {
	}

	~SpaceNavigator() {
		
		try {
			good = false;
			Lock l(mtx);
			close(dev);
		} catch (std::exception& e) {
			if (e.what() != std::string("basic_ios::clear"))
				std::clog << "Exception: " << e.what() << std::endl;
		}
		t.join();
	}
	
	void reset() {

		Lock l(mtx);
		translation = cv::Mat1f::zeros(1,3);
		rotation    = cv::Mat1f::zeros(1,3);
	}
	
	void get(cv::Mat1f &translation, cv::Mat1f &rotation) {
		
		Lock l(mtx);
		translation = this->translation.clone();
		rotation    = this->rotation.clone();
	}
	
	cv::Mat1f getGL(bool rotEnable = true, bool tranEnable = true) {

		Lock l(mtx);
		cv::Mat1f r = cv::Mat1f::eye(4,4);
		
		if (rotEnable) {
			cv::Mat1f rot = r(cv::Rect(0,0,3,3));
			cv::Rodrigues( rotation, rot );
		}
		
		if (tranEnable) 
			r(cv::Rect(0,3,3,1)) = translation;
		
		return r;
	}
		 
	
	void getButtons( bool &left, bool &right, bool clear=true ) {
		
		Lock l(mtx);
		left = leftButton;
		right = rightButton;
		if (clear) 
			leftButton = rightButton = false;
	}
}; 
}
// Thanks for R.T.F.C.

