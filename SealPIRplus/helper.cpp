#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <seal/seal.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string.h>
#include <thread>
#include <mutex>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include "/home/quang/Desktop/vcpkg/installed/x64-linux/include/rapidjson/document.h"
#include "pir.hpp"
#include "pir_client.hpp"
#include "pir_server.hpp"
#include "helper.hpp"

using namespace seal;
using namespace rapidjson;
std::mutex syncLogMutex;

// Function to write data to a binary file from a stringstream
void Write(const string& filename, stringstream& dataStream) {
  // Open the binary file for writing
  ofstream file(filename, std::ios::binary);

  // Get the data from the stringstream as a string
  string data = dataStream.str();

  // Write the data to the binary file
  file.write(data.c_str(), data.size());

  // Close the file
  file.close();
}

// Function to read data from a binary file into a stringstream
void Read(const string& filename, stringstream& dataStream) {
  // Open the binary file for reading
  ifstream inputFile(filename, std::ios::binary);

  // Read the data from the binary file into the stringstream
  dataStream << inputFile.rdbuf();

  // Close the input file
  inputFile.close();
}

string VectorToHexString(const vector<uint8_t>& elems) {
  stringstream s;
  s << hex << setfill('0') << uppercase;

  for (const uint8_t& elem : elems) {
    s << setw(2) << static_cast<int>(elem);
  }

  return s.str();
}

void save_encryption_parameters(const EncryptionParameters &enc_params, const string &filename) {
  try {
    ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
      // Serialize the encryption parameters to the file
      enc_params.save(file);
      file.close();
      //cout << "Encryption parameters saved to " << filename << endl;
    } else {
      cerr << "Error: Unable to open file for writing." << endl;
    }
  } catch (const exception &e) {
    cerr << "Exception: " << e.what() << endl;
  }
}

EncryptionParameters load_encryption_parameters(const string &filename) {
  EncryptionParameters enc_params;

  try {
    std::ifstream file(filename, std::ios::binary);
    if (file.is_open()) {
      // Deserialize the encryption parameters from the file
      enc_params.load(file);
      file.close();
      //cout << "Encryption parameters loaded from " << filename << endl;
    } else {
      cerr << "Error: Unable to open file for reading." << endl;
    }
  } catch (const exception &e) {
    cerr << "Exception: " << e.what() << endl;
  }

  return enc_params;
}
EncryptionParameters load_encryption_parameters_from_buffer(const std::vector<uint8_t> &buffer) {
    EncryptionParameters enc_params;
    std::stringstream loaded_stream;
    for (uint8_t byte : buffer) {
        loaded_stream.put(byte);
    }
    loaded_stream.seekg(0);
    enc_params.load(loaded_stream);
    return enc_params;
}

