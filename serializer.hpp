////////////////////////////////////////////////////////////////////////
// serializer.hpp: One class to rule it all
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once
#include <uSnippets/object.hpp>

namespace cv { template<class> struct Mat_; template<class,int> struct Vec; template<class,int, int> struct Matx; }

namespace uSnippets {

class Serializer {
protected:
	constexpr Serializer() noexcept = delete;
	Serializer(const Serializer&) = delete;
	Serializer(Serializer&&) = delete;
	Serializer& operator=(const Serializer&) & = delete;
	Serializer& operator=(Serializer&&) & = delete;

	// GENERAL DRIVERS
	// UNCONSTIFIER
	template<class T, typename std::enable_if<std::is_const<T>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { pU(is, *const_cast<typename std::remove_cv<T>::type*>(&t)); }
	
	// STRINGING
	static void pS(std::ostream &os, const char *s) = delete; //disable serializing of constant strings. leads to dangerous behavior.
	static void pU(std::ostream &os, char *s) = delete;       //disable serializing of constant strings. leads to dangerous behavior.

	static void pS(std::ostream &os, const std::string &s) { pS(os,s.size()); os << s; }
	static void pU(std::istream &is, std::string &t) { size_t sz; pU(is,sz); t.resize(sz); is.read(&t[0],sz); }

	// ARRAYS OF FIXED SIZE AND SIMPLE CONTENT
	template<class T, std::size_t N, typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
	static void pS(std::ostream &os, const std::array<T,N> &a) { os.write((const char *)a.data(), N*sizeof(T)); }

	template<class T, std::size_t N, typename std::enable_if<std::is_arithmetic<T>::value,int>::type=0>
	static void pU(std::istream &is, std::array<T,N> &a) { is.read((char *)a.data(),N*sizeof(T)); }

	// ARITHMETIC INTEGER
	template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type=0, typename std::enable_if<not std::is_const<T>::value,int>::type=0>
	static void pS(std::ostream &os, const T &v) {
		char c[64], *cp=&c[63]; *cp--='|'; T mv=std::abs(v); while (mv) {*cp--='0'+mv%10; mv/=10;} if (v<0) *cp--='-';
		os.write(cp+1,&c[64]-(cp+1));}
	
	template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type=0, typename std::enable_if<not std::is_const<T>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { 
		char c[64], *p=&c[0]; is.getline(c,64,'|'); 
		if (*p=='-') p++; t=0; while (*p) t=10*t+ *p++ -'0'; t*=(c[0]=='-'?-1:1);  }

	// ARITHMETIC FLOATING POINT
	template<class T, typename std::enable_if<std::is_floating_point<T>::value,int>::type=0, typename std::enable_if<not std::is_const<T>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { os.write((const char *)&v,sizeof(v)); }

	template<class T, typename std::enable_if<std::is_floating_point<T>::value,int>::type=0, typename std::enable_if<not std::is_const<T>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { is.read((char *)&t,sizeof(t)); }

	// PAIRS
	template<class K, class T> 
	static void pS(std::ostream &os, const std::pair<K,T> &t) {  pS(os,t.first); pS(os,t.second);}

	template<class K, class T> 
	static void pU(std::istream &is, std::pair<K,T> &t) { pU(is,t.first); pU(is,t.second); } 
	
	// CONTAINERS
	template<class T, class   = typename T::value_type, class = decltype(std::declval<T>().clear())> 
	static void pS(std::ostream &os, const T &v) { pS(os,v.size()); for (auto &e: v) pS(os,e); }
	
	template<class T, class I = typename T::value_type, class = decltype(std::declval<T>().clear())> 
	static void pU(std::istream &is, T &t) { size_t sz; pU(is,sz); t.clear(); while (sz--) { I i; pU(is,i); t.insert(t.end(),std::move(i)); }} 

	// ARRAYS
	template<typename T, std::size_t Size>
	static void pS(std::ostream &os, const T(&v)[Size]) { for (std::size_t s=0; s<Size; s++) pS(os,v[s]); }
	
	template<typename T, std::size_t Size>
	static void pU(std::istream &is, T(&t)[Size]) { for (std::size_t s=0; s<Size; s++) pU(is,t[s]); } 

	// SERIALIZATIONS
	struct Reader { std::ostream *os; template<class T> Reader &operator&(const T &t) { pS(*os,t); return *this;} };
	struct Writer { std::istream *is; template<class T> Writer &operator&(T &t) { pU(*is,t); return *this;} };

