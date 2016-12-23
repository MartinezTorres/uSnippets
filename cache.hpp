////////////////////////////////////////////////////////////////////////
// fast cache
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once
#include <uSnippets/object.hpp>
#include <uSnippets/serializer.hpp>

#include <boost/noncopyable.hpp>

#include <fstream>
#include <list>
#include <map>
#include <unordered_map>

#include <mutex>
#include <limits>

namespace uSnippets {
class GenericCache : private boost::noncopyable {

	typedef std::lock_guard<std::recursive_mutex> Lock;
	std::recursive_mutex m;
	std::string filename;
	std::fstream file;
		
	struct Chunk {

		std::string key;
		uint64_t count;
		size_t dataSize;
		
		size_t size;
		size_t pos;
		size_t dataPos;
		std::multimap<size_t, std::list<Chunk>::iterator>::iterator holeIt; // Hole BEFORE the actual chunk
		
		template<class Archive> void serialize(Archive &ar, const uint) { ar & key & count & dataSize; }
	};
	
	std::list<Chunk> chunks; //All chunks sorted by disk state
	std::unordered_map<std::string, std::list<Chunk>::iterator> index; //Dictionary from the name to the valid chunk
	std::multimap<size_t, std::list<Chunk>::iterator> holes; //All holes and its related chunk (the hole is before the chunk)

	void updateHole(std::list<Chunk>::iterator it) { // Update the hole between this chunk and the previous one.

		Lock lock(m); 
		if (it==chunks.end()) return;
		
		if (it->holeIt != holes.end()) holes.erase(it->holeIt);
		
		int64_t holeSize = it->pos; //may be temporally negative when resizing the index
		if (it != chunks.begin()) holeSize -= std::prev(it)->pos + std::prev(it)->size;

		it->holeIt = (holeSize>0?holes.insert({holeSize, it}):holes.end());
	}
	
	uint64_t freeAndGetCount(const std::string &key) {

		Lock lock(m); 
		auto count = 0;
		auto indexIt = index.find(key);
		if (indexIt != index.end()) {
			
			if (indexIt->second->holeIt != holes.end())  holes.erase(indexIt->second->holeIt);

			count = indexIt->second->count;
			updateHole(chunks.erase(indexIt->second));
			index.erase(indexIt);
		}
		return count;
	}
	
	bool getRAW(const std::string &key, std::string &sdata) { 
		
		if (not file.is_open()) return false; 
		
		auto indexIt = index.find(key);
		if (indexIt == index.end()) return false;
		Chunk &chunk = *indexIt->second;
		
		sdata.resize(chunk.dataSize);
//		file.sync(); 
		Lock lock(m);		
		file.seekg(chunk.dataPos);		
		file.read(&sdata[0], sdata.size());
		return true;
	}

	void setRAW(const std::string &key, const std::string &sdata) { 
		
		Lock lock(m);
		if (not file.is_open()) return; 

		invalidateIndex();

		uint64_t count = freeAndGetCount(key);

		Chunk chunk;
		chunk.key = key;
		chunk.count = count+1;
		chunk.dataSize = sdata.size();
		chunk.holeIt = holes.end();
		
		std::string sheader = Serializer::serialize(chunk);
		chunk.size = sheader.size() + chunk.dataSize + 1;

		auto holeIt = holes.upper_bound(chunk.size);
		auto chunkIt = chunks.end();
		if (holeIt != holes.end()) {
			
			chunk.pos = holeIt->second->pos - holeIt->first; //store this chunk next to the previous one
			chunkIt = chunks.insert(holeIt->second, chunk);
			updateHole(std::next(chunkIt));			
		} else if (not chunks.empty()) {
			chunk.pos = chunks.rbegin()->pos + chunks.rbegin()->size; //store this chunk next to the last one
			chunkIt = chunks.insert(chunks.end(), chunk);

		} else {
			chunk.pos = 0; //store this chunk first
			chunkIt = chunks.insert(chunks.end(), chunk);
		}

		chunkIt->dataPos = chunkIt->pos + sheader.size();

		if (chunk.pos) { file.seekp(chunk.pos-1); file.put('\n'); }
		file.seekp(chunk.pos);
		file << sheader << sdata;
		if (std::next(chunkIt) != chunks.end())
			file << std::string(std::next(chunkIt)->pos-file.tellp()-1,'-') << '\n';
		else 
			file << '\n' << Serializer::serialize(Chunk()) << '\n';
		file << std::flush;

		index.emplace(key, chunkIt);
	}
	
	
	bool indexed = false;
	void invalidateIndex() {
		
		Lock lock(m); 
		if (not indexed) return;
		file.seekp(-1,file.end); file.put('!');
		indexed = false;
	}
	
