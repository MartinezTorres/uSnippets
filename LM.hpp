////////////////////////////////////////////////////////////////////////
// minimal Levenberg-Marquardt code
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3
//
// GCC-FLAGS: -Ofast -std=c++0x `pkg-config opencv --cflags` `pkg-config opencv --libs`

#pragma once
#include <opencv/cv.h>

struct LM {
	
	double epsilon;
	int maxIter;
	double delta;
	
	LM(double epsilon=1e-9, int maxIter=10000, double delta=1e-12) : epsilon(epsilon), maxIter(maxIter), delta(delta) {}
	
	bool operator()(cv::Mat1d &X, std::function<cv::Mat1d(const cv::Mat1d&)> &&f) {

		return this->operator()(
			X, 
			[&](const cv::Mat1d&X)->cv::Mat1d {
				return f(X);
			}, 
			[&](const cv::Mat1d&X)->cv::Mat1d {
				
				cv::Mat1d Y = f(X);
				cv::Mat1d J(Y.rows, X.rows, 0.);
				cv::Mat1d XP = X.clone();
				for(int i=0; i<X.rows; i++) {
					for (double k=1e-24; k<=1e-6; k*=1e6) {
						XP(i) = X(i) + k;
						cv::Mat1d YP = f(XP);
						J.col(i) = (YP-Y)*(1./k);
						if (cv::norm(YP-Y)/(cv::norm(YP)+1e-100) > delta) break;
					}
					XP(i) = X(i);
				}
				return J;
			}
		);
	}

	bool operator()(cv::Mat1d &X, std::function<cv::Mat1d(const cv::Mat1d&)> &&f, std::function<cv::Mat1d(const cv::Mat1d&)> &&j) {

		cv::Mat1d Y = f(X);
		cv::Mat1d J = j(X);

		double l = 1e-6;
		
		for (int i=0; i<maxIter; i++) {

			cv::Mat1d hlm, A = J.t()*J;
			cv::solve( A + l*cv::Mat1d::eye(X.rows,X.rows) , -J.t()*Y, hlm, cv::DECOMP_QR);
			
			cv::Mat1d XP = X+hlm;
			cv::Mat1d YP = f(XP);
			if (cv::norm(YP)<epsilon) {
				X = XP;
				return true;
			}
			
			if (cv::norm(YP)>=cv::norm(Y)) {
				
				if (l>1e6) return false;
				l = l*16; // be safer
			} else {
				X = XP;	Y = YP;
				J = j(X);
				l = l*.75; //be bolder
			}
		}
		return false;
	}
};
