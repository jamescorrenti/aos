#pragma once

#include <grpc++/grpc++.h>
#include <cstring>
#include <memory>
#include <grpc/support/log.h>
#include <map>
#include <chrono>

#include "mapreduce_spec.h"
#include "file_shard.h"

#include "masterworker.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;

using masterworker::MapperReducer;
using masterworker::MapIn;
using masterworker::MapEmit;
using masterworker::ReduceIn;
using masterworker::ReduceEmit;
using masterworker::Shard;

using std::make_pair;


/* CS6210_TASK: Handle all the bookkeeping that Master is supposed to do.
	This is probably the biggest task for this project, will test your understanding of map reduce */
class Master {

	public:
		/* DON'T change the function signature of this constructor */
		Master(const MapReduceSpec&, const std::vector<FileShard>&);

		/* DON'T change this function's signature */
		bool run();

	private:
		unsigned int workers_;
		vector<string> worker_address_;
		vector<file> input_files_;
		string output_dir_;
		unsigned int num_out_files_;
		unsigned int map_kb_;
		string user_id_;
		vector<FileShard> fileShards_;
		MapReduceSpec spec_; 
		vector<string> int_files;
		std::map<string,int> reduce_status;
		// key = worker, int1 = shard, int2 = status
		std::map<int,int> map_status;

};

class MapReduceClient {
public:
	explicit MapReduceClient(std::shared_ptr<Channel> channel) 
		: stub_(MapperReducer::NewStub(channel)) {}

	int map(const MapReduceSpec& spec, const FileShard fs, vector<string>& int_files) {
		MapIn request;

		request.set_user_id(spec.user_id);
		request.set_output(spec.output_dir);
		
		bool started = false;
		for (vector<file>::const_iterator iter = spec.input_files.begin(); iter != spec.input_files.end(); ++iter){
			
			if ( iter->name == fs.start_file ) {
				std::cout << "File " << iter->name << " starts this file shard at " << fs.start_pos << std::endl;
				started = true;
				Shard* rpc_shard = request.add_in_shards();

				rpc_shard->set_file(fs.start_file);
				rpc_shard->set_start_pos(fs.start_pos);	
				
				if (iter->name == fs.end_file) {
					rpc_shard->set_end_pos(fs.end_pos);
					break;
				}
				else rpc_shard->set_end_pos(iter->file_size);

			} else if (started){
				std::cout << "File " << iter->name << " is in this file shard" << std::endl;
				Shard* rpc_shard = request.add_in_shards();

				rpc_shard->set_file(iter->name);
				rpc_shard->set_start_pos(0);
				
				if (iter->name == fs.end_file) {
					rpc_shard->set_end_pos(fs.end_pos);
					break;
				} else rpc_shard->set_end_pos(iter->file_size);
			} else {
				std::cout << "File " << iter->name << " not in this file shard" << std::endl;
			}	
		}
		
		MapEmit reply;

		ClientContext context;
		CompletionQueue cq;
		Status status;// = stub_->Map(&context, request, &reply);

		std::unique_ptr<ClientAsyncResponseReader<MapEmit> > rpc(
			stub_->PrepareAsyncMap(&context, request, &cq));

		rpc->StartCall();
		rpc->Finish(&reply, &status, (void*)1);
		
		void* got_tag;
		bool ok = false;

		GPR_ASSERT(cq.Next(&got_tag, &ok));
		GPR_ASSERT(got_tag == (void *)1);

		GPR_ASSERT(ok);

		if (status.ok()){
			
			for(const auto r : reply.output_file()) {
				int_files.push_back(r);
			}

		} else {
			std::cout << "Error: "<< status.error_code() << " Details: " << status.error_details() << std::endl;
			return 1; 
		}
		return 0;

	}

	int reduce(const MapReduceSpec& spec, const string int_file,const string o_file) {
		std::cout << "Reducing" << std::endl;
		
		ReduceIn request;
		
		request.set_user_id(spec.user_id);
		
		request.set_out_file(o_file);
		request.set_int_file(int_file);
		//string i_file = request.add_int_file();
		//i_file->set_file(int_file);

		ReduceEmit reply;

		ClientContext context;
		CompletionQueue cq;
		Status status;

		std::unique_ptr<ClientAsyncResponseReader<ReduceEmit> > rpc(
			stub_->PrepareAsyncReduce(&context, request, &cq));

		rpc->StartCall();
		rpc->Finish(&reply, &status, (void*)1);
		
		void* got_tag;
		bool ok = false;

		GPR_ASSERT(cq.Next(&got_tag, &ok));
		GPR_ASSERT(got_tag == (void *)1);

		GPR_ASSERT(ok);

		if (status.ok()){
			std::cout << "We got a response from reducer" << std::endl;
		} else {
			std::cout << "Error: "<< status.error_code() << " Details: " << status.error_details() << std::endl;
			return 1;  
		}
		return 0;
	}

private:
	std::unique_ptr<MapperReducer::Stub> stub_;
};

/* CS6210_TASK: This is all the information your master will get from the framework.
	You can populate your other class data members here if you want */
Master::Master(const MapReduceSpec& mr_spec, const std::vector<FileShard>& file_shards):
	workers_ {mr_spec.workers}, worker_address_ {mr_spec.worker_address}, input_files_ {mr_spec.input_files}, 
	output_dir_ {mr_spec.output_dir}, num_out_files_ {mr_spec.num_out_files}, map_kb_ {mr_spec.map_kb},
	user_id_ {mr_spec.user_id}, fileShards_ {file_shards}, spec_ { mr_spec } { }

/* CS6210_TASK: Here you go. once this function is called you will complete whole map reduce task and return true if succeeded */
bool Master::run() {
	vector<string> status;
	
	for (auto fs: fileShards_){		
		
		std::cout << "Shard " << fs.shard_num << std::endl;
		for(auto client_name: worker_address_) {
			std::cout << "worker " << client_name << std::endl;
			map_status.insert(std::make_pair(fs.shard_num, 2)); // using 2 as busy
			MapReduceClient client(grpc::CreateChannel(client_name, grpc::InsecureChannelCredentials()));
			map_status.insert(std::make_pair(fs.shard_num, client.map(spec_, fs, int_files))); // 1 is failed, 0 is success
		}
	}

	bool leave = false;

	for (auto f: int_files) {
		std::cout << "Reducing " << f << std::endl;
		for(auto client_name: worker_address_) {
			reduce_status.insert(std::make_pair(f, 2)); // using 2 as busy
			MapReduceClient client(grpc::CreateChannel(client_name, grpc::InsecureChannelCredentials()));
			reduce_status.insert(std::make_pair(f, client.reduce(spec_, f,  f + "-out"))); // 1 is failed, 0 is success
		}
	}
	// wait till everyone is done
	
	return true;
}