	// IN-CLASS SERIALIZATIONS
	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serialize(*(new Reader()),0))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); ((T*)&v)->serialize(r,0); }

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serializeR(*(new Reader()),0))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); v.serializeR(r,0); }

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serialize(*(new Writer()),0))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); t.serialize(w,0);}

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serializeW(*(new Writer()),0))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); t.serializeW(w,0);}

	// NON-CLASS SERIALIZATIONS
	template<class T, typename std::enable_if<std::is_void<decltype(serializeRW(*(new Reader()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); serializeRW(r,0,*((T*)&v)); }

	template<class T, typename std::enable_if<std::is_void<decltype(serializeR(*(new Reader()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); serializeR(r,0,*((T*)&v)); }

	template<class T, typename std::enable_if<std::is_void<decltype(serializeRW(*(new Writer()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); serializeRW(w,0,t);}

	template<class T, typename std::enable_if<std::is_void<decltype(serializeW(*(new Writer()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); serializeW(w,0,t);}

	// OpenCV MATS
//	template<class T, decltype(T().elemSize()) = 0>
//	static void pS(std::ostream &os, const T &t) { pS(os,t.rows); pS(os,t.cols); os.write((char *)t.data, t.rows*t.cols*t.elemSize());}

	template<class T>
	static void pS(std::ostream &os, const cv::Mat_<T> &t) { pS(os,t.rows); pS(os,t.cols); os.write((char *)t.data, t.rows*t.cols*t.elemSize());}
	
	template<class T>
	static void pU(std::istream &is, cv::Mat_<T> &t) { int rows,cols; pU(is,rows); pU(is,cols); t.create(rows,cols); is.read((char *)t.data, t.rows*t.cols*t.elemSize());} 

	template<class T, int N>
	static void pS(std::ostream &os, const cv::Vec<T, N> &t) { os.write((char *)&t, sizeof(cv::Vec<T, N>));}
	
	template<class T, int N>
	static void pU(std::istream &is, cv::Vec<T, N> &t) { is.read((char *)&t, sizeof(cv::Vec<T, N>));} 

	template<class T, int R, int C>
	static void pS(std::ostream &os, const cv::Matx<T, R, C> &t) { os.write((char *)&t, sizeof(cv::Matx<T, R, C>));}
	
	template<class T, int R, int C>
	static void pU(std::istream &is, cv::Matx<T, R, C> &t) { is.read((char *)&t, sizeof(cv::Matx<T, R, C>));} 

public:

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pS(*(std::ostringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static std::string serialize(const T& t) { std::ostringstream oss; pS(oss,t); return oss.str(); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static T unserialize(const std::string &s) { std::istringstream iss(s); T t; unserialize(iss, t); return std::move(t); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static bool unserialize(const std::string &s, T& t) { std::istringstream iss(s); return unserialize(iss, t); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static bool unserialize(std::istream &is, T& t) { try { pU(is,t); return true; } catch (std::istream::failure) { return false; } }

};

/*
namespace Serializer2 {

//	template<class U, typename std::enable_if<std::is_integral<U>::value,int>::type=0> using Integral = U;
//	template<class U, typename std::enable_if<std::is_floating_point<U>::value,int>::type=0>  using FloatingPoint = U;
//	template<class U, std::size_t N, typename std::enable_if<std::is_arithmetic<U>::value,int>::type=0> using Array = std::array<U,N>;
	template<class U, class = typename U::value_type, class = decltype(std::declval<U>().clear())> using Container = U;
	template<class U, typename std::enable_if<std::is_pod<U>::value,int>::type=0> using Pod = U;

	// Pod data types
	template<class T> static inline void S(std::ostream &os, const Pod<T> &v) { os.write((const char *)&v,sizeof(v)); }
	template<class T> static inline void pU(std::istream &is, Pod<T> &t) { is.read((char *)&t,sizeof(t)); }
	 
	// STRINGING
	static inline void S(std::ostream &, const char *) = delete; //disable serializing of constant strings. leads to dangerous behavior.
	static inline void U(std::istream &, char *) = delete; //disable serializing of constant strings. leads to dangerous behavior.

	static inline void S(std::ostream &os, const std::string &s) { S(os,s.size()); os << s; }
	static inline void U(std::istream &is, std::string &t) { size_t sz; U(is,sz); t.resize(sz); is.read(&t[0],sz); }

	// PAIRS
	template<class K, class T> static inline void S(std::ostream &os, const std::pair<K,T> &t) { S(os,t.first); S(os,t.second); }
	template<class K, class T> static inline void U(std::istream &is, std::pair<K,T> &t) { U(is,t.first); U(is,t.second); } 
		
	// CONTAINERS
	template<class T> static inline void S(std::ostream &os, const Container<T> &v) { S(os,v.size()); for (auto &e: v) S(os,e); }
	template<class T> static inline void U(std::istream &is, Container<T> &t) { size_t sz; pU(is,sz); t.clear(); while (sz--) { I i; pU(is,i); t.insert(t.end(),std::move(i)); }} 

	// SERIALIZATION HELPERS
	struct Reader { std::ostream *os; template<class T> Reader &operator&(const T &t) { pS(*os,t); return *this;} };
	struct Writer { std::istream *is; template<class T> Writer &operator&(T &t) { pU(*is,t); return *this;} };

	// IN-CLASS SERIALIZATIONS
	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serialize(*(new Reader()),0))>::value,int>::type=0> 
	static inline void pS(std::ostream &os, const T &v) { Reader r({&os}); ((T*)&v)->serialize(r,0); }

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serialize(*(new Writer()),0))>::value,int>::type=0> 
	static inline void pU(std::istream &is, T &t) { Writer w({&is}); t.serialize(w,0);}

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serializeR(*(new Reader()),0))>::value,int>::type=0> 
	static inline void pS(std::ostream &os, const T &v) { Reader r({&os}); v.serializeR(r,0); }

	template<class T, typename std::enable_if<std::is_void<decltype(std::declval<T>().serializeW(*(new Writer()),0))>::value,int>::type=0> 
	static inline void pU(std::istream &is, T &t) { Writer w({&is}); t.serializeW(w,0);}

	// NON-CLASS SERIALIZATIONS
	template<class T, typename std::enable_if<std::is_void<decltype(serializeRW(*(new Reader()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); serializeRW(r,0,*((T*)&v)); }

	template<class T, typename std::enable_if<std::is_void<decltype(serializeRW(*(new Writer()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); serializeRW(w,0,t);}

	template<class T, typename std::enable_if<std::is_void<decltype(serializeR(*(new Reader()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pS(std::ostream &os, const T &v) { Reader r({&os}); serializeR(r,0,*((T*)&v)); }

	template<class T, typename std::enable_if<std::is_void<decltype(serializeW(*(new Writer()),0,*((T*)nullptr)))>::value,int>::type=0> 
	static void pU(std::istream &is, T &t) { Writer w({&is}); serializeW(w,0,t);}

	// OpenCV MATS
//	template<class T, decltype(T().elemSize()) = 0>
//	static void pS(std::ostream &os, const T &t) { pS(os,t.rows); pS(os,t.cols); os.write((char *)t.data, t.rows*t.cols*t.elemSize());}

	template<class T>
	static void pS(std::ostream &os, const cv::Mat_<T> &t) { pS(os,t.rows); pS(os,t.cols); os.write((char *)t.data, t.rows*t.cols*t.elemSize());}
	
	template<class T>
	static void pU(std::istream &is, cv::Mat_<T> &t) { int rows,cols; pU(is,rows); pU(is,cols); t.create(rows,cols); is.read((char *)t.data, t.rows*t.cols*t.elemSize());} 

	template<class T, int N>
	static void pS(std::ostream &os, const cv::Vec<T, N> &t) { os.write((char *)&t, sizeof(cv::Vec<T, N>));}
	
	template<class T, int N>
	static void pU(std::istream &is, cv::Vec<T, N> &t) { is.read((char *)&t, sizeof(cv::Vec<T, N>));} 

	template<class T, int R, int C>
	static void pS(std::ostream &os, const cv::Matx<T, R, C> &t) { os.write((char *)&t, sizeof(cv::Matx<T, R, C>));}
	
	template<class T, int R, int C>
	static void pU(std::istream &is, cv::Matx<T, R, C> &t) { is.read((char *)&t, sizeof(cv::Matx<T, R, C>));} 


//	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pS(*(std::ostringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
//	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 

//	static inline T unserialize(const std::string &s) { std::istringstream iss(s); T t; unserialize(iss, t); return std::move(t); }
//	static inline bool unserialize(const std::string &s, T &t) { std::istringstream iss(s); return unserialize(iss, t); }	
//	static inline bool unserialize(std::istream &is, T &t) { try { U(is,t); return true; } catch (std::istream::failure) { return false; } }
//	static inline std::string serialize(const T &t) { std::ostringstream oss; S(oss,t); return oss.str(); }


	






	

public:

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pS(*(std::ostringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static std::string serialize(const T& t) { std::ostringstream oss; pS(oss,t); return oss.str(); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static T unserialize(const std::string &s) { std::istringstream iss(s); T t; unserialize(iss, t); return std::move(t); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static bool unserialize(const std::string &s, T& t) { std::istringstream iss(s); return unserialize(iss, t); }

	template<class T, typename std::enable_if<std::is_void<decltype(Serializer::pU(*(std::istringstream *)nullptr, *(T *)nullptr))>::value,int>::type=0> 
	static bool unserialize(std::istream &is, T& t) { try { pU(is,t); return true; } catch (std::istream::failure) { return false; } }
}*/

struct Archive : public std::string {
	
	using std::string::string;
	Archive() {}
	Archive(const std::string &s) {assign(s);}
	template<typename T> Archive &operator<<(const T &t) { append(Serializer::serialize(t)); return *this;}
	template<typename T> Archive &operator>>(T &t) { std::istringstream ss(*this); Serializer::unserialize(ss, t); assign(substr(ss.tellg())); return *this;}
};
}
