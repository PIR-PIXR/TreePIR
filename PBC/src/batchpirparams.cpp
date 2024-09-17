#include <cmath>
#include "batchpirparams.h"

BatchPirParams::BatchPirParams(int batch_size, size_t num_entries, size_t entry_size, unsigned int tree_height,
    unsigned int children, seal::EncryptionParameters seal_params)
    : file_name_(DatabaseConstants::FileName),
      tree_height_(tree_height),
      num_hash_funcs_(DatabaseConstants::NumHashFunctions),
      batch_size_(tree_height),
      cuckoo_factor_(DatabaseConstants::CuckooFactor),
      entry_size_(entry_size),
      max_attempts_(DatabaseConstants::MaxAttempts),
      children_(children){

        seal_params_ = seal_params;
        int num_nodes = 0;
        for (int i = 1; i <= tree_height; i++) {
            num_nodes += pow(children_, i);
        }
        num_entries_ = num_nodes;
      }

std::string_view BatchPirParams::get_file_name() {
    return file_name_;
}

int BatchPirParams::get_tree_height() {
    return tree_height_;
}

int BatchPirParams::get_num_hash_funcs() {
    return num_hash_funcs_;
}

seal::EncryptionParameters BatchPirParams::get_seal_parameters() const
{
    return seal_params_;
}

uint32_t BatchPirParams::get_num_slots_per_entry() {
    return ceil((8 * entry_size_ * 1.0) / (seal_params_.plain_modulus().bit_count()-1));
}

int BatchPirParams::get_batch_size() {
    return batch_size_;
}

double BatchPirParams::get_cuckoo_factor() {
    return cuckoo_factor_;
}

size_t BatchPirParams::get_num_entries() {
    return num_entries_;
}

size_t BatchPirParams::get_entry_size() {
    return entry_size_;
}

size_t BatchPirParams::get_max_attempts() {
    return max_attempts_;
}

size_t BatchPirParams::get_max_bucket_size() {
    return max_bucket_size_;
}

size_t BatchPirParams::get_first_dimension_size() {
    return dim_size_;
}

uint64_t BatchPirParams::get_default_value(){
    return default_value_;
}

void BatchPirParams::set_first_dimension_size(size_t max_bucket_size){
    size_t cube_root = std::ceil(std::cbrt(max_bucket_size));
    dim_size_ = utils::next_power_of_two(cube_root);
    auto dim_size = dim_size_;
    auto prev_dim_size = dim_size;
    auto batch_size = ceil((batch_size_*cuckoo_factor_)*1.0/2);
    while(batch_size * dim_size <= seal_params_.poly_modulus_degree()/2){
        prev_dim_size = dim_size;
        dim_size = utils::next_power_of_two(dim_size + 1);
    }
    dim_size_ = prev_dim_size;
}

void BatchPirParams::set_max_bucket_size(size_t max_bucket_size){
    max_bucket_size_ = max_bucket_size;
    set_first_dimension_size(max_bucket_size_);
}

void BatchPirParams::print_params() const {
std::cout << "+---------------------------------------------------+" << std::endl;
std::cout << "|                  Batch Parameters                 |" << std::endl;
std::cout << "+---------------------------------------------------+" << std::endl;
std::cout << std::left << std::setw(20) << "| file_name_: " << file_name_ << std::endl;
std::cout << std::left << std::setw(20) << "| tree_height_: " << tree_height_ << std::endl;
std::cout << std::left << std::setw(20) << "| number_of_children: " << children_ << std::endl;
std::cout << std::left << std::setw(20) << "| number_of_nodes_: " << num_entries_ << std::endl;
std::cout << std::left << std::setw(20) << "| num_hash_funcs_: " << num_hash_funcs_ << std::endl;
std::cout << std::left << std::setw(20) << "| batch_size_: " << batch_size_ << std::endl;
std::cout << std::left << std::setw(20) << "| cuckoo_factor_: " << cuckoo_factor_ << std::endl;
std::cout << std::left << std::setw(20) << "| max_attempts_: " << max_attempts_ << std::endl;
std::cout << "+---------------------------------------------------+" << std::endl;
}

void BatchPirParams::save_params() {
    std::filesystem::create_directory("params");
    std::string file_name = "params/params_" + to_string(tree_height_) +
        "_" + to_string(children_) + ".txt";
    ofstream file_obj;
    file_obj.open(file_name, ios::in);
    auto temp = this;
    file_obj.write((char*)&temp, sizeof(temp));
    file_obj.close();
}
