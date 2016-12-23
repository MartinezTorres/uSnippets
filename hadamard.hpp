////////////////////////////////////////////////////////////////////////
// micro JPEG encoder
//
// Manuel Martinez (manuel.martinez@kit.edu)
// based on: jpge from Rich Geldreich
//
// Stripped down JPEG encoder with the base essentials. Compression is
// fixed to baseline YUV 411. Suit yourself to modify it for your own
// needs.
// 
// The goal of this library is to provide a small but clear baseline jpeg
// implementation for teaching and experimentation purposes.
//
// license: LGPLv3

#pragma once

#include <opencv/cv.h>
#include <string>
#include <cstdio>
#include <cmath>
#include <vector>
#include <cassert>

namespace uSnippets {
class Hadamard {
	
	static void hadamard44( int16_t (&H)[4][4], const uint8_t *img, uint32_t stride, const uint8_t (&Q)[4]) {
		static const int16_t HT[] = {
			 1, 1, 1, 1,
			 1,-1, 1,-1,
			 1, 1,-1,-1,
			 1,-1,-1, 1};
						
		int16_t H1[4][4];
		
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				H1[i][j]=H[i][j]=0;
		
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				for (int k=0; k<4; k++) 
					H1[i][j] += HT[j*4+k] * img[i+stride*k];
					
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				for (int k=0; k<4; k++) 
					H[j][i] += HT[j*4+k] * H1[k][i];
					
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				H[i][j] = round(H[i][j]/(pow(2,2*Q[i])));				
	}
	
	static void ihadamard44( uint8_t *img, uint32_t stride, int16_t (&H)[4][4], const uint8_t (&Q)[4]) {
		
		static const int16_t HT[] = {
			 1, 1, 1, 1,
			 1,-1, 1,-1,
			 1, 1,-1,-1,
			 1,-1,-1, 1};
						
		int16_t H1[4][4];
		
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				H[i][j] = H[i][j]*(pow(2,2*Q[i]));

		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				img[i+stride*j]=H1[i][j]=0;

		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				for (int k=0; k<4; k++) 
					H1[j][i] += HT[j*4+k] * H[k][i];
					
		for (int i=0; i<4; i++) 
			for (int j=0; j<4; j++) 
				for (int k=0; k<4; k++) 
					img[i+stride*j] += HT[j*4+k] * H1[i][k] / 16;
		
	}
	

	static void mm44_v0( int16_t (&)[4][4], uint8_t *img, uint32_t stride, const uint8_t (&)[4]) {
		
		static double sz = 0;
		static double n = 0;
		
		uint8_t mx = 0, mn = 255;
		for (int i=0; i<4; i++) {
			for (int j=0; j<4; j++) { 
				mx = std::max(mx, img[i+stride*j]);
				mn = std::min(mn, img[i+stride*j]);
			}
		}
					
		if (mx-mn > 63) {
			double r=8;
			for (int i=0; i<4; i++) 
				for (int j=0; j<4; j++) 
					img[i+stride*j] = mn+(mx-mn)*round(r*(img[i+stride*j]-mn)/(std::max(mx-mn,1)))/r;
			sz += 6;

		} else if (mx-mn > 15) {
			double r=4;
			for (int i=0; i<4; i++) 
				for (int j=0; j<4; j++) 
					img[i+stride*j] = mn+(mx-mn)*round(r*(img[i+stride*j]-mn)/(std::max(mx-mn,1)))/r;
			sz += 4;

		} else if (mx-mn > 3) {
			double r=2;
			for (int i=0; i<4; i++) 
				for (int j=0; j<4; j++) 
					img[i+stride*j] = mn+(mx-mn)*round(r*(img[i+stride*j]-mn)/(std::max(mx-mn,1)))/r;
			sz += 2;
		} else {
			for (int i=0; i<4; i++) 
				for (int j=0; j<4; j++) 
					img[i+stride*j] = (mn+mx)/2;
			sz += 2;
		}
		sz += 2;
		n++;
		
		std::cout << 100.*sz/(16.*n) << std::endl;
	}
	
