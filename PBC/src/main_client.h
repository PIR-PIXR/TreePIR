#include <iostream>
#include <map>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <functional>
#include "pirparams.h"
#include "client.h"
#include "batchpirparams.h"
#include "batchpirserver.h"
#include "batchpirclient.h"
#include "database_constants.h"
#include "utils.h"

using namespace std;
using namespace chrono;

int batchpir_main_client(int argc, const char* argv[])
{
    unsigned int tree_height = stoi(argv[2]);
    unsigned int children = stoi(argv[3]);
    if (argc == 5) {
        std::filesystem::create_directory("client_log");
        std::string out_name = "client_log/client_" + std::to_string(tree_height) + "_" + std::to_string(children) + ".txt";
        const char* out = out_name.c_str();
        FILE* output_file = freopen(out, "w", stdout);
    }
    const int client_id = 0;
    //  batch size, number of entries, size of entry
    std::vector<std::array<size_t, 3 >> input_choices;
    size_t num_nodes = 0;
    for (int i = 1; i <= tree_height; i++) {
        num_nodes += pow(children, i);
    }
    input_choices.push_back({ tree_height, num_nodes, 32 });

    std::vector<std::chrono::milliseconds> init_times;
    std::vector<std::chrono::milliseconds> database_times;
    std::vector<std::chrono::milliseconds> query_gen_times;
    std::vector<std::chrono::milliseconds> resp_gen_times;
    std::vector<size_t> communication_list;

    std::cout << "Starting Client" << std::endl;

    const auto& choice = input_choices[0];

    string selection = std::to_string(choice[0]) + "," + std::to_string(choice[1]) + "," + std::to_string(choice[2]);

    auto encryption_params = utils::create_encryption_parameters(selection);
    BatchPirParams params(choice[0], choice[1], choice[2], tree_height, children, encryption_params);
    size_t bucket_size = utils::load_bucket_size(tree_height, children);
    params.set_max_bucket_size(bucket_size);

    unsigned int upper = static_cast<unsigned int>(std::pow(children, tree_height + 1) - 1) / (children - 1);
    unsigned int lower = static_cast<unsigned int>(std::pow(children, tree_height) - 1) / (children - 1) + 1;

    //std::cout << "Main: Starting query generation and information retrieval for " + to_string(num_batches) + " iterations..." << endl;
    int fails = 0;
    std::filesystem::create_directory("requests");
    std::ofstream myfile;
    std::string file_name = "requests/pbc_indices_" + to_string(tree_height) + "_" + to_string(children) + ".txt";
    myfile.open(file_name, std::ofstream::out | std::ofstream::trunc);
    unsigned int num_buckets = ceil(DatabaseConstants::CuckooFactor * tree_height);
    auto hash_map = utils::load_map(tree_height, children);
    auto start = chrono::high_resolution_clock::now();
    auto end = chrono::high_resolution_clock::now();
    auto duration_querygen = chrono::duration_cast<chrono::milliseconds>(end - start);
    auto start_map = chrono::high_resolution_clock::now();
    auto end_map = chrono::high_resolution_clock::now();
    auto total_map = chrono::duration_cast<chrono::microseconds>(end_map - start_map);
    std::string file_output = "";

    std::string file_path = "list_TXs_" + std::to_string(tree_height) + "_" + std::to_string(children) + ".txt";
    std::ifstream input_file(file_path);
    std::vector<int> randIndices;

    if (input_file.is_open()) {
        int index;
        while (input_file >> index) {
            randIndices.push_back(index);
        }
        input_file.close();
    }

    unsigned int num_batches = randIndices.size();

    for (int TX_index : randIndices) {
        try {
            BatchPIRClient batch_client(tree_height, children, params);
            start_map = chrono::high_resolution_clock::now();
            batch_client.set_map(hash_map);
            end_map = chrono::high_resolution_clock::now();
            total_map += chrono::duration_cast<chrono::microseconds>(end_map - start_map);
            vector<uint64_t> entry_indices = generate_batch(tree_height, children, lower, TX_index);
            start = chrono::high_resolution_clock::now();
            auto queries = batch_client.create_queries(entry_indices);
            auto hashed_query = batch_client.get_cuckoo_table();
            auto leaves = batch_client.leaves;
            end = chrono::high_resolution_clock::now();
            duration_querygen += chrono::duration_cast<chrono::milliseconds>(end - start);
            file_output = file_output + "TX_index: " + to_string(entry_indices[0] - lower + 1) + "\n";
            for (int v = 0; v < num_buckets; v++) {
                file_output = file_output + "PBC" + to_string(v + 1) + "_" + to_string(tree_height) + "_" + to_string(children) + ".json; " +
                    "NodeID: " + to_string(leaves[v]) + "; index: " + to_string(hashed_query[v]) + "\n";
            }
        }
        catch (std::invalid_argument const&) {
            fails++;
        }
    }
    myfile << file_output;
    myfile.close();
    query_gen_times.push_back(duration_querygen);
    //std::cout << "Main: Query generation complete for example." << endl;
    //std::cout << "Timings Report" << endl;
    for (size_t i = 0; i < input_choices.size(); ++i)
    {
        //std::cout << "Input Parameters: ";
        std::cout << "Batch_Length: " << tree_height << endl;
        std::cout << "Number_of_Entries: " << input_choices[i][1] << endl;
        std::cout << "Entry_Size: " << input_choices[i][2] << " (bytes)" << endl;
        std::cout << "Average_Indexing_time: " << query_gen_times[i].count() / num_batches << " (ms)" << endl;
        std::cout << "Average_Map_handover_time: " << total_map.count() / num_batches << " (us)" << endl;
        std::cout << "Rate_of_failure: " << fails / num_batches << " (%)" << endl;
    }

    return 0;
}
