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

struct LBFGS {

    int m; // Hist size
	double epsilon;
	int max_iterations;
	
	LBFGS(int m = 6, double epsilon=1e-9, int max_iterations=10000) : m(m), epsilon(epsilon), max_iterations(max_iterations) {}

	bool operator()(cv::Mat1d &X, std::function<double(const cv::Mat1d&)> &&f) {
		return this->operator()(
			X, 
			[&](const cv::Mat1d&X)->double {
				return f(X);
			}, 
			[&](const cv::Mat1d&X)->cv::Mat1d{
				cv::Mat1d G(X.rows,1);
				cv::Mat1d XP = X.clone();
				double y = f(X);
				for (int i=0; i<X.rows; i++) {
					for (double k=1e-12; k<=1e+6; k*=1e1) {
						
						XP(i) = X(i) + (X(i) ? X(i)*k: k);
						double yp = f(XP);
						G(i) = (yp-y)/(X(i) ? X(i)*k: k);
						if (std::abs((yp-y)/(yp+1e-100)) > 1e-9) break;
					}
					XP(i) = X(i);
				}
				return G;
			}
		);
	}

	bool operator()(cv::Mat1d &X, std::function<double(const cv::Mat1d&)> &&f, std::function<cv::Mat1d(const cv::Mat1d&)> &&g) {
		
		struct Iteration {
			double alpha, ys, yy;
			cv::Mat1d S;
			cv::Mat1d Y;
		};
		std::deque<Iteration> hist;

		// Evaluate the function value and its gradient.
		double fx = f(X);
		cv::Mat1d G = g(X);

		// Compute the direction; we assume the initial hessian matrix H_0 as the identity matrix.
		cv::Mat1d D = -G / cv::norm(G);
		
		if (cv::norm(G)/std::max(1.0, cv::norm(X)) < epsilon) return true;
		
		for (int k=0; k<max_iterations; k++) {
			
			/* Store the current position and gradient vectors. */
			cv::Mat1d XP = X.clone();
			cv::Mat1d GP = G.clone();
			
			// Search for an optimal step. (Backtrack)
			{
				// Compute the initial gradient in the search direction.
				double dginit = G.dot(D);
				if (0<dginit) { std::cerr << "not descent direction to improve" << std::endl; return false; } // Ensure descent direction

				double fxp=fx;
				double step = 1; 
				while (step>1e-20) {
					
					fxp = f(XP+D*step);
					if (fxp < fx + step * dginit * 1e-4) break; // ARMIJO
					step *= .5;
				}
				
				if (fxp>=fx) { std::cerr << "failed to improve" << std::endl; return false; }
				X = XP+D*step;
				fx = fxp;
			}
        
			G = g(X);
			if (cv::norm(G)/std::max(1.0, cv::norm(X)) < epsilon) return true; //SUCCESS
        
            //Update vectors s and y:
            //    s_{k+1} = x_{k+1} - x_{k} = \step * d_{k}.
            //    y_{k+1} = g_{k+1} - g_{k}.
			Iteration it;
			it.S = X - XP;
			it.Y = G - GP;
        
			// Compute scalars ys and yy:
			// ys = y^t \cdot s = 1 / \rho.
			// yy = y^t \cdot y.
			// Notice that yy is used for scaling the hessian matrix H_0 (Cholesky factor).
			it.ys = it.Y.dot(it.S);
			it.yy = it.Y.dot(it.Y);
			
			if (std::abs(it.ys) < 1e-20 or std::abs(it.yy) < 1e-20) { std::cerr << "almost divide by zero" << std::endl; return false; };
//			std::cerr << std::scientific << it.yy << " " << it.ys << std::endl;

            
			// Recursive formula to compute dir = -(H \cdot g).
			// This is described in page 779 of: Jorge Nocedal.
			// Updating Quasi-Newton Matrices with Limited Storage.
			D = -G;
			for (int i=hist.size()-1; i>=0; i--) {
				// \alpha_{j} = \rho_{j} s^{t}_{j} \cdot q_{k+1}.
				hist[i].alpha = hist[i].S.dot(D);
				hist[i].alpha /= hist[i].ys;
				// q_{i} = q_{i+1} - \alpha_{i} y_{i}.
				D -= hist[i].Y*hist[i].alpha;
			}
			D *= it.ys/it.yy;
			
			for (int i=0; i<int(hist.size()); i++) {
				
				// \beta_{j} = \rho_{j} y^t_{j} \cdot \gamma_{i}. */
				double beta = hist[i].Y.dot(D)/hist[i].ys;
				// \gamma_{i+1} = \gamma_{i} + (\alpha_{j} - \beta_{j}) s_{j}. */
				D += hist[i].S*(hist[i].alpha-beta);
			}
			
			hist.push_back(it);
			if (int(hist.size())>m) hist.pop_front();
		}
		return false;
	}
};


struct GD {

	double alpha;
	double epsilon;
	int max_iterations;
	
	GD(double alpha=0.000001, double epsilon=1e-3, int max_iterations=10000) : alpha(alpha), epsilon(epsilon), max_iterations(max_iterations) {}

	bool operator()(cv::Mat1d &X, std::function<double(const cv::Mat1d&)> &&f) {
		return this->operator()(
			X, 
			[&](const cv::Mat1d&X)->double {
				return f(X);
			}, 
			[&](const cv::Mat1d&X)->cv::Mat1d{
				cv::Mat1d G(X.rows,1);
				cv::Mat1d XP = X.clone();
				double y = f(X);
				for (int i=0; i<X.rows; i++) {
					for (double k=1e-12; k<=1e+6; k*=1e1) {
						
						XP(i) = X(i) + (X(i) ? X(i)*k: k);
						double yp = f(XP);
						G(i) = (yp-y)/(X(i) ? X(i)*k: k);
						if (std::abs((yp-y)/(yp+1e-100)) > 1e-9) break;
					}
					XP(i) = X(i);
				}
				return G;
			}
		);
	}

	bool operator()(cv::Mat1d &X, std::function<double(const cv::Mat1d&)> &&f, std::function<cv::Mat1d(const cv::Mat1d&)> &&g) {
		
		double fx = f(X);
		for (int k=0; k<max_iterations; k++) {

			X -= alpha*g(X);
			double fxp = f(X);
			if (abs(fx-fxp)<epsilon) return true;
			fx = fxp;
		}
		return false;
	}
};
