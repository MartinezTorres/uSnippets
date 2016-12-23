#pragma once

#include <list>
#include <array>
#include <deque>
#include <vector>
#include <cstdlib>
#include <memory>

template <class Pt = std::vector<double>, class T=int>
struct KDTree {
	
	static inline double sqDist( const Pt &p1, const Pt &p2 ) {
	
		double r = 0;
		for (uint i=0; i<p1.size(); i++)
			r += (p1[i]-p2[i])*(p1[i]-p2[i]);
		return r;
	}
	
	static inline double sum( const Pt &p1 ) {
	
		double r = 0;
		for (uint i=0; i<p1.size(); i++)
			r += p1[i];
		return r;
	}
	
	struct Node {
		Pt p;
		T t;
		size_t k;
		int l,r;
	};
	std::vector<Node> nodes;

	void add(const Pt p, T t = T()) {

		nodes.push_back({p, t, std::rand()%p.size(), -1,-1});
		if (nodes.size()==1) return;
		
		int id = nodes.size()-1;

		int i=0, o=0;
		while (i>=0) {
			o = i;
			i = (p[nodes[o].k]<nodes[o].p[nodes[o].k]?nodes[o].l:nodes[o].r);
		}	
		(p[nodes[o].k]<nodes[o].p[nodes[o].k]?nodes[o].l:nodes[o].r) = id;
	}
	
	std::vector<std::pair<Node, double>> getNN(const Pt p, uint N, double R = 1. ,bool skipEqual = false) const {
		
		std::vector<std::pair<int, double>> best(N, {-1,1e100});
		std::deque<std::pair<int, Pt>> toCheck;
		toCheck.push_back({0,p});
		for (uint i=0; i<p.size(); i++) toCheck.front().second[i]=0;
		while (not toCheck.empty()) {
			
			int id = toCheck.front().first;
			Pt minDist = toCheck.front().second; 
			toCheck.pop_front();

			if (id < 0) continue;
			
			const Node &n = nodes[id];
			
			if (sum(minDist)*R > best.back().second) continue;
			
			double d = sqDist(p, n.p);
			
			if (d<best.back().second and (d!=0 or not skipEqual)) { 
			
				int pos = N-1;
				while (pos-- and d<best[pos].second)
					std::swap(best[pos+1],best[pos]);
				best[pos+1] = {id,d};
			}
			
			if (p[n.k]<n.p[n.k]) {
				if (n.l>0) toCheck.push_front({n.l,minDist});
				if (n.r<0) continue;
				minDist[n.k]=(p[n.k]-n.p[n.k])*(p[n.k]-n.p[n.k]);
				toCheck.push_back({n.r,minDist});
				
			} else {
				if (n.r>0) toCheck.push_front({n.r,minDist});
				if (n.l<0) continue;
				minDist[n.k]=(p[n.k]-n.p[n.k])*(p[n.k]-n.p[n.k]);
				toCheck.push_back({n.l,minDist});
			}
		}

		std::vector<std::pair<Node, double>> ret;
		for (auto &i : best) 
			if (i.first>=0)
				ret.push_back({nodes[i.first], i.second});
		return ret;
	}
};




