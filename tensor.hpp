#pragma once
#include <opencv2/opencv.hpp>
#include <pstreams/pstream.h>
#include <fstream>
#include <iomanip>
#include <memory>
#include <type_traits>
#include <uSnippets/log.hpp>

namespace uSnippets {

	

template<typename T>
class Tensor_ {
protected: // Storage

	struct Range { 
		int64_t start, end, step;
		Range() : start(0), end(0), step(1) {} // all
		Range(int64_t start) : start(start), end(start+1), step(1) {}
		Range(int64_t start, int64_t end) : start(start), end(end), step(start<end?1:-1) {}
		Range(int64_t start, int64_t end, int64_t step) : start(start), end(end), step(step) {}
		Range(std::initializer_list<size_t> l) { 
			Assert(l.size()<4) << "Range format not recognized";
			std::vector<int64_t> r; for (auto &li : l) r.push_back(li);
			if (r.size()==1) { start=r[0]; end=r[1]+1; step=1;    }
			if (r.size()==2) { start=r[0]; end=r[1];   step=(start<end?1:-1); }
			if (r.size()==3) { start=r[0]; end=r[1];   step=r[2]; }
		}
		size_t size() const { return (end-start)/step; }
		bool operator==(const Range &r) const { return start==r.start and end==r.end and step==r.step; }
		bool operator!=(const Range &r) const { return not (*this == r); }
	};
	
	struct Storage { // Always continuous 
		cv::Mat_<T> M;
		T * const data;
		Storage(size_t sz) : data((T *)malloc(sizeof(T)*sz)) { Assert(sz) << "null allocation"; Assert (data) << "failed to allocate memory"; }
		//Storage &resize(size_t sz) { data = realloc(data, sizeof(T)*sz); Assert(sz) << " null allocation"; Assert (data) << " failed to reallocate memory"; }
		Storage(cv::Mat_<T> mat) : M(mat.isContinuous()?mat:mat.clone()), data(&M(0,0)) {} 
		~Storage() { if (M.empty() and data) free(data);}
	};

	std::shared_ptr<Storage> storage;
	std::shared_ptr<std::vector<size_t>> Sdims, Sstride0, Sstride1;
	T *data, *pend;
	size_t N, *dims, *stride0, *stride1;

protected: // Constructors
	
	Tensor_(std::initializer_list<size_t>) = delete;

	explicit Tensor_(const std::vector<size_t> dimensions) :
		Sdims   (std::make_shared<std::vector<size_t>>(dimensions)),
		Sstride0(std::make_shared<std::vector<size_t>>(dimensions.size(),0)),
		Sstride1(std::make_shared<std::vector<size_t>>(dimensions.size(),1)),
		N(dimensions.size()),
		dims(&Sdims->front()),
		stride0(&Sstride0->front()),
		stride1(&Sstride1->front())
	{
		Assert(N>0) << "called with dimension zero... might be worth to be considered at some point " << N;
		
		int nElem = 1;
		for (size_t i=0; i<N; i++)
			nElem *= dims[i];
			
		storage = std::make_shared<Storage>(nElem);
		data = storage->data;
		
		for (size_t i=N-1; i; i--)
			stride1[i-1] = stride1[i] * dims[i]; 
	}
		
	
public: // Constructors

	explicit Tensor_(cv::Mat_<T> mat) {
		
		storage = std::make_shared<Storage>(mat);
		data = storage->data;

		Sdims = std::make_shared<std::vector<size_t>>();
		for (int i = (mat.dims==2 and mat.rows==1); i<mat.dims; i++) 
			Sdims->push_back(mat.size[i]);

		N = Sdims->size();
		Sstride0 = std::make_shared<std::vector<size_t>>(N,0);
		Sstride1 = std::make_shared<std::vector<size_t>>(N,1);

		dims    = &Sdims->front();
		stride0 = &Sstride0->front();
		stride1 = &Sstride1->front();
		
		for (size_t i=N-1; i; i--)
			stride1[i-1] = stride1[i] * dims[i];
	}
	
