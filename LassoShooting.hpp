////////////////////////////////////////////////////////////////////////
// minimal Lasso-Shooting code
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3
//
// GCC-FLAGS: -Ofast -std=c++0x `pkg-config opencv --cflags` `pkg-config opencv --libs`

//      Minimizes: ||Xb-Y|| + lambda |b|_1

#pragma once
#include <opencv/cv.h>

static cv::Mat1d LassoShooting(cv::Mat1d X, cv::Mat1d Y, double lambda, int minNonZero = 0,  int maxIter = 10000, double epsilon = 1e-4) {


	cv::Mat1d XTX = 2*X.t()*X;
	cv::Mat1d XTY = 2*X.t()*Y;
	double *pXTY = &XTY(0);
	double *pXTX = &XTX(0);

	cv::Mat1d B = cv::Mat1d(X.cols,1,0.);
	double *pB = &B(0);

	cv::Mat1d XPRE(X.cols,1,0.);
	double *pXPRE = &XPRE(0);
	
	int nonZero = 0;		
	for (double l = 1e10*lambda; l>=lambda or nonZero<minNonZero; l/=10) { 
		for (int iter=0; iter<maxIter; iter++) {

			double update = 0;
			for (int j=0; j<X.cols; j++) {

				double oldB = pB[j];

				double sj = pXPRE[j] - pXTY[j] - pXTX[j*XTX.cols+j]*pB[j];
				
				if (sj>l) {
					pB[j] = (l-sj)/pXTX[j*XTX.cols+j];
				} else if (sj<-l) {
					pB[j] =-(l+sj)/pXTX[j*XTX.cols+j];
				} else {
					pB[j] = 0;
				}

				if (oldB!=pB[j])
					for (int k=0; k<X.cols; k++)  
						pXPRE[k] += pXTX[j*XTX.cols+k]*(pB[j]-oldB);
				
				update += std::abs(oldB-pB[j]);
			}
			if (update<epsilon)	break;
		}
		nonZero = 0;
		for (int i=0; i<B.rows; i++) if (B(i)!=0.) nonZero++;
	}
	
	return B;
}
