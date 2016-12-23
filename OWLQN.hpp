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

struct OWLQN {

	double c;

    int m;
	double epsilon;
	int max_iterations;
	
	OWLQN(double c=.1, int m = 6, double epsilon=1e-4, int max_iterations=10000) : c(c), m(m), epsilon(epsilon), max_iterations(max_iterations) {}

	cv::Mat1d pseudoGradient(cv::Mat1d X, cv::Mat1d G) {
		
		cv::Mat1d PG = G.clone();
		for (int i=0; i<G.rows; i++)
			if      (X(i) < 0.) PG(i) -= c;
			else if (X(i) > 0.) PG(i) += c;
			else if (G(i) < -c) PG(i) += c;
			else if (G(i) >  c) PG(i) -= c;
			else PG(i) = 0;
		return PG;
	}

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
					for (double k=1e-18; k<=1e-6; k*=1e3) {
						
						XP(i) = X(i) + k;
						double yp = f(XP);
						G(i) = (yp-y)/k;
						if (std::abs((yp-y)/(yp+1e-100)) > 1e-12) break;
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
		double fx = f(X)+c*cv::norm(X,cv::NORM_L1);
		cv::Mat1d G = g(X);
		cv::Mat1d PG = pseudoGradient(X, G);

		// Compute the direction; we assume the initial hessian matrix H_0 as the identity matrix.
		cv::Mat1d D = -PG / cv::norm(PG);
		
		if (cv::norm(PG)/std::max(1.0, cv::norm(X)) < epsilon) return true;
		
		for (int k=0; k<max_iterations; k++) {
			
			// Store the current position and gradient vectors.
			cv::Mat1d XP = X.clone();
			cv::Mat1d GP = G.clone();
			
			// Search for an optimal step. (Backtrack)
			{
				double dgtest = PG.dot(D);
				cv::Mat1d WP = XP.clone(); // Choose the orthant for the new point.
				for (int i=0; i<WP.rows; i++)
					if (WP(i) == 0.)
						WP(i) = -PG(i);

				double fxp=0;
				for (double step = 1.;  step>1e-20; step*=.5) {
					
					X = XP+D*step;
					for (int i=0; i<WP.rows; i++) if (X(i)*WP(i)<=0) X(i)=0;
					fxp = f(X) + c*cv::norm(X,cv::NORM_L1);
					if (fxp <= fx + dgtest * 1e-4) break; // ARMIJO
				}
				
				if (fxp>=fx) return false;
				fx = fxp;
			}
        
			G = g(X);
			PG = pseudoGradient(X, G);
			if (cv::norm(PG)/std::max(1.0, cv::norm(X)) < epsilon) return true; //SUCCESS
        
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

            
			// Recursive formula to compute dir = -(H \cdot g).
			// This is described in page 779 of: Jorge Nocedal.
			// Updating Quasi-Newton Matrices with Limited Storage.
			D = -PG;
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
			
			for (int i=0; i<D.rows; i++)
				D(i) = (D(i)*PG(i)>=0?0.:D(i)); // Constrain the search direction for orthant-wise updates.
			
			hist.push_back(it);
			if (int(hist.size())>m) hist.pop_front();
		}
		return false;
	}
};