	explicit Tensor_(cv::MatExpr && mate) : Tensor_(cv::Mat_<T>(mate)) {}

	template<typename... Ts>
	explicit Tensor_(size_t size, Ts ... t) : Tensor_(std::vector<size_t>({size, static_cast<size_t>(t)...})) {}
	
	Tensor_(Tensor_ &&) = default;
	Tensor_(Tensor_ & ) = default;
	Tensor_(const Tensor_ &&t) : Tensor_(t.clone()) {}
	Tensor_(const Tensor_ &t ) : Tensor_(t.clone()) {}
	
	Tensor_ &operator=(Tensor_ &&t) & = default;
	
	~Tensor_() {}

protected: // Views and Accessors

	Tensor_ viewP() { return *this; }
	
	Tensor_ viewP(const std::vector<Range> v) {
	
		Assert(v.size()==N) << "wrong number of dimensions while generating view";
		
		Tensor_ t = *this;
		
		t.Sdims    = std::make_shared<std::vector<size_t>>(*t.Sdims   );
		t.Sstride0 = std::make_shared<std::vector<size_t>>(*t.Sstride0);
		t.Sstride1 = std::make_shared<std::vector<size_t>>(*t.Sstride1);
		t.dims     = &t.Sdims->front()    + (dims    - &(*Sdims)[0]);
		t.stride0  = &t.Sstride0->front() + (stride0 - &(*Sstride0)[0]);
		t.stride1  = &t.Sstride1->front() + (stride1 - &(*Sstride1)[0]);

		for (size_t d=0; d<N; d++) {
			Range r = (v[d]==Range()?Range(0, dims[d]):v[d]);
			Assert(t.dims[d]) << "t.dims[d]==0 " << r.start << " " << r.end << " " << r.size() << " " << r.step;

			t.stride0[d] += t.stride1[d] * r.start;
			t.stride1[d] *= r.step;
			t.dims[d]     = std::abs(r.size());
		}
		return t;
	}

	template<typename... Ts>
	Tensor_ viewP(size_t v, Ts... rest) {
		

		Assert(N!=0) << "wrong number of dimensions while generating view";
		Assert(v < dims[0]) << " out of bounds access"  << " "<< N<< " "<< v <<" "<< dims[0];

		Tensor_ t = *this;
		t.N--;
		t.data  = t.data + t.stride0[0] + t.stride1[0] * v;
		t.dims++;
		t.stride0++;
		t.stride1++;
		
		return t.viewP(rest...);
	}
	
	
public: // Views and Accessors

	Tensor_ clone() const { // Explicit copy SHOULD BE EXACTLY AND ONLY HERE
		Log(-2) << "Clone!"; 
		Tensor_ t(std::vector<size_t>(dims, dims+N));
		return t.view() = *this; // Holy fuck! it works!
	}

	      Tensor_ view(std::initializer_list<Range> l)       { return view(std::vector<Range>(l.begin(), l.end())); }
	const Tensor_ view(std::initializer_list<Range> l) const { return view(std::vector<Range>(l.begin(), l.end())); }

	// Const unduplifier for views. Also important to avoid copies due to constructors.
	template<typename... Ts>       Tensor_ view(Ts... t)       {                                   return                                                   (*this).viewP(t...) ; }
	template<typename... Ts> const Tensor_ view(Ts... t) const { Log(-3) << "C " << sizeof...(Ts); return const_cast<const Tensor_ &&>(const_cast<Tensor_ &>(*this).viewP(t...)); }

	//helpers
	template<typename... Ts>       Tensor_ operator()(size_t v, Ts... rest)       { return view(v, rest...); }
	template<typename... Ts> const Tensor_ operator()(size_t v, Ts... rest) const { return view(v, rest...); }

