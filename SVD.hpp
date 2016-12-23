#pragma once

#ifdef USELAPACKSVD
extern "C" int dgesdd_(const char *jobz, int *m, int *n, double *a, int *lda, double *s, double *u, int *ldu, double *vt, int *ldvt, double *work, int *lwork, int *iwork, int *info);
#endif	

#ifdef USELAPACKSVD
struct SVD {
	
	cv::Mat1d u,w,vt;
	SVD(cv::Mat1d S, int=0) {
		int m = S.rows, n = S.cols, l=std::min(n,m);
		
		cv::Mat1d wd  = cv::Mat1d(1,l);
		cv::Mat1d ud  = cv::Mat1d(l,m);
		cv::Mat1d vtd = cv::Mat1d(n,l);

		double workSize;
		int lwork = -1;
		std::vector<int> iwork(8*std::min(m,n));
		int info = 0;

		dgesdd_("S", &m, &n, &cv::Mat1d(S.t())(0,0), &m, &wd(0), &ud(0,0), &m, &vtd(0,0), &l, &workSize, &lwork, &iwork[0], &info);

		lwork = workSize;
		std::vector<double> work(workSize);

		dgesdd_("S", &m, &n, &cv::Mat1d(S.t())(0,0), &m, &wd(0), &ud(0,0), &m, &vtd(0,0), &l, &work[0], &lwork, &iwork[0], &info);
		
		w=wd.t();
		u=ud.t();
		vt=vtd.t();
	}
	
	~SVD(){}
};
#else
typedef cv::SVD SVD;
#endif	
