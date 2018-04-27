#pragma once

#include <mr_task_factory.h>
#include <grpc++/grpc++.h>
#include <memory>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <vector> 
#include "masterworker.grpc.pb.h"
#include "mr_tasks.h"

// grpc imports
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;

// Masterworker imports

using masterworker::MapperReducer;
using masterworker::MapIn;
using masterworker::MapEmit;
using masterworker::ReduceIn;
using masterworker::ReduceEmit;
using masterworker::Shard;

// other C++ libs
using std::string;
using std::vector;

inline vector<string> split(const string &s, char delim) {
	std::stringstream ss { s };
	string item;
	std::vector<string> token;
	while(getline(ss,item,delim)){
		token.push_back(item);
	}
	return token;
}

/* CS6210_TASK: Handle all the task a Worker is supposed to do.
	This is a big task for this project, will test your understanding of map reduce */

extern std::shared_ptr<BaseMapper> get_mapper_from_task_factory(const std::string& user_id);
extern std::shared_ptr<BaseReducer> get_reducer_from_task_factory(const std::string& user_id);

class Worker {

	public:
		/* DON'T change the function signature of this constructor */
		Worker(std::string ip_addr_port);

		/* DON'T change this function's signature */
		bool run();

	private:
		string ip_addr_port_;
		
		std::unique_ptr<Server> server_;

		class MapReduceImpl final: public MapperReducer::Service {
			Status Map(ServerContext* context, const MapIn* request, MapEmit* response){
				
				auto mapper = get_mapper_from_task_factory(request->user_id());
				
				vector<string> buffers;
				for (const auto r : request->in_shards()){
				
					int size = r.end_pos() - r.start_pos();
					
					vector<char> buff(size);

					std::ifstream in_file { r.file(), std::ios::binary | std::ios::ate };

					in_file.seekg(r.start_pos(), std::ios::beg);
					in_file.read(&buff[0], size);

					in_file.close();
					
					string str { buff.begin(), buff.end() };

					// find if there are any / ... this will prob fail if there is more than 1 /
					int pos = r.file().find('/');

					if( pos > r.file().length()) {
						pos = 0;
					} else {
						pos = pos + 1;	
					}

					// naming convention for outfile is <file_name>/start_pos-end_pos
					string out_file = request->output() + "/" + r.file().substr(pos) + "-" + std::to_string(r.start_pos()) + "-" + std::to_string(r.end_pos());
					mapper->impl_->intermediate_file = out_file;
					
					mapper->map(str);
					

					response->add_output_file(out_file);
				}
				

				return Status::OK;

			}

			Status Reduce(ServerContext* context, const ReduceIn* request, ReduceEmit* response){
				vector<string> in_token;
				auto reducer = get_reducer_from_task_factory(request->user_id());

				in_token = split(request->int_file(), ',');

				for(auto file_: in_token) {
					reducer->impl_->out_file = file_ + "-out.txt";
					if(file_.find("dummy") < file_.length() ) {
						reducer->reduce("", std::vector<std::string>({""}));
						return Status::OK;
					} 

					std::cout << file_ << std::endl;
					std::ifstream in_file { file_ };
					string s;
					std::map<string, int> reduce_map;
					vector<string> token;

					while(getline(in_file, s)){	
						if (!s.empty()) {
							token = split(s, ' ');
							string q = token.front();
							int b = std::stoi(token.back());

							auto it = reduce_map.find(q);
							
							if (it != reduce_map.end()) {
								it->second = it->second +  b;
							}
							else {
								reduce_map.emplace(q, b);
							}
						}
					}

					for (std::map<string,int>::iterator iter=reduce_map.begin(); iter != reduce_map.end(); ++iter){
						reducer->reduce(iter->first, std::vector<std::string>({std::to_string(iter->second)}));
					}
					remove(file_.c_str());
				}				
				return Status::OK;		
			}
		};
};


/* CS6210_TASK: ip_addr_port is the only information you get when started.
	You can populate your other class data members here if you want */
Worker::Worker(string ip_addr_port) {
	ip_addr_port_ = ip_addr_port;		
}

/* CS6210_TASK: Here you go. once this function is called your woker's job is to keep looking for new tasks 
	from Master, complete when given one and again keep looking for the next one.
	Note that you have the access to BaseMapper's member BaseMapperInternal impl_ and 
	BaseReduer's member BaseReducerInternal impl_ directly, 
	so you can manipulate them however you want when running map/reduce tasks*/
bool Worker::run() {	
	ServerBuilder builder;
	MapReduceImpl service_;
	builder.AddListeningPort(ip_addr_port_, grpc::InsecureServerCredentials());
	builder.RegisterService(&service_);
	std::unique_ptr<Server> server_ {builder.BuildAndStart()};

	std::cout << "Server Listening on " << ip_addr_port_ << std::endl;
	server_->Wait();

	return true;
}