	explicit operator T&()       { Assert(N==0) << " Non empty cast N=" << N; return *data; }
	explicit operator T () const { Assert(N==0) << " Non empty cast N=" << N; return *data; }
	
	const T *begin() const { Assert(isContinuous()) << "matrix is not continuous"; return data; }
	T *begin() { Assert(isContinuous()) << "matrix is not continuous"; return data; }
	const T *end() const { return data+nElem(); }
	T *end() { return data+nElem(); }
	
	size_t nElem() const { size_t n=1; for (size_t d=0; d<N; d++) n*=dims[d]; return n; }

protected: // Methods
	
	friend std::ostream & operator<<(std::ostream &os, const Tensor_& t) {

		os << std::scientific;
		
		if (not t.N) {
			os << T(t);
		} else if (t.N>5 or t.nElem()>500) {
			os << "[ Tensor ";
			for (size_t n=0; n<t.N; n++) os << (n?"x":"") << t.dims[n];
			os << " = " << t.nElem() << " ]";
		} else if (t.N==1) { 
			os << "["; 
			for (size_t i=0; i<t.dims[0]; i++) {
				if (i!=0) os << " ";
				os << t(i);
			}
			os << "]";
		} else if (t.N==2) {
			os << "[";
			for (size_t i=0; i<t.dims[0]; i++) {
				if (i!=0) os << "\n ";
				os << t(i);
			}
			os << "]";
		} else {
			std::vector<std::vector<int>> C(1);
			for (size_t n=0; n<t.N-2; n++) {
				std::vector<std::vector<int>> C2; std::swap(C, C2);
				for (size_t i=0; i<t.dims[n]; i++) {
					for (auto V : C2) {
						V.push_back(i);
						C.push_back(V);
					}
				}				
			}
			for (auto &V : C) {
				os << "(";
				for (auto i : V) os << i+1 << ",";
				os << ".,.) =\n";
				if (V.size()==1) os << t(V[0]) << '\n';
				if (V.size()==2) os << t(V[0],V[1]) << '\n';
				if (V.size()==3) os << t(V[0],V[1],V[2]) << '\n';
			}
		}
		return os;
	}
	

public: // Methods
	Tensor_ &rand(T start=0, T end=1) {
		
		if (N==0) {
			*data = (double(std::rand()%(1<<20))/(1<<20))*(end-start)+start;
		} else { // room for improvement later, maybe
			for (size_t i=0; i<dims[0]; i++) 
				(*this)(i).rand();
		}
		return *this;
	}

	bool isContinuous() const { //submatrixes are continous

		if (N==0) return true;
		
		for (size_t i=0; i<N; i++)
			if (stride0[i]) 
				return false;

		for (size_t i=1; i<N; i++)
			if (stride1[i-1] != stride1[i]*dims[i]) 
				return false;
		
		if (stride1[N-1]!=1) return false;

		return true;
	}

	// postcondition: this->dims()(dim) = this->dims()(dim) + data.dims()(dim)
	Tensor_ append(size_t dim, const Tensor_ t) const {

		if (t.storage.get() == storage.get()) return append(dim, clone());
		
		Assert (t.N==N) << "Try concatenatig tensors of different dimensionality";
		Assert (dim<N)   << "Out of range dimension";
		for (size_t i=0; i!=N; i++)
			Assert (i!=dim or t.dims[i]==dims[i]) << "Other dimensions do not match";

		std::vector<size_t> dimensions(dims, dims+N);
		dimensions[dim] += t.dims[dim];

		std::vector<Range> range1(N);
		std::vector<Range> range2(N);
		range1[dim].start=0;               range1[dim].end=dims[dim];
		range2[dim].start=dims[dim];  range2[dim].end=dims[dim]+t.dims[dim];
		
		Tensor_ newT(dimensions);
		newT.view(range1) = *this;
		newT.view(range2) = t;
		return newT;
	}
	
//	Tensor_ &operator=(const Tensor_ &&t) && { return view() = static_cast<const Tensor_ &>(t); }

