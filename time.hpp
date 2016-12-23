////////////////////////////////////////////////////////////////////////
// rime management utility library
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3
#pragma once

#include <ctime>
#include <chrono>
#include <ostream>


namespace uSnippets {

using hours        = std::chrono::hours;
using minutes      = std::chrono::minutes;
using seconds      = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds  = std::chrono::nanoseconds;
	
//static constexpr auto &now = std::chrono::system_clock::now;
using time_point = std::chrono::system_clock::time_point;
static inline time_point now() { return std::chrono::system_clock::now(); }

typedef std::chrono::duration<long double> duration_d;
typedef std::chrono::time_point<std::chrono::system_clock, duration_d> time_point_d;

template<class A> static void serializeRW(A & ar, const uint, time_point &tp) { uint64_t ms = std::chrono::duration_cast<microseconds>(tp.time_since_epoch()).count(); ar & ms; tp = time_point(microseconds(ms)); }

static inline std::string print(const time_point &tp) { auto tt = std::chrono::system_clock::to_time_t(tp); return std::ctime(&tt); }
static inline std::string printUTC(const time_point &tp) { auto tt = std::chrono::system_clock::to_time_t(tp); return std::asctime(std::gmtime(&tt)); }

//constexpr duration operator "" _h  (long double               t) { return hours(t); }
constexpr hours operator "" _h  (unsigned long long int    t) { return hours(t); }
//constexpr duration operator "" _min(long double               t) { return duration::minutes(t); }
constexpr minutes operator "" _min(unsigned long long int    t) { return minutes(t); }
//constexpr duration operator "" _s  (long double               t) { return duration::seconds(t); }
constexpr seconds operator "" _s  (unsigned long long int    t) { return seconds(t); }
//constexpr duration operator "" _ms (long double               t) { return duration::milliseconds(t); }
constexpr milliseconds operator "" _ms (unsigned long long int    t) { return milliseconds(t); }
//constexpr duration operator "" _us (long double               t) { return duration::microseconds(t); }
constexpr microseconds operator "" _us (unsigned long long int    t) { return microseconds(t); }
//constexpr duration operator "" _ns (long double               t) { return duration::nanoseconds(t); }
constexpr nanoseconds operator "" _ns (unsigned long long int    t) { return nanoseconds(t); } 
	
}

