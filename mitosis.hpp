#pragma once

#include <opencv2/opencv.hpp>

template<typename T>
struct PlanarCluster_ {

	typedef cv::Vec<T, 3> Vec3;
	typedef cv::Matx<T, 3, 3> Mat33;
	
	Vec3 Acenter;
	Vec3 center;
	Vec3 normal;
	Mat33 aggMat;
	Vec3 aggVec;
	int size;
	
	PlanarCluster_() : Acenter(0,0,0), center(0,0,0), normal(0,0,0), aggMat(0.), aggVec(0.), size(0) {}
	
	double dist(Vec3 p) {
		return (p-center).dot(p-center);
	}
	
	double isOnPlane(Vec3 p) {
		return abs((p-center).dot(normal));
	}
	
	void update() {
		
		auto mNorm = aggMat.inv()*aggVec;
		normal = { mNorm(0), mNorm(1), -1 };
		normal *= 1/cv::norm(normal);
		
		if (size) center = Acenter * T(1./size);
	}
	
	void push(Vec3 p) {
		
		auto x=p[0], y=p[1], z=p[2];
		aggMat(0,0) += x*x;
		aggMat(1,0) += x*y;
		aggMat(2,0) += x;
		aggMat(0,1) += x*y;
		aggMat(1,1) += y*y;
		aggMat(2,1) += y;
		aggMat(0,2) += x;
		aggMat(1,2) += y;
		aggMat(2,2) += 1;
		aggVec(0) += x*z;
		aggVec(1) += y*z;
		aggVec(2) += z;
		
		Acenter += p;
		size++;
	}

	void pop(Vec3 p) {
		
		auto x=p[0], y=p[1], z=p[2];
		aggMat(0,0) -= x*x;
		aggMat(1,0) -= x*y;
		aggMat(2,0) -= x;
		aggMat(0,1) -= x*y;
		aggMat(1,1) -= y*y;
		aggMat(2,1) -= y;
		aggMat(0,2) -= x;
		aggMat(1,2) -= y;
		aggMat(2,2) -= 1;
		aggVec(0) -= x*z;
		aggVec(1) -= y*z;
		aggVec(2) -= z;

		center -= p;
		size--;
	}
	
	void push(const PlanarCluster_ &c) {
		
		aggMat += c.aggMat;
		aggVec += c.aggVec;
		
		Acenter += c.Acenter;
		size += c.size;
	}
};

typedef PlanarCluster_<double> PlanarCluster;