	Tensor_ &operator=(const cv::Mat_<T>  &t) && { return view() = Tensor_(t); }
	Tensor_ &operator=(const cv::Mat_<T> &&t) && { return view() = Tensor_(t); }
	Tensor_ &operator=(const cv::MatExpr &&t) && { return view() = Tensor_(t); }

	Tensor_ &operator=(const Tensor_ &t) && {
		
		if (&t == this) return *this;
		if (t.storage.get() == storage.get()) return *this = t.clone(); // What about T = T.t() ??

		Assert(t.N == N) << "dimensions do not match in assignment " << N << " " << t.N;
		
		if (N==0) {
			*data = *t.data;
		} else { // room for improvement later, maybe
			
			Assert(t.dims[0] == dims[0]) << "dimensions do not match in assignment " << N << " " << dims[0] << " " << t.dims[0];
			for (size_t i=0; i<dims[0]; i++) 
				(*this)(i) = t(i);
		}
		return *this;
	}
	
	Tensor_ &operator=(const T v) && {
		
		if (N==0) {
			*data = v;
		} else { // room for improvement later, maybe
			for (size_t i=0; i<dims[0]; i++) 
				(*this)(i) = v;
		}
		return *this;
	}

	std::vector<size_t> dimensions() const { return std::vector<size_t>(dims, dims+N); }

/*
	static std::string exec(std::string command, std::string in) {

		constexpr size_t CS = 1<<10;
		
		int linkA[2], linkB[2];
		Assert(pipe(linkA)!=-1) << "Error creating pipe 1";
		Assert(pipe(linkB)!=-1) << "Error creating pipe 2";
		pid_t pid;
		Assert((pid=fork()) !=-1) << "Error forking";
		if (pid==0) {
			dup2 (linkA[0], STDIN_FILENO);
			close(linkA[1]);
			dup2 (linkB[1], STDOUT_FILENO);
  			close(linkB[0]);
			execl("/bin/bash", "bash", "-c", command.c_str(), (char*)0);
			Assert(false) << "execl failed";
		}
		Log(0) << "Payload delivering";
		close(linkA[0]);
		close(linkB[1]);

		for (size_t i=0; i<in.size(); i += CS) { Log(-2) << write(linkA[1], &in[i], std::min(in.size()-i, CS)); fsync(linkA[1]); }
		close(linkA[1]);
		
		Log(0) << "Payload delivered";
		
		std::string out;
		while (true) {
			out.resize(out.size()+CS);
			ssize_t r = read(linkB[0], &out[out.size()-CS], CS);
			Assert(r!=-1) << "buff...";
			out.resize(out.size()-CS+r);
			if (r<CS) break;
		}
		wait(NULL);
		return out;
	}

	std::string toTorchPipes() const {

		Assert(N) << "empty matrix";
	
		// Should be lock protected
		
		std::string command(R"(th -e "dims={} for i=1,io.read('*n') do dims[i]=io.read('*n') end D=torch.FloatTensor(unpack(dims)) D:apply(function() return io.read('*n') end) io.write(torch.serialize(D)) " )");
		
		constexpr size_t CS = 1<<20;
		
		int linkA[2], linkB[2];
		Assert(pipe(linkA)!=-1) << "Error creating pipe 1";
		Assert(pipe(linkB)!=-1) << "Error creating pipe 2";
		pid_t pid;
		Assert((pid=fork()) !=-1) << "Error forking";
		if (pid==0) {
			dup2 (linkA[0], STDIN_FILENO);
			close(linkA[1]);
			dup2 (linkB[1], STDOUT_FILENO);
  			close(linkB[0]);
			execl("/bin/bash", "bash", "-c", command.c_str(), (char*)0);
			Assert(false) << "execl failed";
		}

		Log(-2) << "Delivering payload";
		close(linkA[0]);
		close(linkB[1]);

		char s[100]; 
		snprintf(s, 100, "%zu\n", N); write(linkA[1], s, strlen(s));
		for (size_t i=0; i<N; i++) { snprintf(s, 100, "%zu\n", dims[i]); write(linkA[1], s, strlen(s)); }
		for (const auto &v : this->clone()) { snprintf(s, 100, "%.9f\n", float(v)); write(linkA[1], s, strlen(s)); }
		close(linkA[1]);
		
		Log(-2) << "Payload delivered";
		
		std::string out;
		while (true) {
			out.resize(out.size()+CS);
			ssize_t r = read(linkB[0], &out[out.size()-CS], CS);
			Assert(r!=-1) << "buff...";
			out.resize(out.size()-CS+r);
			if (r<CS) break;
		}
		wait(NULL);
		return out;
	}
	
	std::string toTorch() && { // Limited to around 1GB only
	
		Assert(N) << "empty matrix";
		Assert(isContinuous()) << "matrix is not continuous";
		
		std::string fileName = std::tmpnam(nullptr);
		std::ofstream(fileName).write((char *)storage->data, nElem()*sizeof(T));
		storage.reset();
		
//		std::string command(R"(th -e "D=torch.FloatTensor(") D:apply(function() return io.read('*n') end) io.write(torch.serialize(D)) " )");
		
		std::ostringstream oss; oss << "th -e \"D=torch.FloatTensor(";
		for (size_t i=0; i<N; i++) oss << (i?",":"") << dims[i];
		oss << ") torch.DiskFile('" << fileName<< "','r'):binary():readFloat(D:storage()) ";
		oss << " io.write(torch.serialize(D))\"";
		std::string command = oss.str();
		
		Log(0) << command;
		
		constexpr size_t CS = 1<<20;
		
		int linkB[2];
		Assert(pipe(linkB)!=-1) << "Error creating pipe 2";
		pid_t pid;
		Assert((pid=fork()) !=-1) << "Error forking";
		if (pid==0) {
			dup2 (linkB[1], STDOUT_FILENO);
  			close(linkB[0]);
			execl("/bin/bash", "bash", "-c", command.c_str(), (char*)0);
			Assert(false) << "execl failed";
		}

		Log(-2) << "Delivering payload";
		close(linkB[1]);
		
		Log(-2) << "Payload delivered";
		
		std::string out;
		while (true) {
			out.resize(out.size()+CS);
			ssize_t r = read(linkB[0], &out[out.size()-CS], CS);
			Assert(r!=-1) << "buff...";
			out.resize(out.size()-CS+r);
			if (r<CS) break;
		}
		close(linkB[0]);
		wait(NULL);
		return out;
	}*/
	
	void toTorchHDF5(std::string fileNameH5, std::string tensorPath, std::string accessMode="a") && {
	
		Assert(N) << "empty matrix";
		Assert(isContinuous()) << "matrix is not continuous";

		// Todo: move to mkstemp
		std::string fileName = std::tmpnam(nullptr);
		std::ofstream(fileName).write((char *)storage->data, nElem()*sizeof(T));
		storage.reset();
		
		std::ostringstream oss; oss << "th -e \"D=torch.FloatTensor(";
		for (size_t i=0; i<N; i++) oss << (i?",":"") << dims[i];
		oss << ") torch.DiskFile('" << fileName<< "','r'):binary():readFloat(D:storage()) ";
		oss << " os.execute('rm " << fileName << "');";
		oss << " require 'hdf5';";
		oss << " hdf5.open('" << fileNameH5 << "', '"<< accessMode <<"'):write('" << tensorPath << "', D)\"";
		
		Log(0) << oss.str();
		
		Log(0) << system(oss.str().c_str());
	}
};

typedef Tensor_<float> Tensor1f;

}
