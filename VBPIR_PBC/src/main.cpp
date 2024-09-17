#include <iostream>
#include <chrono>
#include <functional>
#include "pirparams.h"
#include "batchpirparams.h"
#include "batchpirserver.h"
#include "batchpirclient.h"

using namespace std;
using namespace chrono;

long first_nodeID(int l) {
    return static_cast<long>(std::pow(2, l) - 1) + 1;
}

std::vector<long> PathID(int h, int index) {
    std::vector<long> path_nodeID(h);

    path_nodeID[h - 1] = first_nodeID(h) + index - 1;
    // Node ID from bottom to top
    for (int i = h - 2; i >= 0; i--) {
        path_nodeID[i] = static_cast<long>(std::ceil(static_cast<double>(path_nodeID[i + 1] - 1) / 2));
    }
    return path_nodeID;
}

void printHex(const std::vector<unsigned char>& bytes) {
    std::string hexString;
    for (const auto& byte : bytes) {
        std::stringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        hexString += ss.str();
    }
    // Print the hexadecimal string
    std::cout << "Hexadecimal string: 0x" << hexString << std::endl;
}

int batchpir_main(int argc, char* argv[])
{
    unsigned int tree_height = 10;
    unsigned int children = 2;
    size_t serialized_anw_size = 0;
    size_t serialized_query_size = 0;
    std::chrono::microseconds query_gen_costs{0};
    std::chrono::microseconds resp_gen_costs{0};
    std::chrono::microseconds extract_costs{0};

    //  batch size, number of entries, size of entry
    std::vector<std::array<size_t, 3>> input_choices;
    size_t num_nodes = pow(children, tree_height + 1) - 2;
    input_choices.push_back({ tree_height, num_nodes, 32 }); //{height, N, size}
    //input_choices.push_back({10, 2046, 32}); //{height, N, size}

    const int client_id = 0;

    //std::vector<std::chrono::microseconds> init_times;
    std::vector<std::chrono::microseconds> query_gen_times;
    std::vector<std::chrono::microseconds> resp_gen_times;
    std::vector<std::chrono::microseconds> extract_times;
    std::vector<size_t> communication_query_list;
    std::vector<size_t> communication_response_list;

    std::string filePath = "/home/quang/Desktop/TreePIR/VBPIR_PBC/list_TXs_" + std::to_string(tree_height) + "_" + std::to_string(2) + ".txt";
    std::vector<long> randIndices;

    std::ifstream file(filePath);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            randIndices.push_back(std::stol(line));
            //cout << std::stol(line) << endl;
        }
        file.close();
    } else {
        std::cerr << "Unable to open file: " << filePath << std::endl;
    }

 for (size_t iteration = 0; iteration < randIndices.size(); ++iteration)
{
     long index = randIndices[iteration];
    std::cout << "***************************************************" << std::endl;
    std::cout << "             Starting example index = " << index << "               " << std::endl;
    std::cout << "***************************************************" << std::endl;

    const auto& choice = input_choices[0];

    string selection = std::to_string(choice[0]) + "," + std::to_string(choice[1]) + "," + std::to_string(choice[2]);

    vector<long> MerkleProof =  PathID(tree_height, index);
    vector<uint64_t> entry_indices;

    std::cout << "Merkle Proof: ";
    for (uint64_t id : MerkleProof) {
        std::cout << id << " ";
        entry_indices.push_back(id - 2);
    }
    std::cout << std::endl;

    std::cout << "entry_indices: ";
    for (uint64_t index : entry_indices) {
        std::cout << index << " ";
    }
    std::cout << std::endl;

    auto encryption_params = utils::create_encryption_parameters(selection);
    BatchPirParams params(choice[0], choice[1], choice[2], encryption_params);
    params.print_params();

    //auto start = chrono::high_resolution_clock::now();
    BatchPIRServer batch_server(tree_height, children, params);
    //auto end = chrono::high_resolution_clock::now();
    //auto duration_init = chrono::duration_cast<chrono::microseconds>(end - start);
    //init_times.push_back(duration_init);

    BatchPIRClient batch_client(params);

    auto map = batch_server.get_hash_map();
    batch_client.set_map(map);

    batch_server.set_client_keys(client_id, batch_client.get_public_keys());


    cout << "Main: Starting query generation for example " << (iteration + 1) << "..." << endl;
    auto start = chrono::high_resolution_clock::now();
    auto queries = batch_client.create_queries(entry_indices);
    auto end = chrono::high_resolution_clock::now();
    auto duration_querygen = chrono::duration_cast<chrono::microseconds>(end - start);
    query_gen_times.push_back(duration_querygen);
    cout << "Main: Query generation complete for example " << (iteration + 1) << "." << endl;

    cout << "Main: Starting response generation for example " << (iteration + 1) << "..." << endl;
    start = chrono::high_resolution_clock::now();
    PIRResponseList responses = batch_server.generate_response(client_id, queries);
    end = chrono::high_resolution_clock::now();
    auto duration_respgen = chrono::duration_cast<chrono::microseconds>(end - start);
    resp_gen_times.push_back(duration_respgen);
    cout << "Main: Response generation complete for example " << (iteration + 1) << "." << endl;

    cout << "Main: Extracting responses for example " << (iteration + 1) << "..." << endl;
    start = chrono::high_resolution_clock::now();
    auto decode_responses = batch_client.decode_responses_chunks(responses);
    end = chrono::high_resolution_clock::now();
    auto duration_extract = chrono::duration_cast<chrono::microseconds>(end - start);
    extract_times.push_back(duration_extract);
    cout << "Main: Extraction complete for example " << (iteration + 1) << "." << endl;

    //Total Answer Communication costs
    serialized_anw_size = 0;
    for (int i = 0; i < responses.size(); i++){
        serialized_anw_size += ceil(responses[i].save_size());
    }

    //Total Query Communication costs
    serialized_query_size = 0;
    for (const auto & query : queries[client_id]){
        serialized_query_size += ceil(query.save_size()/2);
    }
    //communication_query_list.push_back(batch_client.get_serialized_commm_size());

    // Printing decode_responses
    std::cout << "Contents of decode_responses:" << std::endl;
    for (const auto& outer_vec : decode_responses) {
        for (const auto& inner_vec : outer_vec) {
            printHex(inner_vec); // Printing each RawDB object as hexadecimal bytes
        }
    }

    cout << "Total Query communication: " << serialized_query_size/1024 << " KB" << endl;
    cout << "Total Answer communication: " << serialized_anw_size/1024 << " KB" << endl;

    auto RAW_DB = batch_server.getRawDB();

    int count = 0;
    for (int i = 0; i < entry_indices.size(); i++){
        for (const auto& outer_vec : decode_responses) {
            for (const auto& inner_vec : outer_vec) {
                if (inner_vec == RAW_DB[entry_indices[i]]) {
                    //cout << "true " << i << endl;
                    count++;
                }
            }
        }
    }

    auto cuckoo_table = batch_client.get_cuckoo_table();

    if (batch_server.check_decoded_entries(decode_responses, cuckoo_table) && count == entry_indices.size())
    {
        cout << "Main: All the entries matched for example " << (iteration + 1) << "!!" << endl;
    }

    cout << endl;
}


    cout << "***********************" << endl;
    cout << "     Timings Report    " << endl;
    cout << "***********************" << endl;
    cout << "Input Parameters: ";
    cout << "Batch Size: " << input_choices[0][0]*1.5 << ", ";
    cout << "Number of Entries: " << input_choices[0][1] << ", ";
    cout << "Entry Size: " << input_choices[0][2] << endl;

    for (size_t i = 0; i < randIndices.size(); ++i)
    {
        query_gen_costs += query_gen_times[i];
        resp_gen_costs += resp_gen_times[i];
        extract_costs += extract_times[i];
    }

    cout << "Query generation time: " << query_gen_costs.count()/query_gen_times.size() << " microseconds" << endl;
    cout << "Response generation time: " << resp_gen_costs.count()/resp_gen_times.size() << " microseconds" << endl;
    cout << "Extraction time: " << extract_costs.count()/extract_times.size() << " microseconds" << endl;
    cout << "Total Query communication: " << serialized_query_size/1024 << " KB" << endl;
    cout << "Total Answer communication: " << serialized_anw_size/1024 << " KB" << endl;
    cout << endl;

    return 0;
}


int main(int argc, char *argv[])
{
    batchpir_main(argc, argv);
    return 0;
}