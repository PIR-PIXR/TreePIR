#ifndef __HELPER__
#define __HELPER__
#include <iostream>
#include <fstream>
#include <mutex>
#include <boost/asio.hpp>
#include "pir.hpp"
#include "pir_client.hpp"
#include "pir_server.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <seal/seal.h>
#include "/home/quang/Desktop/vcpkg/installed/x64-linux/include/rapidjson/document.h"

using namespace seal;
using namespace rapidjson;
extern std::mutex syncLogMutex;

struct ClientRequestInfo {
    uint64_t eIndex;
    std::string hostAddr;
    std::string jsDBFile;
    void print(){
        cout<<"host: "<<hostAddr<<", db: "<<jsDBFile<<", element index: "<<eIndex<<endl;
    }
    void clear(){
        hostAddr.clear();
        jsDBFile.clear();
    }
};

class PIR_DB_Data {
public:
    PIR_DB_Data(){
        Number_of_items = 0;
        Size_per_item = 0;
    }
    ~PIR_DB_Data(){
        Database.clear();
    }
public:
    uint64_t Number_of_items;
    uint64_t Size_per_item;
    std::vector<uint8_t> Database;
};


typedef struct ServerResultLog {
  
  uint64_t Size;
  uint64_t Time;
  string Msg;

    void log(fstream *f){
        time_t curTime = time(NULL);
        *f << "Start Time: " << ctime(&curTime);
        *f << "msg: " << Msg << endl;
        *f << "reply_size: " << Size << " (bytes)" << endl;
        *f << "reply_time: " << Time << " (us)" << endl;
        *f << "End ---------------------------"<<endl;
    }
}ServerResultLog;

typedef struct ClientResultLog {
  uint64_t QuerySize;
  uint64_t QueryTime;
  uint64_t ExtTime;
  string Server;
  string Msg;

    void log(fstream *f){
        time_t curTime = time(NULL);
        *f << "Start Time: " << ctime(&curTime);
        *f << "Host: " << Server << endl;
        *f << "msg: " << Msg << endl;
        *f << "query_size: " << QuerySize << " (bytes)" << endl;
        *f << "query_time: " << QueryTime << " (us)" << endl;
        *f << "extract_time: " << ExtTime << " (us)" << endl;
        *f << "End ---------------------------" << endl;
    }
}ClientResultLog;

void Write(const string& filename, stringstream& dataStream) ;
void Read(const string& filename, stringstream& dataStream);
string VectorToHexString(const vector<uint8_t>& elems) ;
EncryptionParameters load_encryption_parameters(const string &filename);
EncryptionParameters load_encryption_parameters_from_buffer(const std::vector<uint8_t>  &buffer);
std::vector<uint8_t>  loadFileToBuffer(const string &filename);
EncryptionParameters load_encryption_parameters_from_string(const string  &buffer);
std::string loadFileToString(const string &filename);
void save_pir_parameters(const PirParams &pir_params, const string &filename);
std::vector<uint8_t>  get_seal_params(string jsonFile,uint64_t &number_of_items,uint64_t &size_per_item,Document &doc);
std::unique_ptr<uint8_t[]>  readDatabase(string jsonFile,uint64_t &number_of_items,uint64_t &size_per_item);
PirParams load_pir_parameters(const string &filename);
PIRClient create_pir_params(string encryptionFile,PirParams &pir_params,uint64_t number_of_items,uint64_t size_per_item);
int parseRequestInfo(const std::string &str,ClientRequestInfo &clientRequestInfo);
std::vector<ClientRequestInfo> getListServers(const string &filename);
bool fileExists (const std::string& name);
void clearLogFile(const std::string& name);
void clientLogResult(fstream *f,ClientResultLog* logResult);
void serverLogResult(const string logFileName,ServerResultLog* logResult);
#endif
