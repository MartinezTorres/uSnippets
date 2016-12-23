#pragma once

struct Executer {
	
	typedef std::lock_guard<std::mutex> Lock;
	
	std::shared_ptr<std::mutex> mtx = std::make_shared<std::mutex>();

	std::map<std::string, std::future<std::string>> instances;
	
	std::future<std::string> &async(const object &id, const std::string &command, const std::string &in = "") {

		auto mtx2 = mtx;
		instances.erase(id);
		instances.emplace(id, std::async(std::launch::async, [mtx2, command, in](){
			Lock l(*mtx2);
			ofstream("/tmp/execFast") << in; 
			Log(-2) << system("sync");
			Log(-2) << system((command+" < /tmp/execFast > /tmp/execFast.out").c_str());
			stringstream ss; ss << ifstream("/tmp/execFast.out").rdbuf();
			return ss.str();
		}));
		return instances.find(id)->second;		
	}
	
	std::string get(const object &id) { return instances.find(id)->second.get(); }
	bool isFree(const object &id) { return instances.find(id) == instances.end() or not instances.find(id)->second.valid(); }
	bool isReady(const object &id) { 
		return instances.find(id) != instances.end() 
			and instances.find(id)->second.valid()
			and instances.find(id)->second.wait_for(uS(0)) == std::future_status::ready; 
	}

	std::string sync(const string &command, const string &in = "") {
			
		Lock l(*mtx);	
		redi::pstream commandStream(command);
		std::string out;
		std::thread t([&](){
			while (not commandStream.eof()) {
				out.resize(out.size()+(1<<15));
				std::this_thread::yield();
				commandStream.read(&out[out.size()-(1<<15)], (1<<15));
				out.resize(out.size()-(1<<15)+commandStream.gcount());
			}
		});
		for (uint i=0; i<in.size(); i+= (1<<15)) { commandStream.write(&in[i], min(int(in.size()-i),1<<15)); commandStream.flush(); std::this_thread::yield(); }
		commandStream << redi::peof;
		t.join();
		return out;
	}
};
