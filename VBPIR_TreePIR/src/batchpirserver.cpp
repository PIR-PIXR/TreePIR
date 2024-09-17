#include "header/batchpirserver.h"

BatchPIRServer::BatchPIRServer(unsigned int tree_height, unsigned int children, BatchPirParams &batchpir_params)
{
    tree_height_ = tree_height;
    children_ = children;
    batchpir_params_ = &batchpir_params;
    is_client_keys_set_ = false;
    is_simple_hash_ = false;

    std::cout << "BatchPIRServer: Populating raw database..." << std::endl;
    populate_raw_db();
    std::cout << "BatchPIRServer: Raw database populated." << std::endl;

    std::cout << "BatchPIRServer: Reading Color sub-databases and balancing..." << std::endl;
    read_colorBD();
    balance_buckets();
    std::cout << "BatchPIRServer: Reading Color sub-databases completed." << std::endl;

    std::cout << "BatchPIRServer: Preparing PIR servers......" << std::endl;
    prepare_pir_server();
    std::cout << "BatchPIRServer: PIR servers preparation complete." << std::endl;
}


void BatchPIRServer::populate_raw_db()
{
    auto db_entries = batchpir_params_->get_num_entries();
    auto entry_size = batchpir_params_->get_entry_size();

    // Resize the rawdb vector to the correct size
    rawdb_.resize(db_entries);

    std::string filename = "WholeTree_" + std::to_string(tree_height_) + "_" + std::to_string(children_) + ".json";

    // Open the JSON file
    ifstream file("/home/quang/Desktop/PIR-CSA/VBPIR_CSA/treedata/" + filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file." << std::endl;
    }
    // Read the entire file into a string
    string json((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    // Close the file
    file.close();

    // Create a Document object to hold the JSON data
    rapidjson::Document doc;
    // Parse the JSON data
    doc.Parse(json.c_str());
    json.clear();

    size_t i = 0;

    // Access keys and values
    if (doc.IsObject()) {
        for (rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
            // Access key
            std::string key = itr->name.GetString();
            // Access value
            std::string value = itr->value.GetString();
            //std::cout << "Key: " << key << ", Value: " << value << std::endl;

            // Convert hexadecimal string to bytes
            std::vector<unsigned char> bytes;
            bytes.reserve(value.size() / 2);

            for (size_t i = 0; i < value.size(); i += 2) {
                std::string byteString = value.substr(i, 2);
                unsigned char byte = std::stoi(byteString, nullptr, 16);
                bytes.push_back(byte);
            }

            // Convert bytes back to hexadecimal string
            std::string hexString;
            for (const auto& byte : bytes) {
                std::stringstream ss;
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                hexString += ss.str();
            }
            //std::cout << "Hexadecimal string (RawDB): 0x" << hexString << std::endl;
            rawdb_[i] = bytes;
            i++;
        }
    }
}

std::size_t BatchPIRServer::get_max_bucket_size() const
{
    return ceil((pow(children_, tree_height_ + 1) - 2) / tree_height_);
}

void BatchPIRServer::read_colorBD() {
    std::vector<char> color = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
                               'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    auto total_buckets = tree_height_;
    buckets_.resize(total_buckets);

    for (uint64_t i = 0; i < tree_height_; i++) {
        std::string filename = "color" + std::string(1, color[i]) + "_" + std::to_string(tree_height_) + "_" + std::to_string(children_) + ".json";
        std::string full_path = "/home/quang/Desktop/PIR-CSA/VBPIR_CSA/colorSubDB/" + filename;

        //std::cout << "fullpath: " << full_path << std::endl;

        // Open the JSON file
        std::ifstream file(full_path);
        if (!file.is_open()) {
            std::cerr << "Error: Unable to open file: " << full_path << std::endl;
            continue; // Skip to the next iteration
        }

        // Read the entire file into a string
        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // Close the file
        file.close();

        // Create a Document object to hold the JSON data
        rapidjson::Document doc;
        // Parse the JSON data
        doc.Parse(json.c_str());
        json.clear(); // Clear the string to release memory

        // Handle the JSON format (array of key-value pairs)
        if (doc.IsArray()) {
            for (rapidjson::SizeType j = 0; j < doc.Size(); ++j) {
                const auto& obj = doc[j];
                for (rapidjson::Value::ConstMemberIterator itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {
                    // Access key
                    std::string key = itr->name.GetString();
                    // Access value
                    std::string value = itr->value.GetString();
                    //std::cout << "Key: " << key << ", Value: " << value << std::endl;

                    // Convert hexadecimal string to bytes
                    std::vector<unsigned char> bytes;
                    bytes.reserve(value.size() / 2);

                    for (size_t j = 0; j < value.size(); j += 2) {
                        std::string byteString = value.substr(j, 2);
                        unsigned char byte = std::stoi(byteString, nullptr, 16);
                        bytes.push_back(byte);
                    }

                    // Convert bytes back to hexadecimal string
                    std::string hexString;
                    for (const auto& byte : bytes) {
                        std::stringstream ss;
                        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                        hexString += ss.str();
                    }
                    //std::cout << "Bucket " << i << ": 0x" << hexString << std::endl;
                    buckets_[i].push_back(bytes);
                }
            }
        }
    }
    print_stats();

    batchpir_params_->set_max_bucket_size(get_max_bucket_size());
    //balance_buckets();
}

void BatchPIRServer::balance_buckets()
{
    auto max_bucket = ceil((pow(children_, tree_height_ + 1) - 2) / tree_height_);
    auto num_buckets = buckets_.size();
    auto entry_size = batchpir_params_->get_entry_size();

    auto generate_one_entry = [entry_size]() -> std::vector<unsigned char>
    {
        return std::vector<unsigned char>(entry_size, 1);
    };

    for (int i = 0; i < num_buckets; i++)
    {
        auto size = (max_bucket - buckets_[i].size());
        for (int j = 0; j < size; j++)
        {
            buckets_[i].push_back(generate_one_entry());
        }
    }

    is_simple_hash_ = true;
}

void BatchPIRServer::print_stats() const
{
    std::cout << "BatchPIRServer: Bucket Statistics:\n";
    std::cout << "===================\n";
    std::cout << "BatchPIRServer: Number of Buckets: " << buckets_.size() << "\n";

    size_t max_bucket_size = get_max_bucket_size();

    std::cout << "Max Bucket Size: " << max_bucket_size << "\n";
}

size_t BatchPIRServer::get_first_dimension_size(size_t num_entries)
{
    size_t cube_root = std::ceil(std::cbrt(num_entries));
    return utils::next_power_of_two(cube_root);
}

void BatchPIRServer::prepare_pir_server()
{

    size_t max_bucket_size = ceil((pow(children_, tree_height_ + 1) - 2) / tree_height_);
    size_t entry_size = batchpir_params_->get_entry_size();
    size_t dim_size = batchpir_params_->get_first_dimension_size();

    cout<<"dim_size = " << dim_size << endl;

    auto max_slots = batchpir_params_->get_seal_parameters().poly_modulus_degree();
    auto num_buckets = buckets_.size();
    size_t per_server_capacity = max_slots / dim_size;
    size_t num_servers = ceil(num_buckets * 1.0 / per_server_capacity);

    auto remaining_buckets = num_buckets;
    auto previous_idx = 0;
    for (int i = 0; i < num_servers; i++)
    {
        const size_t offset = std::min(per_server_capacity, num_buckets - previous_idx);
        vector<RawDB> sub_buckets(buckets_.begin() + previous_idx, buckets_.begin() + previous_idx + offset);
        previous_idx += offset;

        PirParams params(max_bucket_size, entry_size, offset, batchpir_params_->get_seal_parameters(), dim_size);
        params.print_values();
        Server server(params, sub_buckets);

        server_list_.push_back(server);
    }
}

void BatchPIRServer::set_client_keys(uint32_t client_id, std::pair<seal::GaloisKeys, seal::RelinKeys> keys)
{
    for (int i = 0; i < server_list_.size(); i++)
    {
        server_list_[i].set_client_keys(client_id, keys);
    }
    is_client_keys_set_ = true;
}

void BatchPIRServer::get_client_keys()
{

    for (int i = 0; i < server_list_.size(); i++)
    {
        server_list_[i].get_client_keys();
    }
}

PIRResponseList BatchPIRServer::generate_response(uint32_t client_id, vector<PIRQuery> queries)
{

    if (!is_client_keys_set_)
    {
        throw std::runtime_error("Error: Client keys not set");
    }
    vector<PIRResponseList> responses;

    for (int i = 0; i < server_list_.size(); i++)
    {
        responses.push_back(server_list_[i].generate_response(client_id, queries[i]));
    }

    return merge_responses(responses, client_id);
}

PIRResponseList BatchPIRServer::merge_responses(vector<PIRResponseList> &responses, uint32_t client_id)
{
    return server_list_[0].merge_responses_chunks_buckets(responses, client_id);
}