	static void mm44_v1(int16_t (&H)[4][4], uint8_t *img, uint32_t stride, const uint8_t (&)[4]) {
						
		int c=2; 
		// 1 3 5 7 > 2 -2 6 -2 > 4 -2 -4 -2 
		// 1 3 5 9 > 2 -2 7 -4 > 4 -2 -5 -4 
		auto f  = [](int16_t &h1, int16_t &h2) { int16_t a=h1, b=h2; h1 = (a+b)>>1; h2 = (a-b); };
		auto fi = [](int16_t &h1, int16_t &h2) { int16_t a=(h1<<1)+(h2&1), b=h2; h1 = (a+b)>>1; h2 = (a-b)>>1; };
		
		if (0) for (int i=0; i<256; i++) for (int j=0; j<256; j++) {
			int16_t ii = i, jj = j;
			f(ii,jj); 
			int16_t iii = ii, jjj = jj;
			fi(iii,jjj);
			if (iii!=i || jjj!=j) {std::cout << i << ":" << ii << ":" << iii << " " << j << ":" << jj << ":" << jjj << std::endl; exit(-1); }
		}

		for (int i=0; i<4; i++) for (int j=0; j<4; j++) H[i][j] = img[i+stride*j];
		
		for (int i=0; i<4; i++) { f(H[i][0], H[i][1]); f(H[i][2], H[i][3]); f(H[i][0], H[i][2]); if (c   ) { H[i][1] -= H[i][2]/2; H[i][3] -= H[i][2]/2; } }
		for (int i=0; i<4; i++) { f(H[0][i], H[1][i]); f(H[2][i], H[3][i]); f(H[0][i], H[2][i]); if (c==2) { H[1][i] -= H[2][i]/2; H[3][i] -= H[2][i]/2; } }

		for (int i=0; i<4; i++) for (int j=0; j<4; j++) if ((i%2)+(j%2)) H[i][j]/=16; else if(i+j) H[i][j]/=16;
		
		
					
	}
	
	static void imm44_v1(uint8_t *img, uint32_t stride, int16_t (&H)[4][4], const uint8_t (&)[4]) {
		
		int c=2; 
		//auto f  = [](int16_t &h1, int16_t &h2) { int16_t a=h1, b=h2; h1 = (a+b)>>1; h2 = (a-b); };
		auto fi = [](int16_t &h1, int16_t &h2) { int16_t a=(h1<<1)+(h2&1), b=h2; h1 = (a+b)>>1; h2 = (a-b)>>1; };
		
		for (int i=0; i<4; i++) for (int j=0; j<4; j++) if ((i%2)+(j%2)) H[i][j]*=16; else if(i+j) H[i][j]*=16;

		for (int i=0; i<4; i++) { if (c==2) { H[1][i] += H[2][i]/2; H[3][i] += H[2][i]/2; } fi(H[0][i], H[2][i]); fi(H[0][i], H[1][i]); fi(H[2][i], H[3][i]);  }
		for (int i=0; i<4; i++) { if (c   ) { H[i][1] += H[i][2]/2; H[i][3] += H[i][2]/2; }  fi(H[i][0], H[i][2]); fi(H[i][0], H[i][1]); fi(H[i][2], H[i][3]);  }

		for (int i=0; i<4; i++) for (int j=0; j<4; j++) img[i+stride*j] = H[i][j];
	}
	
	
	struct Header {
		uint16_t magic, rows, cols, q;
	};
	
public:

	static std::string encode(cv::Mat1b img, int quality = 50) {
		
		static const uint8_t Q[] = {1,4,2,4};
		
		Header header = { 0x8ADA, (uint16_t)img.rows, (uint16_t)img.cols, (uint16_t)quality };
		std::string res((char *)&header, sizeof(Header));
		
		int mean = 0;
		
		for (int i=0; i<img.rows; i+=4) {
			for (int j=0; j<img.cols; j+=4) {
				
				int16_t H[4][4];
				mm44_v0(H, &img(i,j), img.cols, Q);
				
				//for (int ii=0; ii<4; ii++) for (int jj=0; jj<4; jj++) mean += abs(H[ii][jj]);
				
				//imm44_v1(&img(i,j), img.cols, H, Q);
				//hadamard44(H, &img(i,j), img.cols, Q);
				//ihadamard44(&img(i,j), img.cols, H, Q);
			}
		}
		std::cout << double(mean)/(img.rows*img.cols) << std::endl;
		
		return res;
	}

	static cv::Mat1b decode(std::string data) {
		
		if (data.size() < sizeof(Header)) return cv::Mat1b();
		Header &header = *(Header *)&data[0];
		cv::Mat1b res(header.rows, header.cols);
		return res;
	}
};
}
// Thanks for R.T.F.C.
