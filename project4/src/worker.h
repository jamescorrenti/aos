#pragma once

#include <mr_task_factory.h>
#include <grpc++/grpc++.h>
#include <memory>
#include <fstream>

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

/* CS6210_TASK: Handle all the task a Worker is supposed to do.
	This is a big task for this project, will test your understanding of map reduce */

class Worker {

	public:
		/* DON'T change the function signature of this constructor */
		Worker(std::string ip_addr_port);

		/* DON'T change this function's signature */
		bool run();

	private:
		string ip_addr_port_;
		
		std::unique_ptr<Server> server_;

};

extern std::shared_ptr<BaseMapper> get_mapper_from_task_factory(const std::string& user_id);
extern std::shared_ptr<BaseReducer> get_reducer_from_task_factory(const std::string& user_id);

class MapReduceImpl final: public MapperReducer::Service {
	Status Map(ServerContext* context, const MapIn* request, MapEmit* response){
		std::cout << " Mapper Tworking" <<std::endl;
		
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
			string out = "output/int.txt";
			// string out_file = request->output() + "/" + r.file() + "-" + std::to_string(r.start_pos()) + "-" + std::to_string(r.end_pos());
			// std::cout << "Intermeddiate file " << out_file << std::endl;
			mapper->map(str);
			mapper->impl_->test();

			string *reply = response->add_output_file();
			//reply = &out_file;
			reply = &out;
		}

		return Status::OK;

	}

	Status Reduce(ServerContext* context, const ReduceIn* request, ReduceEmit* response){
		std::cout << "Reduce Tworking" <<std::endl;
		auto reducer = get_reducer_from_task_factory(request->user_id());
		reducer->reduce("dummy", std::vector<std::string>({"1", "1"}));
		return Status::OK;		
	}
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

	std::cout << "Tworking" <<std::endl;	
	ServerBuilder builder;
	MapReduceImpl service_;
	builder.AddListeningPort(ip_addr_port_, grpc::InsecureServerCredentials());
	builder.RegisterService(&service_);
	std::unique_ptr<Server> server_ {builder.BuildAndStart()};

	std::cout << "Server Listening on " << ip_addr_port_ << std::endl;
	server_->Wait();

	return true;
}
