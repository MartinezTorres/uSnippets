#pragma once

#include <opencv/cv.h>
#include <uSnippets/LM.hpp>
#include <uSnippets/LassoShooting.hpp>
#include <uSnippets/SVD.hpp>
#include <uSnippets/OWLQN.hpp>

cv::Mat1d LassoShotgun(cv::Mat1d X_, cv::Mat1d Y, double lambda, cv::Mat1b mask = cv::Mat1b());

struct SparseSubspaceClustering {

	int N;
	cv::Mat1i labels;
	
	SparseSubspaceClustering() {}
	
	SparseSubspaceClustering(cv::Mat1d samples, int nClusters, int K=2, cv::Mat1d labels = cv::Mat1d()) {
		train(samples, nClusters, K, labels);
	}
	
	static cv::Mat1i relabel(int N, const cv::Mat1i NL_, const cv::Mat1i OL) {
		
		cv::Mat1i NL = NL_.clone();
		std::vector<int> R(N);
		
		std::map<int, bool> NLAvail, OLAvail;
		for (int i=0; i<N; i++) 
				NLAvail[i]=OLAvail[i]=true;
		
		for (int i=0; i<N; i++) {
			std::map<std::pair<int, int>, int> countMap;
			for (int k=0; k<NL.rows; k++)
				//if (NLAvail[NL(k)] and OLAvail[OL(k)])
				if (NLAvail[NL(k)])
					countMap[std::make_pair(NL(k), OL(k))]++;

			int ol = 0, nl = 0;
			if (countMap.empty()) { 
				while (not NLAvail[nl]) nl++;
				while (not OLAvail[ol]) ol++;
			} else {
				
				int best = 0;
				for (auto &e : countMap) {
					if (e.second>best) {
						best = e.second;
						nl = e.first.first;
						ol = e.first.second;
					}
				}
			}
			R[nl] = ol;
			NLAvail[nl] = false;
			OLAvail[ol] = false;
		}
		
		for (int k=0; k<NL.rows; k++)	NL(k) = R[NL(k)];
			
		return NL;
	}
	
	void train(cv::Mat1d S_, int N_, int K=2, cv::Mat1i labels_ = cv::Mat1d()) {
		
		//cv::PCA pca(S_, cv::Mat(), CV_PCA_DATA_AS_COL, 8);
		//cv::Mat1d S = pca.project(S_);
		cv::Mat1d S = S_;
		
		N = N_;
		cv::Mat1d C;
		
		if (false) {

/*			for (int i=0; i<S.cols; i++) {

				std::cout << "A " << i << std::endl;
			
				cv::Mat1d x = cv::Mat1d(S.cols,1);
				cv::Mat1d y = cv::Mat1d(S.rows+1,1);
				for (int j=0; j<S.cols; j++)
					x(j) = (2./655360000.)*(rand()%65536-65536/2)*(1./S.cols);

				LM( x, y, [&](const cv::Mat1d &x, cv::Mat1d &y)->bool{
					
					for (int j=0; j<S.rows; j++)
						y(j) = S(j,i);
						
					for (int j=0; j<S.cols; j++)
						if (i!=j)
							for (int k=0; k<S.rows; k++)
								y(k) -= x(j)*S(k,j);
						
					y(S.rows) = 0.;	
					for (int j=0; j<S.cols; j++)
						if (i!=j)
							y(S.rows) += (std::abs(x(j)))*1e-2;

					return false;
				}, 10, 1e-6);
				//cout << x.t() << endl;
				//cout << y.t() << endl;
				//cout << S.col(i).t() << endl;
				C.push_back(cv::Mat1d(x.t()));
			}
			C = C.t();*/
		} else {
		
			for (int i=0; i<S.cols; i++) {

				//std::cout << "A " << i << std::endl;
			
				cv::Mat1d SP = cv::Mat1d(S.colRange(0,i).t());
				SP.push_back(cv::Mat1d(S.colRange(i+1,S.cols).t()));
				cv::Mat1d D = SP.t(), DT = SP, Y = S.col(i);

				cv::Mat1d y = LassoShooting(D, Y, 1000., 16);

				cv::Mat1d y2(S.cols,1);
				for (int j=0; j<S.cols; j++)
					if (j<i) y2(j)=y(j);
					else if (j>i) y2(j)=y(j-1);
					else y2(j)=0;
				C.push_back(cv::Mat1d(y2.t()));
			}
			C = C.t();
			
		}
		
		std::cout << "A" << S.cols << std::endl;
		
		//Affinity matrix
		cv::Mat1d AM = cv::abs(C)+cv::abs(C.t());

		//std::cout << "A1" << AM << std::endl;
		
		//Laplacian
		cv::Mat1d AMD;
		cv::reduce(AM, AMD, 0, CV_REDUCE_SUM);
		cv::Mat1d AMSQ;
		cv::sqrt(AMD, AMSQ);
//		cv::Mat1d DN =  cv::Mat1d::diag(1./AMSQ.t());
		std::cout << "AK" << std::endl;
//		cv::Mat1d L = cv::Mat1d::eye(S.cols,S.cols)-DN*AM*DN;
		cv::Mat1d DNAMDN = AM.clone();
		for (int i=0; i<DNAMDN.rows; i++) DNAMDN.row(i)*=1./AMSQ(i);
		for (int i=0; i<DNAMDN.cols; i++) DNAMDN.col(i)*=1./AMSQ(i);
//		for (int i=0; i<DNAMDN.cols; i++) DNAMDN(i,i) += 1.;
		cv::Mat1d L = cv::Mat1d::eye(S.cols,S.cols)-DNAMDN;

		std::cout << "A2" << std::endl;

		
		//Spectral Clustering
if (false) {		SVD svdL(L); // Almost Main bottleneck
		std::cout << "A3" << std::endl;
		cv::Mat1f data = svdL.vt.rowRange(svdL.vt.rows-K,svdL.vt.rows).t();
		cv::kmeans(data,N,labels,cv::TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 1e6, 1e-10),100,cv::KMEANS_RANDOM_CENTERS);
} else {
		cv::PCA pca(L, cv::Mat(), CV_PCA_DATA_AS_COL);
		std::cout << "A3" << std::endl;
		cv::Mat1f data = pca.eigenvectors.rowRange(pca.eigenvectors.rows-K-1,pca.eigenvectors.rows-1).t();
		cv::kmeans(data,N,labels,cv::TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 1e6, 1e-10),100,cv::KMEANS_RANDOM_CENTERS);
}

		if (labels_.rows == labels.rows) labels = relabel(N,labels, labels_);

		std::cout << "A" << std::endl;
	};
};

