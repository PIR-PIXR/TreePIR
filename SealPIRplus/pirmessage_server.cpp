#include <condition_variable>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"
#include "proto/pirmessage.grpc.pb.h"
#include "pir.hpp"
#include "pir_client.hpp"
#include "pir_server.hpp"
#include "helper.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using namespace seal;
using namespace std::chrono;
using std::chrono::system_clock;
using pir_message::PIRService;
using pir_message::RequestData;
using pir_message::ResponseData;
using pir_message::DBInfo;
using namespace std;

ABSL_FLAG(uint16_t, port, 3000, "Server port for the service");
#define SERVER_LOG_FILE  "server_log.txt"

class PIRServiceImpl final : public PIRService::Service {
 public:
   ~PIRServiceImpl() {      
      mapPIRDB.clear();
   }
   Status GetPIR(ServerContext* context, const RequestData* request,
                  ResponseData* reply) override {
   
    //Load encryption params
    EncryptionParameters enc_params;
    ServerResultLog log; 
    PIR_DB_Data* dbData;
   
    if (mapPIRDB.count(request->dbname()) > 0){
        dbData  = &(mapPIRDB[request->dbname()]);
    }else{
      reply->set_result(-1);
      reply->set_msg("Invalid databse");
      log.Msg = "Invalid databse " + request->dbname();
      cout<<"Invalid database "<<request->dbname();
      serverLogResult(SERVER_LOG_FILE,&log);
      return Status::OK; 
    }

    stringstream epbuf(request->pirconfig().epparams());
    enc_params.load(epbuf);

    uint64_t number_of_items = request->pirconfig().num_of_items();
    uint64_t size_per_item = request->pirconfig().size_per_item();
    uint32_t d = request->pirconfig().d();
    bool use_symmetric = request->pirconfig().use_symmetric();
    bool use_batching = request->pirconfig().use_batching();
    bool use_recursive_mod_switching = request->pirconfig().use_recursive_mod_switching();

    //generate pir param
    PirParams pir_params;
    gen_pir_params(number_of_items, size_per_item, d, enc_params, pir_params,
    use_symmetric, use_batching, use_recursive_mod_switching);
    PIRServer server(enc_params, pir_params);
    
    //get galois key
    string gkey = request->gkey();
    auto ctx = seal::SEALContext(enc_params);
    GaloisKeys galois_keys = SEALDeserialize(ctx,gkey);    
    server.set_galois_key(0, galois_keys);
   
   //prepare database
    auto db(make_unique<uint8_t[]>(number_of_items * size_per_item));
    for (size_t i = 0; i < dbData->Database.size(); ++i) {
        db.get()[i] = dbData->Database[i];
    }
   
    auto time_pre_s = high_resolution_clock::now();
    server.set_database(move(db), number_of_items, size_per_item);
    server.preprocess_database();
    auto time_pre_e = high_resolution_clock::now();
    auto time_pre_us = duration_cast<microseconds>(time_pre_e - time_pre_s).count();

    //get query from client
    std::stringstream client_stream;
    client_stream.str(request->query());

    ///// SERVER PROCESS query
    // Measure query deserialization (useful for receiving over the network)
    auto time_deserial_s = high_resolution_clock::now();
    PirQuery query2 = server.deserialize_query(client_stream);
    auto time_deserial_e = high_resolution_clock::now();
    auto time_deserial_us = duration_cast<microseconds>(time_deserial_e - time_deserial_s).count();
    // Measure query processing (including expansion)
    auto time_server_s = high_resolution_clock::now();
    // Answer PIR query from client 0. If there are multiple clients,
    // enter the id of the client (to use the associated galois key).
    PirReply replyQuery = server.generate_reply(query2, 0);
    auto time_server_e = high_resolution_clock::now();
    auto time_server_us = duration_cast<microseconds>(time_server_e - time_server_s).count();

    // Serialize reply (useful for sending over the network)
    stringstream server_stream;
    int reply_size = server.serialize_reply(replyQuery, server_stream);

    string ss = server_stream.str();
    reply->mutable_data()->assign(ss);
    reply->set_result(0);
    reply->set_msg("Successfully");
    db.reset();
    epbuf.clear();
    gkey.clear();
    log.Size = reply_size;
    log.Time = time_server_us;
    log.Msg =  "Done with client " + context->peer();
    serverLogResult(SERVER_LOG_FILE,&log);
    return Status::OK;
  }

  Status GetDBInfo(ServerContext* context,const pir_message::DBInfoRequest* request,pir_message::DBInfo* reply) override {
  string dbname = request->dbname();
  if (!fileExists(dbname)) {
      reply->set_num_of_items(0);
      reply->set_size_per_item(0);
      return Status::OK; 
  }
  if (mapPIRDB.count(request->dbname()) == 0){
    LoadDB(request->dbname());
  }
  PIR_DB_Data* dbData = &(mapPIRDB[request->dbname()]);
    reply->set_num_of_items(dbData->Number_of_items);
    reply->set_size_per_item(dbData->Size_per_item);
    return Status::OK;
  }

  void LoadDB(std::string jsonFile){
      Document doc;
      uint64_t number_of_items;
      uint64_t size_per_item;  
      PIR_DB_Data dbData;
      dbData.Database = get_seal_params(jsonFile,number_of_items,size_per_item,doc);
      doc.Clear();
      dbData.Number_of_items = number_of_items;
      dbData.Size_per_item = size_per_item;     
      mapPIRDB.insert({jsonFile,dbData});
  }
  private:    

  map<string,PIR_DB_Data> mapPIRDB;
};


void RunServer(uint16_t port) {
  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);
 
  PIRServiceImpl service;
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
  server_address.clear();  
  
}

//run program: ./pirmessage_server -port 3000
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  RunServer(absl::GetFlag(FLAGS_port));
 
  return 0;
}
