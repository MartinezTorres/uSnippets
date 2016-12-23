////////////////////////////////////////////////////////////////////////
// communication library
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once

#include <thread>
#include <mutex>
 
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace uSnippets {
	
	// Suggested values: 
	// -3: very low level, ordinary stuff: which is part of the normal operating procedure and might cause message flood
	// -2: low level, ordinary stuff: which is part of the normal operating procedure
	// -1: controlled recovery stuff: i.e. purging an excessively big buffer
	//  0: meaningful events: part of normal operating conditions (connection of a client to a server)
	//  1: very meaningful event: might still be part of normal operating conditions, but uncommon (temporal loss of connectivity)
	//  2: non-fatal error: something that shuold not happen... but happens and is correctable (wrong checksum or magic)
	//  3: fatal error diagnostics
	class Log {
		int level;
		std::stringstream * const sstr;
		int threadid() { 
			static std::map<std::thread::id,int> dict; 
			int ret = dict[std::this_thread::get_id()];
			if (not ret) ret = dict[std::this_thread::get_id()] = dict.size();
			return ret;
		}

		double threadTime(uint id) { 
			static std::vector<std::chrono::time_point<std::chrono::steady_clock>> last;
			if (id>=last.size()) last.resize(id+1, std::chrono::steady_clock::now());
			auto now = std::chrono::steady_clock::now();
			double ret = std::chrono::duration<double>(now-last[id]).count();
			last[id]=now;
			return ret;
		}
	public:
		std::string msg() const { return sstr?sstr->str():""; }
		static int &reportLevel() { static int rl = 0; return rl; };
		static int reportLevel(int l) { return reportLevel()=l; };
		Log (int level) : level(level), sstr(level>=reportLevel()?new std::stringstream():nullptr) {}
		~Log() {
			if (sstr) {
				static std::mutex mtx;
				static auto start = std::chrono::steady_clock::now();
				static auto last = std::chrono::steady_clock::now();

				std::lock_guard<std::mutex> lock(mtx);
				auto now = std::chrono::steady_clock::now();
				if (level==0) std::cerr << "\x1b[34;1m";
				if (level==1 or level==-2) std::cerr << "\x1b[32;1m";
				if (level>=2 or level==-1) std::cerr << "\x1b[31;1m";
				std::boolalpha(std::cerr);
				std::cerr << "L" << std::setw( 2 ) << level << " ";				
				std::cerr << "[" << std::fixed << std::setw( 8 ) << std::setprecision( 4 )  << std::chrono::duration<double>(now-start).count() << "] ";
//				std::cerr << "[" << std::fixed << std::setw( 8 ) << std::setprecision( 4 )  << std::chrono::duration<double>(now-start).count() << " " <<  std::setw( 8 ) << std::chrono::duration<double>(now-last).count() << "] ";
				last = now;

				int t = threadid();
				std::cerr << std::string( ((t-1)%6)*25,' ') << "#" << t << "[" << std::fixed << std::setw( 8 ) << std::setprecision( 4 )  << threadTime(t) << "] ";
				
				std::cerr << sstr->str() << std::endl;
				std::cerr << "\x1b[0m";
				delete sstr;
			}
		}
		template<typename T> Log &operator<<(const T &v) { if (sstr) *sstr << v; return *this; }
		void operator<<(std::nullptr_t) {}
	};
	
	class Assert {
		bool condition;
		Log * const l = nullptr;
	public:
		static bool &verbose() { static bool b = 0; return b; };
		static bool  verbose(bool b) { return verbose() = b; };
		
		Assert (bool condition, int level=9) : condition(condition), l(not verbose() and condition?nullptr:new Log(level)) {}
		~Assert() {
			if (l) {
				std::string s = l->msg();
				delete l;
				if (not condition) throw std::runtime_error(s);
			}
		}
		template<typename T> Assert &operator<<(const T &v) { if (l) *l << v; return *this; }
		void operator<<(std::nullptr_t) {}
	};
}
	