std::vector<uint8_t>  loadFileToBuffer(const string &filename) {
  //EncryptionParameters enc_params;
    try{
        ifstream file(filename);
        // Read the entire file into a string
        string str((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        std::vector<uint8_t> vec(str.begin(), str.end());
        str.clear();
        return vec;
    }catch(const exception &e){
        cerr << "Error: Unable to open file for reading." << endl;
    }   
    return std::vector<uint8_t>();
}


std::string loadFileToString(const string &filename){
  try{
        ifstream file(filename);
        // Read the entire file into a string
        string str((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());       
        return str;
    }catch(const exception &e){
        cerr << "Error: Unable to open file for reading." << endl;
    }   
    return "";
}

EncryptionParameters load_encryption_parameters_from_string(const string  &buffer){
    EncryptionParameters enc_params;
    std::stringstream loaded_stream(buffer);
    enc_params.load(loaded_stream);
    return enc_params;
}

void save_pir_parameters(const PirParams &pir_params, const string &filename) {
  try {
    ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
      // Serialize the PirParams structure to the file
      file.write(reinterpret_cast<const char*>(&pir_params), sizeof(PirParams));
      file.close();
      //cout << "PirParams saved to " << filename << endl;
    } else {
      cerr << "Error: Unable to open file for writing." << endl;
    }
  } catch (const exception &e) {
    cerr << "Exception: " << e.what() << endl;
  }
}

std::vector<uint8_t> get_seal_params(string jsonFile,uint64_t &number_of_items,uint64_t &size_per_item,Document &doc) {    
    //jsonFile = "layer1.json";
    ifstream file(jsonFile);
    // Read the entire file into a string
    string json((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // Close the file
    file.close();

    // Parse the JSON data
    doc.Parse(json.c_str());
    json.clear();
    const Value& obj = doc[0];

    number_of_items = obj.MemberCount();
    string str = obj.MemberBegin()->value.GetString();
    size_per_item = str.length() / 2; // in bytes
    uint32_t N = 2048;
    str.clear();
    //////////////////////////////////////////////SERVER////////////////
    vector<uint8_t> db(number_of_items * size_per_item);
    vector<uint8_t> db_copy(number_of_items * size_per_item);
    // Check if the JSON is an array
    if (doc.IsArray()) {
    uint64_t i = 0;
    string hexString; // Initialize an empty string

    // Loop through the object's members
    for (Value::ConstMemberIterator it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
      //cout << "Key: " << it->name.GetString() << ", Value: " << it->value.GetString() << endl;
      hexString = it->value.GetString();
      //number_of_items++;
      for (uint64_t j = 0; j < size_per_item; j++) {
        string byteStr = hexString.substr(j * 2, 2); // Get two characters at a time
        uint8_t val = stoul(byteStr, nullptr, 16); // Convert hex string to uint8_t
        db[(i * size_per_item) + j] = val;
        db_copy[(i * size_per_item) + j] = val;       
      }
      i++;
    }
    
  }
  else {
    cerr << "JSON is not an array." << endl;   
    return vector<uint8_t>();
  }
  return db;
}

std::unique_ptr<uint8_t[]>  readDatabase(string jsonFile,uint64_t &number_of_items,uint64_t &size_per_item) {
  // Open the JSON file
  ifstream file(jsonFile);
  // Read the entire file into a string
  string json((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

  // Close the file
  file.close();

  // Create a Document object to hold the JSON data
  Document doc;
    
  // Parse the JSON data
  doc.Parse(json.c_str());
  json.clear();
  const Value& obj = doc[0];

  number_of_items = obj.MemberCount();
  string str = obj.MemberBegin()->value.GetString();
  size_per_item = str.length() / 2; // in bytes
  uint32_t N = 2048;
  str.clear();
  //////////////////////////////////////////////SERVER////////////////
   auto db(make_unique<uint8_t[]>(number_of_items * size_per_item));
 
    // Check if the JSON is an array
    if (doc.IsArray()) {
    uint64_t i = 0;
    string hexString; // Initialize an empty string

    // Loop through the object's members
    for (Value::ConstMemberIterator it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
      //cout << "Key: " << it->name.GetString() << ", Value: " << it->value.GetString() << endl;
      hexString = it->value.GetString();
      for (uint64_t j = 0; j < size_per_item; j++) {
        string byteStr = hexString.substr(j * 2, 2); // Get two characters at a time
        uint8_t val = stoul(byteStr, nullptr, 16); // Convert hex string to uint8_t
        db.get()[(i * size_per_item) + j] = val;    
      }
      i++;
    }
    doc.Clear();
  }
  else {
    cerr << "JSON is not an array." << endl;
    doc.Clear();
    return NULL;
  }
  return db;
}

PirParams load_pir_parameters(const string &filename) {
  PirParams pir_params;

  try {
    ifstream file(filename, std::ios::binary);
    if (file.is_open()) {
      // Deserialize the PirParams structure from the file
      file.read(reinterpret_cast<char*>(&pir_params), sizeof(PirParams));
      file.close();
      //cout << "PirParams loaded from " << filename << endl;
    } else {
      cerr << "Error: Unable to open file for reading." << endl;
    }
  } catch (const exception &e) {
    cerr << "Exception: " << e.what() << endl;
  }

  return pir_params;
}

PIRClient create_pir_params(string encryptionFile,PirParams &pir_params, uint64_t number_of_items,uint64_t size_per_item){
  // Recommended values: (logt, d) = (20, 2).
  uint32_t logt = 20;
  uint32_t d = 2;
  bool use_symmetric = true; // use symmetric encryption instead of public key
  // (recommended for smaller query)
  bool use_batching = true;  // pack as many elements as possible into a BFV
  // plaintext (recommended)
  bool use_recursive_mod_switching = true;

  // Generates all parameters
  EncryptionParameters enc_params = load_encryption_parameters(encryptionFile);
  verify_encryption_params(enc_params);

  gen_pir_params(number_of_items, size_per_item, d, enc_params, pir_params,
    use_symmetric, use_batching, use_recursive_mod_switching);

  print_seal_params(enc_params);
  print_pir_params(pir_params);
  // Initialize PIR client....
  PIRClient client(enc_params, pir_params);
  return client;
}
std::string trim(std::string str) {
  // Find the first non-whitespace character in the string.
  std::string::iterator start = str.begin();
  while (start != str.end() && (*start == ' ' || *start == '\t' || *start=='\r' || *start=='\n')) {
    ++start;
  }

  // Find the last non-whitespace character in the string.
  std::string::iterator end = str.end();
  while (end != str.begin() && (*end == ' ' || *end == '\t')) {
    --end;
  }

  // Return a substring of the string from the first non-whitespace character to the last non-whitespace character.
  return str.substr(start - str.begin(), end - start);
}
int parseRequestInfo(const std::string &str,ClientRequestInfo &clientRequestInfo){
  std::vector<std::string> substrings;
  std::stringstream ss(str);
  std::string substring;
  while (std::getline(ss, substring, ';')) {
    substrings.push_back(substring);
  }
  if (substrings.size() >=3) {
    string temp = trim(substrings[0]);
    clientRequestInfo.hostAddr = string(temp);
    temp = trim(substrings[1]);
    clientRequestInfo.jsDBFile =  string(temp);
    temp = trim(substrings[2]);
    clientRequestInfo.eIndex = (uint64_t)atoll(temp.c_str());
  }else{
    return -1;
  }
  return 0;
}
std::vector<ClientRequestInfo> getListServers(const string &filename){
    
     try{
        ifstream file(filename);
        // Read the entire file into a string
        string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        string str = trim(fileContent);
        
        std::vector<ClientRequestInfo> list;
        std::string line;
        for (size_t i=0;i<str.size();i++) {
            if (str[i]=='\r' || str[i] =='\n') {
                if (line.size() > 0) {
                    ClientRequestInfo rq;
                    if (parseRequestInfo(line,rq) >=0) {
                        list.push_back(rq);                                       
                    }                    
                    line.clear();
                }                            
            }else{
                line.push_back(str[i]);
            }
        }
        fileContent.clear();
        str.clear();
        line.clear();
        file.close();
        return list;
    }catch(const exception &e){
        cerr << "Error: Unable to open file for reading." << endl;
    }   
    return std::vector<ClientRequestInfo>();    
}

void clientLogResult(fstream *f,ClientResultLog* logResult)
{ 
  std::lock_guard <std::mutex> gurad(syncLogMutex);
  logResult->log(f);
  
}

void serverLogResult(const string logFileName,ServerResultLog* logResult)
{
  std::lock_guard <std::mutex> gurad(syncLogMutex);
  fstream f;
  f.open(logFileName, ios::app);
  logResult->log(&f);
  f.close();
}

bool fileExists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

void clearLogFile(const std::string& name)
{
  std::ofstream ofs;
  ofs.open(name, std::ofstream::out | std::ofstream::trunc);
  ofs.close();
}
