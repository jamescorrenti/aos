#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <ios>
#include <cmath>
#include "mapreduce_spec.h"

using std::string;

/* CS6210_TASK: Create your own data structure here, where you can hold information about file splits,
     that your master would use for its own bookkeeping and to convey the tasks to the workers for mapping */
struct FileShard {
	string start_file;
	int start_pos;
	string end_file;
	int end_pos;
	int shard_num;
};


/* CS6210_TASK: Create fileshards from the list of input files, map_kilobytes etc. using mr_spec you populated  */ 
inline bool shard_files(const MapReduceSpec& mr_spec, vector<FileShard>& fileShards) {
	vector<unsigned int> file_b;

	long map_b = 1024.0 * mr_spec.map_kb;

	int current_shard = 0;
	int file_sharded = 0;
	
	FileShard fs;

	fs.start_file = mr_spec.input_files.at(0).name;
	fs.start_pos = file_sharded;
	
	int i = 0;
	//fs.shard_num = i;
	int size_cur_file;
	

	// create the sizes of the shards
	for (vector<file>::const_iterator iter = mr_spec.input_files.begin(); iter != mr_spec.input_files.end(); ++iter){
		size_cur_file = iter->file_size;
		//std::cout << "Sharding " << iter->name << std::endl;
		while (true) {
			//std::cout << "hi " << std::endl;		
			if (file_sharded + ( map_b - current_shard) < size_cur_file ) {
				file_sharded += map_b - current_shard;

				std::ifstream file { iter->name, std::ios::binary | std::ios::ate };
				
				// find the closest new line
				int nl = -1;
				
				while (nl < 0) {
					//std::cout << "File sharded: " << file_sharded << " Size of file: " << size_cur_file << std::endl;
					char data[101];
					file.seekg(file_sharded, std::ios::beg);	
					//std::cout << "hi 1" << std::endl;
					file.read(data, 100);
					data[100] = '\n';

					for (int j = 0; j < 100; j++){
						if (data[j] == '\n') {
							nl = j;
							break;
						}
						// else std::cout << data[j];
					}
					if (nl == -1)
						file_sharded += 100;
				}
				file_sharded += nl;
				// std::cout << "bye" << std::endl;
				// std::cout << "Next new line: " << nl << std::endl;
				// std::cout << "Start File " << mr_spec.input_files.at(i) << " i: " << i << std::endl;	
				fs.end_file = iter->name;
				fs.end_pos = file_sharded;
				fs.shard_num = i;
				i++;
				fileShards.push_back(fs);

				fs.start_file = iter->name;					
				fs.start_pos = file_sharded;

				current_shard = 0;
				file.seekg(0, std::ios::beg);
				file.close();
			}

			else {
				current_shard += size_cur_file - file_sharded;
				file_sharded = 0;
				//std::cout << "Size of chard " << current_shard << std::endl;
				
				break;
			}	
		}
		file_sharded = 0;
	}

	fs.end_file = mr_spec.input_files.back().name;
	fs.end_pos =  mr_spec.input_files.back().file_size;
	fs.shard_num = i;
	fileShards.push_back(fs);
	
	for (vector<FileShard>::const_iterator iter = fileShards.begin(); iter != fileShards.end(); ++iter){
		std::cout << "Start File " << iter->start_file << std::endl;	
		std::cout << "Start Pos " << iter->start_pos << std::endl;
		std::cout << "End File "<< iter->end_file << std::endl;
		std::cout << "End Pos "<< iter->end_pos << std::endl;	
	}
	return true;
}
