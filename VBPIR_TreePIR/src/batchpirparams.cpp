#include "batchpirparams.h"


BatchPirParams::BatchPirParams(int batch_size, size_t num_entries, size_t entry_size, seal::EncryptionParameters seal_params)
    :
      batch_size_(batch_size),
      num_entries_(num_entries),
      entry_size_(entry_size){

        seal_params_ = seal_params;

      }

seal::EncryptionParameters BatchPirParams::get_seal_parameters() const
{
    return seal_params_;
}

uint32_t BatchPirParams::get_num_slots_per_entry() {
    return ceil((8 * entry_size_ * 1.0) / (seal_params_.plain_modulus().bit_count()-1));
}

size_t BatchPirParams::get_num_entries() {
    return num_entries_;
}

size_t BatchPirParams::get_entry_size() {
    return entry_size_;
}

size_t BatchPirParams::get_first_dimension_size() {
    return dim_size_;
}

uint64_t BatchPirParams::get_default_value(){
    return default_value_;
}

void BatchPirParams::set_first_dimension_size(size_t max_bucket_size){
    dim_size_ = 64;//We set 64 for our experiments until h = 20
}

void BatchPirParams::set_max_bucket_size(size_t max_bucket_size){
    max_bucket_size_ = max_bucket_size;
    set_first_dimension_size(max_bucket_size_);
}


void BatchPirParams::print_params() const {
std::cout << "+---------------------------------------------------+" << std::endl;
std::cout << "|                  Batch Parameters                 |" << std::endl;
std::cout << "+---------------------------------------------------+" << std::endl;
std::cout << std::left << std::setw(20) << "| batch_size_: " << batch_size_ << std::endl;
std::cout << std::left << std::setw(20) << "| num_entries_: " << num_entries_ << std::endl;
std::cout << "+---------------------------------------------------+" << std::endl;
}