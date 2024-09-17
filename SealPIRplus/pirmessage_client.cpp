#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include <condition_variable>
#include "proto/pirmessage.grpc.pb.h"
#include "pir.hpp"
#include "pir_client.hpp"
#include "helper.hpp"
#include "safequeue.hpp"
#include "/home/quang/Desktop/vcpkg/installed/x64-linux/include/rapidjson/document.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using pir_message::PIRService;
using pir_message::RequestData;
using pir_message::ResponseData;
using pir_message::DBInfoRequest;

using namespace std::chrono;
using namespace std;
using namespace seal;
using namespace rapidjson;

#define CLIENT_LOG_FILE  "client_log.txt"

void clientLogResult(fstream *f,ClientResultLog* Log);

fstream fLogfile;
std::mutex log_mutex;

class ClientWorker {
 public:
  ClientWorker(std::shared_ptr<Channel> channel)
      : stub_(PIRService::NewStub(channel)) {}

  ClientWorker(std::shared_ptr<Channel> channel,std::string jsfile,uint64_t eIndex)
      : stub_(PIRService::NewStub(channel)),jsonDBFileName(jsfile),ele_index(eIndex) {

      }

  int GetDBInfo(pir_message::DBInfo &dbInfo) {
      ClientContext context;  
      DBInfoRequest requestDB;
      requestDB.set_dbname(jsonDBFileName);
      Status status =  stub_->GetDBInfo(&context,requestDB,&dbInfo);
      if (status.ok()) {
        //Log.Size = dbInfo.num_of_items();
        return 0;
      }else{
        return -1;
      }
  }
  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string GetPIRFromServer(PIRClient *pirClient,const RequestData& request) {        
    ClientContext context;
    ResponseData reply;
    
    Status status = stub_->GetPIR(&context,request,&reply);
    if (status.ok()) {      
      
      stringstream read_stream;
      read_stream.str(reply.data());
      /////////////////////////////////Client///////////////////////////////////////////
      // Read data from the binary file into the stringstream
      PirReply reply2 = pirClient->deserialize_reply(read_stream);

      // Measure response extraction
      auto time_decode_s = high_resolution_clock::now();
      vector<uint8_t> elems = pirClient->decode_reply(reply2, offset);
      auto time_decode_e = high_resolution_clock::now();
      Log.ExtTime = duration_cast<microseconds>(time_decode_e - time_decode_s).count();

      string decodedVal = VectorToHexString(elems);
      cout <<"Server response: "<<decodedVal << endl;
      Log.Msg = "Decode val: " + decodedVal;
      //validate with database
      //std::string result = ValidateResult(jsonDBFileName,elems,decodedVal);
      return "The strings are EQUAL.";
    } else {
      Log.Msg = "GetPIRFromServer failed. Please check connection";
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
  

  std::string ValidateResult(std::string jsonFile,const vector<uint8_t> &elems,const std::string &decodedVal){
      Document doc;
      uint64_t number_of_items;
      uint64_t size_per_item;

      get_seal_params(jsonFile,number_of_items,size_per_item,doc);

      cout <<"Validate result with database: ";
      assert(elems.size() == size_per_item);

      //validate
      const Value& obj = doc[0];
      Value::ConstMemberIterator it = obj.MemberBegin();
      std::advance(it, ele_index); // Move the iterator to the i-th position
      //string key = it->name.GetString();
      string value = it->value.GetString();

      cout << value << endl;

      int result = decodedVal.compare(value);     
      if (result == 0) {
          return "The strings are EQUAL.";
      } else {
          return "The strings are NOT equal.";
      }    
  }

  
  int PrepareData(uint64_t number_of_items,uint64_t size_per_item,PIRClient* &pirClient,RequestData &requestData){
    
    uint32_t logt = 20;
    uint32_t d = 2;
    bool use_symmetric = true; // use symmetric encryption instead of public key
    // (recommended for smaller query)
    bool use_batching = true;  // pack as many elements as possible into a BFV
    // plaintext (recommended)
    bool use_recursive_mod_switching = true;
   
    auto ecp = loadFileToString("encryption_parameters.bin");
    if (ecp.size() ==0) {
      return -1;
    }
   
    EncryptionParameters enc_params = load_encryption_parameters_from_string(ecp);
    //cout << "Main: Verifying SEAL parameters" << endl;
    verify_encryption_params(enc_params);

    PirParams pir_params;
    gen_pir_params(number_of_items, size_per_item, d, enc_params, pir_params,
      use_symmetric, use_batching, use_recursive_mod_switching);
   
    print_seal_params(enc_params);
    print_pir_params(pir_params);
    pirClient = new PIRClient(enc_params, pir_params);
    //cout << "Main: Generating galois keys for client" << endl;
    GaloisKeys galois_keys = pirClient->generate_galois_keys();

    string gkey = SEALSerialize(galois_keys);
    
    //Generate query
    uint64_t index = pirClient->get_fv_index(ele_index);   // index of FV plaintext
    offset = pirClient->get_fv_offset(ele_index); // offset in FV plaintext
    cout << "Main: element index = " << ele_index << " from [0, "
    << number_of_items - 1<< "]" << endl;

    //Query generation
    auto time_query_s = high_resolution_clock::now();
    PirQuery query = pirClient->generate_query(index);
    auto time_query_e = high_resolution_clock::now();
    Log.QueryTime = duration_cast<microseconds>(time_query_e - time_query_s).count();

    //Measure serialized query generation (useful for sending over the network)
    stringstream client_stream;
    auto time_s_query_s = high_resolution_clock::now();
    int query_size = pirClient->generate_serialized_query(index, client_stream);
    auto time_s_query_e = high_resolution_clock::now();
    auto time_s_query_us =
    duration_cast<microseconds>(time_s_query_e - time_s_query_s).count();
    Log.QuerySize = query_size;
    
    requestData.set_clientid(1);
    requestData.set_requestid(1);       
    requestData.mutable_pirconfig()->set_use_symmetric(use_symmetric);
    requestData.mutable_pirconfig()->set_use_batching(use_batching);
    requestData.mutable_pirconfig()->set_use_recursive_mod_switching(use_recursive_mod_switching);
    requestData.mutable_pirconfig()->set_d(2);
    requestData.mutable_pirconfig()->set_num_of_items(number_of_items);
    requestData.mutable_pirconfig()->set_size_per_item(size_per_item);

    requestData.mutable_pirconfig()->mutable_epparams()->assign(ecp);
    requestData.set_gkey(gkey);
    requestData.set_query(client_stream.str());
    requestData.set_dbname(this->jsonDBFileName);
   
    return 0;
}

 
 private:
  std::unique_ptr<PIRService::Stub> stub_;
  uint64_t ele_index;
  uint64_t offset;
  std::string jsonDBFileName;
public:
  ClientResultLog Log;
};


void requestThread(vector<ClientRequestInfo>::iterator requestInfo)//boost::shared_ptr<ClientRequestInfo> requestInfo)//
{
    cout<<"Start client"<<endl;
    ClientWorker client(grpc::CreateChannel(requestInfo->hostAddr, grpc::InsecureChannelCredentials()),requestInfo->jsDBFile,requestInfo->eIndex);
    pir_message::DBInfo dbInfo;
    requestInfo->print();
    client.Log.Server = requestInfo->hostAddr;
    if (client.GetDBInfo(dbInfo) >=0) {
        //cout<<"Get db info successfully "<<dbInfo.num_of_items()<<endl;
        RequestData requestData;
        PIRClient *pirClient = NULL;
        if (client.PrepareData(dbInfo.num_of_items(),dbInfo.size_per_item(),pirClient,requestData) >=0) {
            
            client.GetPIRFromServer(pirClient,requestData);
            
        }else{
                      
          client.Log.Msg = "Prepare data error. Cannot get encription parameters.";
         
        }
        if (pirClient != NULL) {
            delete pirClient;
        }
        requestData.Clear();
    }else{       
        client.Log.Msg = "Cannot get database information";    
    }
    clientLogResult(&fLogfile,&client.Log);
}



int main(int argc, char** argv) {
    // Get list server
    std::vector<ClientRequestInfo> listServers = getListServers("servers_list.txt");
    int i, j = 0;

    int num_test = 1;

    int total = listServers.size();
    int num_servers = total / num_test;

    std::cout << "num_servers = " << num_servers << std::endl;

    ClientWorker* arrayClient[total];
    pir_message::DBInfo dbInfo[total];

    RequestData requestData[total];
    PIRClient* pirClient[total];

    int resGetDBInfo[num_servers];
    int resGenQuery[num_servers];
    int resPIRAnsFromServer[num_servers];
    fLogfile.open(CLIENT_LOG_FILE, ios::app);

    for (auto& requestInfo : listServers) {
        requestInfo.print();
        if (j < total) {
            arrayClient[j] = new ClientWorker(grpc::CreateChannel(requestInfo.hostAddr, grpc::InsecureChannelCredentials()), requestInfo.jsDBFile, requestInfo.eIndex);
            arrayClient[j]->Log.Server = requestInfo.hostAddr;
            j++;
        }
    }

    for (int k = 0; k < num_test; k++) {
        std::cout << "================ BEGIN =============" << std::endl;

        // Get DBInfo
        for (i = 0; i < num_servers; i++) {
            resGetDBInfo[i] = arrayClient[i + k * num_servers]->GetDBInfo(dbInfo[i + k * num_servers]);
        }

        // Generate Query
        for (i = 0; i < num_servers; i++) {
            if (resGetDBInfo[i] >= 0) {
                resGenQuery[i] = arrayClient[i + k * num_servers]->PrepareData(dbInfo[i + k * num_servers].num_of_items(), dbInfo[i + k * num_servers].size_per_item(), pirClient[i + k * num_servers], requestData[i + k * num_servers]);
            } else {
                resGenQuery[i] = -1; // or some error code indicating failure
            }
        }

        // Get PIR from the server
        for (i = 0; i < num_servers; i++) {
            if (resGenQuery[i] >= 0) {
                arrayClient[i + k * num_servers]->GetPIRFromServer(pirClient[i + k * num_servers], requestData[i + k * num_servers]);
                clientLogResult(&fLogfile, &arrayClient[i + k * num_servers]->Log);
            }
        }

        std::cout << "================ END ===============" << k << std::endl;
    }

    // Free memory
    for (i = 0; i < total; i++) {
        delete arrayClient[i];
        delete pirClient[i];
        requestData[i].Clear();
    }

    fLogfile.close();
    listServers.clear();

    return 0;
}
