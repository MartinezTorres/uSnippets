////////////////////////////////////////////////////////////////////////
// fast online configuration file
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3
//
// GCC-FLAGS: -Ofast -std=c++0x `pkg-config opencv --cflags` `pkg-config opencv --libs`

#pragma once
#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem/operations.hpp>
#include <chrono>


namespace uSnippets {
struct Configuration {
	
	std::string configFileName;
	uint64_t    lastCheckTime;
	std::time_t lastConfigTime;
	boost::property_tree::ptree pTree;
	bool updated;
	
	Configuration(std::string configFileName) : configFileName(configFileName), lastConfigTime(0) {}
	
	static uint64_t now() {	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
	
	void update() {
		
		boost::filesystem::path p( configFileName ) ;
		if ( not lastConfigTime or lastCheckTime < now()-1000000) {
			
			lastCheckTime = now();
			if ( boost::filesystem::exists( p ) and boost::filesystem::last_write_time( p ) != lastConfigTime ) {

				lastConfigTime = boost::filesystem::last_write_time( p ) ;
				boost::property_tree::json_parser::read_json( configFileName, pTree );
				updated = true;
			} else {

				updated = false;
			}
		}
	}

	template <typename T>
	inline T operator()(const std::string &key, const T def) {
		
		update();
		
		std::string pkey = std::string("base.")+key;
		auto child = pTree.get_child_optional(pkey);
		if (child)
			return child->get_value<T>();
		
		pTree.put(pkey, def);
		boost::property_tree::json_parser::write_json( configFileName, pTree );

		return def;
	}
	
	template <typename T>
	inline void set(const std::string &key, const T val) {
		
		update();
		
		std::string pkey = std::string("base.")+key;
		auto child = pTree.get_child_optional(pkey);
		if (child and child->get_value<T>()==val)
			return;
		
		pTree.put(pkey, val);
		boost::property_tree::json_parser::write_json( configFileName, pTree );
	}
};
}
