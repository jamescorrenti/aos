#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <dirent.h>

using std::vector;
using std::string;

struct file {
	string name;
	int file_size;
};

/* CS6210_TASK: Create your data structure here for storing spec from the config file */
struct MapReduceSpec {
	unsigned int workers;
	vector<string> worker_address;
	vector<file> input_files;
	string output_dir;
	unsigned int num_out_files;
	unsigned int map_kb;
	string user_id;
};

// found string split @ ysonggit.github.io/coding/2014/12/16/split-a-string-using-c.html
inline vector<string> split(string &s, char delim) {
	std::stringstream ss { s };
	string item;
	std::vector<string> token;
	while(getline(ss,item,delim)){
		token.push_back(item);
	}
	return token;
}

inline bool read_mr_spec_from_config_file(const std::string& config_filename, MapReduceSpec& mr_spec) {
	string line;
	std::cout << "Reading from file "<< config_filename << std::endl;
	std::ifstream config_file {config_filename};
	vector<string> token;

	if(config_file.is_open()) {
		while(getline (config_file, line)) {
			token = split(line, '=');
			string d = token.front();	
			if (d.compare("n_workers") == 0) {
				mr_spec.workers = stoi(token.back());
			} else if (d.compare("map_kilobytes") == 0) {
				mr_spec.map_kb = stoi(token.back());
			
			} else if (d.compare("n_output_files") == 0) {
				mr_spec.num_out_files = stoi(token.back());
			
			} else if (d.compare("user_id") == 0) {
				mr_spec.user_id = token.back();				
			} else if (d.compare("output_dir") == 0) {
				mr_spec.output_dir = token.back();

			} else if (d.compare("worker_ipaddr_ports") == 0) {
				mr_spec.worker_address = split(token.back(), ',');

			} else if (d.compare("input_files") == 0) {

				vector<string> file_names = split(token.back(), ',');
				for(string name_: file_names){
					std::cout << "In file " << name_;
					file new_f;
					new_f.name = name_;
					std::ifstream f { name_, std::ios::binary | std::ios::ate };
					new_f.file_size = f.tellg();
					std::cout << " size " << new_f.file_size << std::endl;
					f.close();
					mr_spec.input_files.push_back(new_f);
				}

			} else {
				std::cout << "Invalid initializer"<< d  << ": " << token.back() << std::endl;
			}	
		}
		config_file.close();

	} else {
		return false;
	}
	
	return true;
}

inline bool validate_mr_spec(const MapReduceSpec& mr_spec) {
	//TODO: validate worker_ipaddr_ports, user_id (?) and max values for ints ??

	// check if all in files exist
	for (vector<file>::const_iterator iter = mr_spec.input_files.begin(); iter != mr_spec.input_files.end(); ++iter){
		std::ifstream test_file { iter->name};
		if (! (bool) test_file) return false;
		else test_file.close();
	}

	DIR* output_dir = opendir(mr_spec.output_dir.c_str());

	// check if output_dir exists
	if (!output_dir) return false;
	else closedir(output_dir);

	// check to see that all ints are greater than 0
	if (mr_spec.num_out_files < 1 || mr_spec.map_kb < 1 || mr_spec.workers < 1) return false;

	if (mr_spec.workers != mr_spec.worker_address.size()) return false;

	return true;
}
