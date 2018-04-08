#include "threadpool.h"

#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>

#include "store.grpc.pb.h"
#include "vendor.grpc.pb.h"

// GRPC imports to act as process requests from client
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;
using grpc::ServerCompletionQueue;

// RPC to communicate with vendor
using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

// GRPC imports to act request information from vendors
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

// RPC to communicate with client
using store::Store;
using store::ProductQuery;
using store::ProductReply;
using store::ProductInfo;

class StoreService final : public Store::Service {
public:
  //deconstructor
  ~StoreService(){
      server_->Shutdown();
      cq_->Shutdown();
    }

  // Call to start server
  void run (const std::string& server_address, const std::vector<std::string>& vendors, int threadpool_size) {
    ServerBuilder builder; 
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);

    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;	  
    HandleRpcs(vendors, threadpool_size);
  }

private:

  class ServerClient {
    public:
      explicit ServerClient(std::shared_ptr<Channel> channel) 
        : stub_(Vendor::NewStub(channel)) {}

      // async contact vendor and get price for the product
      ProductInfo QueryVendor(const std::string& product_name){	
        BidQuery bid_query_;
        BidReply bid_reply_;
        CompletionQueue bid_cq_;
        ClientContext bid_context_;
        Status bid_status_;
        ProductInfo bid_product_info;

        // set the product name for query to vendors
        bid_query_.set_product_name(product_name);
        
        // create async client listener
        std::unique_ptr<ClientAsyncResponseReader<BidReply> > rpc(
          stub_->PrepareAsyncgetProductBid(&bid_context_, bid_query_, &bid_cq_));
        
        // send rpc
        rpc->StartCall();

        rpc->Finish(&bid_reply_, &bid_status_, (void*)1);
        void* got_tag;
        bool ok = false;

        // wait until async call finished
        GPR_ASSERT(bid_cq_.Next(&got_tag, &ok));
        // ensure RPC executed properly
        GPR_ASSERT(got_tag == (void*)1);
        GPR_ASSERT(ok);

        if(bid_status_.ok()){
          // return product_info to calling function
          bid_product_info.set_vendor_id(bid_reply_.vendor_id());
          bid_product_info.set_price(bid_reply_.price());
          return bid_product_info;
        }
        else {
          std::cerr << "Error connecting to RPC" << std::endl;
          exit(1);
        }
		}

    private:
      std::unique_ptr<Vendor::Stub> stub_;
  };	

  class StoreData {
    public:
      StoreData(Store::AsyncService* service, ServerCompletionQueue* cq, const std::vector<std::string>& vendors) 
        : service_(service), prod_cq_(cq), responder_(&ctx_), status_(CREATE) {
        Proceed(vendors);
      }

      void Proceed(const std::vector<std::string>& vendors){
        if (status_ == CREATE) {
          status_ = PROCESS;
          service_->RequestgetProducts(&ctx_, &prod_query_, &responder_, prod_cq_, prod_cq_, this);
        }
        else if (status_ == PROCESS) {
          // create new StoreData object to handle other requests from clients
          new StoreData(service_, prod_cq_, vendors);

          // iterate through list of vendors
          for (std::vector<std::string>::const_iterator iter = vendors.begin(); iter != vendors.end(); ++iter){
            // set up rpc channel
            ServerClient client(grpc::CreateChannel(*iter, grpc::InsecureChannelCredentials()));
            
            // query vendor to get their price of the product
            ProductInfo result = client.QueryVendor(prod_query_.product_name());
            
            //get a new ProductInfo object
            ProductInfo *reply = prod_reply_.add_products();
            
            // set ProductInfo objects value
            reply->set_price(result.price());
            reply->set_vendor_id(result.vendor_id());
          }
          status_ = FINISH;
          
          // send response to client
          responder_.Finish(prod_reply_, Status::OK, this);
        } else {
          //cleanup
          GPR_ASSERT(status_ == FINISH);
          delete this;
        }
      }
    private:
      Store::AsyncService* service_;
      ServerCompletionQueue* prod_cq_;
      ServerContext ctx_;

      ProductQuery prod_query_;
      ProductReply prod_reply_;

      ServerAsyncResponseWriter<ProductReply> responder_;

      enum CallStatus { CREATE, PROCESS, FINISH };
      CallStatus status_;
  };
  	
  void HandleRpcs(const std::vector<std::string>& vendors, int threadpool_size){
    threadpool threadpool_(threadpool_size);
    new StoreData(&service_, cq_.get(), vendors);
    void* tag;
    bool ok;
    while(true) {
      GPR_ASSERT(cq_->Next(&tag, &ok));
      GPR_ASSERT(ok);

      // when request is received, repsonse is handled by thread pool
      threadpool_.work([tag, vendors] { 
        static_cast<StoreData*>(tag)->Proceed(vendors); 
      });
    }
  }
  std::unique_ptr<ServerCompletionQueue> cq_;
  Store::AsyncService service_;
  std::unique_ptr<Server> server_;
};

int main(int argc, char** argv) {
  std::string port;
  int threadpool_size;

  // CLI addressof service, max threads
  if (argc == 3) {
    port = argv[1];
    threadpool_size = std::max( -1, atoi(argv[2]));
  } else {
    std::cerr << "Correct usage: ./run_store $store_list_port $num_threads" << std::endl;
    return EXIT_FAILURE;
  }
    std::string filename = "vendor_addresses.txt";
    std::ifstream file(filename);
    std::vector<std::string> vendor_ip_addrresses;

    // Get list of vendors from file
    if (file.is_open()) {
      std::string ip_addr;
      while (getline(file, ip_addr)) {
        vendor_ip_addrresses.push_back(ip_addr);
      }
      file.close();
  }
  else {
    std::cerr << "Failed to open file " << filename << std::endl;
    return EXIT_FAILURE;
  }

  StoreService run_store;
  run_store.run("localhost:" + port, vendor_ip_addrresses, threadpool_size);
  return EXIT_SUCCESS;
}