	bool getIndex() {

		Lock lock(m); 
		try {
		
			file.seekg (-1, file.end);
			if (file.get()!='#') return false;
			file.seekg (-20, file.end);
			size_t pos=0;
			for (int i=0; i<19; i++) pos = pos*10+file.get()-'0';
			file.sync(); 
			file.seekg(pos);
			
			uint64_t nChunks=0;
			Serializer::unserialize(file, nChunks);
			for (uint64_t i=0; i<nChunks; i++) {
				Chunk chunk;
				Serializer::unserialize(file, chunk);
				Serializer::unserialize(file, chunk.pos);
				Serializer::unserialize(file, chunk.size);
				chunk.holeIt = holes.end();
				chunk.dataPos = chunk.pos + (chunk.size - chunk.dataSize - 1);
				index.emplace(chunk.key, chunks.insert(chunks.end(), chunk));
			}
			for (auto it = chunks.begin(); it!=chunks.end(); it++) updateHole(it);
		} catch (std::istream::failure) { file.clear(); return false; }
		return true;
	}
	
	bool writeIndex() {
		
		Lock lock(m); 
		if (chunks.empty()) return false;
		try {
			
			std::string sindex;
			sindex += Serializer::serialize(index.size());
			for (auto &chunk : chunks) {
				sindex += Serializer::serialize(chunk);
				sindex += Serializer::serialize(chunk.pos);
				sindex += Serializer::serialize(chunk.size);
			}
			setRAW("__index", sindex);
			
			size_t pos = index["__index"]->dataPos;
			
			std::string spos = "0000000000000000000#";
			for (int i=18; i>=0; i--) { spos[i]+= pos%10; pos /= 10; }
			
			// Do not blindly write it at the end. Filesize would increase strongly. 
			file.seekp(0,file.end);
			file.seekp (std::max(int64_t(chunks.back().pos+chunks.back().size+4), int64_t(file.tellp())-20));
			file << spos << std::flush;

			freeAndGetCount("__index");
		} catch (std::istream::failure) { file.clear(); return false; }
		return true;
	}
	
public:	

	GenericCache(      GenericCache &&) = default; GenericCache& operator=(      GenericCache &&) = default;

	GenericCache(std::string filename) : filename(filename) {
				
		Lock lock(m);
		
		if (filename.empty()) return;

		// Create file if needed
		std::fstream(filename, std::ios_base::out | std::ios_base::app);
		
		file.open(filename, file.in | file.out | file.binary);
		if (not file.is_open()) return;

		indexed = getIndex();
		if (indexed) return;
		
		// Rebuild Index
		file.seekg(0);
		while (true) {
			size_t pos = file.tellg();
			Chunk chunk;
			if (not Serializer::unserialize(file, chunk) or chunk.count==0) break;

			chunk.pos = pos;
			chunk.dataPos = file.tellg();
			chunk.size = (chunk.dataPos-chunk.pos) + chunk.dataSize + 1;
			chunk.holeIt = holes.end();
			
			file.seekg(chunk.pos+chunk.size-1);
			file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
			
			auto found = index.find(chunk.key);
			if (found==index.end()) {
				index.emplace(chunk.key, chunks.insert(chunks.end(), chunk));
			} else if (found->second->count<chunk.count) {
				chunks.erase(found->second);
				found->second = chunks.insert(chunks.end(), chunk);
			}
		}
		file.clear();
		for (auto it = chunks.begin(); it!=chunks.end(); it++) updateHole(it);
		
		indexed = writeIndex();
	}
	
	virtual ~GenericCache() { 
		if (not indexed)
			indexed = writeIndex();
	}
	
	template<class T>
	bool get(const std::string &key, T &t) { std::string str; return getRAW(key, str) and Serializer::unserialize<T>(str,t);}

	bool get(const std::string &key, std::string &str) { return getRAW(key, str); }

	template<class T>
	void set(const std::string &key, const T &t) { setRAW(key, Serializer::serialize(t)); }

	void set(const std::string &key, const std::string  &t) { setRAW(key, t); }
	
	void purge() { 
		
		Lock lock(m);
		chunks.clear();
		index.clear();
		holes.clear();
		indexed = false;
		
		file.close();
		file.open(filename, file.in | file.out | file.binary | file.trunc);

		file << Serializer::serialize(Chunk()) << std::flush;
	}
	
	std::vector<std::string> getKeys() {
		
		Lock lock(m); 
		std::vector<std::string> keys;
		for (auto &c : chunks) keys.push_back(c.key);
		//std::sort(keys.begin(), keys.end());
		return keys;
	}

	void free(const std::string &key) { Lock lock(m); freeAndGetCount(key); }

	size_t usage() {
		
		Lock lock(m); 
		size_t ret = 0;
		for (auto &chunk : chunks)
			ret += chunk.size;
		return ret;
	}
};
}


