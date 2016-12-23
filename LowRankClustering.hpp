#pragma once

#include <opencv/cv.h>
#include <uSnippets/SVD.hpp>
#include <uSnippets/kdtree.hpp>

struct LowRankClustering {

	bool normalizedC;
	int rank, N;
	cv::Mat1i labels;
	cv::Mat1d U, W, V, UW;
	
	KDTree<> kdtree;
	
	LowRankClustering() : normalizedC(false) {}
	
	LowRankClustering(cv::Mat1d samples, int nClusters, int K=2, cv::Mat1d labels = cv::Mat1d(), double tau=0) : normalizedC(false) {
		train(samples, nClusters, K, labels, tau);
	}
	
	~LowRankClustering() {}
	
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
	
	void train(cv::Mat1d S, int N_, int K=2, cv::Mat1i labels_ = cv::Mat1d(), double tau= 0) {
		
		N = N_;

		std::cout << "A" << std::endl;
		
		if (tau==0) tau = 100/(pow(cv::norm(S),2));
		double alpha = 0.5*tau;
		
		double sigma = sqrt((alpha + tau)/(alpha*alpha)/tau);
		sigma = sqrt((alpha + tau)/alpha/tau + sqrt(sigma));
		
		std::cout << "A" << std::endl;

		SVD svdD(S);
		rank = 0;
		for (auto e : cv::Mat1d(svdD.w)) {
			e = e*(e>sigma) + alpha/(alpha+tau)*e*(e<=sigma);
			rank += e > 1./sqrt(tau);
		}
		rank = std::max(rank,1);

		std::cout << "A" << std::endl;

		V = cv::Mat1d(svdD.vt.t()).colRange(0,rank);
		W = cv::Mat::diag(svdD.w.rowRange(0,rank));
		U = svdD.u.colRange(0,rank);
		UW = (1./W)*U.t();

		std::cout << "A" << std::endl;
		
		cv::Mat1d LL(rank,rank,0.);
		for (int i=0; i<LL.rows; i++) 
			LL(i,i) = 1-1./(W(i,i)*W(i,i))/tau;
		cv::Mat1d C = V*LL*V.t();

		std::cout << "A" << std::endl;

		if (normalizedC) { // C Normalization disabled by default
			cv::Mat1d CD;
			cv::reduce(C, CD, 0, CV_REDUCE_SUM);
			for (int i=0; i<C.rows; i++) for (int j=0; j<C.cols; j++) C(i,j)/=CD(j);
		}

		std::cout << "A" << std::endl;
		
		//Affinity matrix
		cv::Mat1d AM = cv::abs(C)+cv::abs(C.t());

		std::cout << "A1" << std::endl;
		
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
		SVD svdL(L); // Almost Main bottleneck
		std::cout << "A3" << std::endl;
		cv::Mat1f data = svdL.vt.rowRange(svdL.vt.rows-K,svdL.vt.rows).t();
		cv::kmeans(data,N,labels,cv::TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 100000, 1e-10),1000,cv::KMEANS_RANDOM_CENTERS);

		if (labels_.rows == labels.rows) labels = relabel(N,labels, labels_);

		std::cout << "A" << std::endl;
			
		kdtree.nodes.clear();
		for (int r=0; r<labels.rows; r++)
			kdtree.add(std::vector<double>(V.ptr<double>(r), V.ptr<double>(r)+V.cols), labels(r));
	};
	
	cv::Mat1d reduce(cv::Mat1d S) const {
		
		return (UW*S).t();
	}
	
	int operator()(cv::Mat1d S) const {
		
		cv::Mat1d uC = reduce(S);
		auto n = kdtree.getNN(std::vector<double>(uC.ptr<double>(0), uC.ptr<double>(0)+uC.cols), 5);

		if (n.empty()) return -1;
		std::vector<int> histogram(N,0);
		for (auto &i : n)
			if (i.first.t>=0 and i.first.t<N)
				++histogram[ i.first.t ];
		return std::max_element( histogram.begin(), histogram.end() ) - histogram.begin();
	};
		
};

