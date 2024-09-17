#include "spiral.h"

static double time_key_gen = 0;
static double time_query_gen = 0;
static double time_expansion_main = 0;
static double time_expansion_further = 0;
static double time_conversion = 0;
static double time_first_multiply = 0;
static double time_folding = 0;
static double time_decoding = 0;

MatPoly pt_encd_correct(n0, n2);
MatPoly pt_real(n0, n2, false);

bool nonoise = false;
bool direct_upload = false;
bool ternary = false;
bool random_data = false;
bool show_diff = false;
bool output_err = false;

static size_t total_offline_size_b = 0;


namespace UnixColours {
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    const std::string UNDERLINE = "\033[4m";
    const std::string RESET = "\033[0m";
}


#ifdef NATIVELOG
namespace NativeLog {
    struct Cout {
        std::ostringstream oss;
        template <typename T>
        Cout& operator<<(const T& val) {
            oss << val;
            return *this;
        }
        Cout& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
            if (manipulator == static_cast<std::ostream& (*)(std::ostream&)>(std::endl)) {
                oss << "\n";
                std::cout << "[Native] " << oss.str();
                oss.str("");
                oss.clear();
            }
            return *this;
        }
    };
    Cout cout;
}
#else
namespace NativeLog {
    struct Cout {
        template<class T>
        Cout& operator<<(const T& val) {
            return *this;
        }
        Cout& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
            return *this;
        }
    };
    Cout cout;
}
#endif

#ifdef LOG
namespace Log {
    struct Cout {
        std::ostringstream oss;
        template <typename T>
        Cout& operator<<(const T& val) {
            oss << val;
            return *this;
        }
        Cout& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
            if (manipulator == static_cast<std::ostream& (*)(std::ostream&)>(std::endl)) {
                oss << "\n";
                std::cout << "[" << UnixColours::CYAN << "Log"
                          << UnixColours::RESET << "] " << oss.str();
                oss.str("");
                oss.clear();
            }
            return *this;
        }
    };
    Cout cout;
}
#else
namespace Log {
    struct Cout {
        template<class T>
        Cout& operator<<(const T& val) {
            return *this;
        }
        Cout& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
            return *this;
        }
    };
    Cout cout;
}
#endif

void get_gadget_1(MatPoly &G) { // n0 x m1
    // identity
    for (size_t j = 0; j < m1; j++) {
        G.data[(j + j * m1) * coeff_count] = 1;
    }
}

/**
 * @brief Perform modulus switching to the output to reduce encoding size.
 * @param out The rescaled response `r`.
 * @param inp The Regev encoding to perform modulus switching over.
 *      @note inp: n1 * n2 * poly_len * sizeof(uint64_t)
 * @return @param out
 *
 * @details
 * Utilises two different scaling factors to achieve a greater rate.
 * @see pg. 936, D. Modulus Switching.
 *
 * @note This is optional, and can be disabled by setting
 *       `modswitch_on_server` to `false`.
*/
void modswitch(uint64_t *out, const uint64_t *inp) {
    size_t rs = n1;
    size_t cs = n2;

    size_t bit_offs = 0;
    size_t bit_width = bits_to_hold_arb_qprime;
    // cout << "PREE:";
    for (size_t r = 0; r < rs; r++) {
        for (size_t c = 0; c < cs; c++) {
            for (size_t m = 0; m < poly_len; m++) {
                int64_t val = inp[r * cs * poly_len + c * poly_len + m];
                // assert(val < Q_i);
                // if (val > (Q_i_u128/2)) val -= Q_i_u128;
                // double rounded_val = round(((double)val) / ((double)(qq_i)));
                // long double dbl_val = ((long double)val) / ((long double)(factor_to_remove_obliviously));
                // long double dbl_val = ((long double)val) * ((long double)(q_intermediate)) / ((long double) Q_i);

                // long double dbl_val = ((long double)val) * ((long double) arb_qprime) / ((long double) Q_i);
                long double dbl_val = ((long double)val) * ((long double) arb_qprime) / ((long double) Q_i);
                // cout << val << " -> " << (int64_t)round(dbl_val) << endl;

                // uint64_t val_mod_q = val % q_i;
                // double mul_of_q = round((dbl_val - val_mod_q) / (long double)q_i);
                // uint64_t result = q_const * ((uint64_t)mul_of_q) + val_mod_q;// % q_i;
                int64_t result = (int64_t)round(dbl_val);
                // if (result < 0) result += (switch_factor * q_const);
                // result = (result + arb_qprime) % arb_qprime;
                assert(result <= (int64_t)arb_qprime);
                // assert(result >= 0);
                // cout << ((r * cs * poly_len + c * poly_len + m) * bit_width) << endl;
                // out[r * cs * poly_len + c * poly_len + m] = result;
                write_arbitrary_bits(out, result, bit_offs, bit_width);
                // cout << result << " ";
                bit_offs += bit_width;
            }
        }
    }
    // cout << endl;
}

auto start_stage = chrono::high_resolution_clock::now();
auto start = chrono::high_resolution_clock::now();
size_t stage = 0;
void start_timing() {
    // cout << "#\t\t\t\t\t START" << endl;
    stage = 0;
    start_stage = chrono::high_resolution_clock::now();
    start = chrono::high_resolution_clock::now();
}
void record() {
    // cout << "#\t\t\t\t\t" << stage;
    // cout << " :"
    //      << (chrono::high_resolution_clock::now() - start).count() / 1000
    //      << endl;
    // start = chrono::high_resolution_clock::now();
    // stage++;
}
void record(const string &s) {
    // cout << "#" << s << "\t\t\t\t\t";
    // cout << " "
    //      << (chrono::high_resolution_clock::now() - start).count() / 1000
    //      << endl;
    // start = chrono::high_resolution_clock::now();
    // stage++;
}
double end_timing() {
    uint64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now() - start_stage).count();
    // double duration = (chrono::high_resolution_clock::now() - start_stage).count() / 1000;
    // cout << "#\t\t\t\t\t";
    // cout << "  >"
    //      << duration
    //      << endl;
    // cout << "#\t\t\t\t\t END" << endl;
    start_stage = chrono::high_resolution_clock::now();
    start = chrono::high_resolution_clock::now();
    stage = 0;
    return duration_us;
}

double perf_time(std::function<void()> f) {
    auto start_time = chrono::high_resolution_clock::now();
    f();
    double duration = (chrono::high_resolution_clock::now() - start_time).count() / 1000;
    return duration;
}

void setup_H_F_for_n2_eq_2() {
    // Question: What is F? What does this do? n2 = 2.
    F_mp = MatPoly(n2, n2, false);
    memset(F_inv_vals, 0, sizeof(F_inv_vals));

    // Question: What is H? Why is it being populated with 1's and 0's?
    H_mp = MatPoly(n0, n2, false);
    build_from_constants(H_mp, {
        {1, 0},
        {0, 1}
    });
}

void setup_constants() {
    // Question: Is this a problem?
    if (n2 == 2) {
        setup_H_F_for_n2_eq_2();
    } else if (n2 == 3) {
        // setup_H_F_for_n2_eq_3();
    } else if (n2 == 4) {
        // setup_H_F_for_n2_eq_4();
    } else if (n2 == 6) {
        // setup_H_F_for_n2_eq_6();
    } else {
        cout << "unsupported value of n2" << endl;
        exit(1);
    }

    MatPoly ZO(n0, n2, false);
    // Question: What is H and F? Why are they multiplied?
    // Note: HF nor HF_plain is not being used. Maybe is just a check?
    MatPoly HF(n0, n2);
    MatPoly HF_plain(n0, n2, false);
    multiply(HF, to_ntt(H_mp), to_ntt(F_mp));
    from_ntt(HF_plain, HF);
    reduce_mod(HF_plain, p_i);
    bool ok = is_eq(HF_plain, ZO);
    NativeLog::cout << "HF == zero? " << ok << endl;

    if (!ok) {
        cout << "HF = ";
        for (size_t i = 0; i < n0 * n2 * poly_len; i++) {
            cout << HF_plain.data[i] << " ";
        }
        cout << endl;
        exit(1);
    }

    size_t num_expansions_max = 16;
    MatPoly neg1s_pre(1, num_expansions_max, false);
    MatPoly neg1s(1, num_expansions_max, false);
    uint64_t *neg1s_data = (uint64_t *)neg1s_pre.data;
    for (size_t j = 0; j < num_expansions_max; j++) {
        size_t idx = poly_len - (1 << j);
        uint64_t val = 1;

        neg1s_data[j * poly_len + idx] = val;
    }
    invert(neg1s, neg1s_pre);

    MatPoly half(1, 1, false);
    half.data[0] = (Q_i + 1UL) / 2UL;

    MatPoly neg1s_nttd = to_ntt(neg1s);
    for (size_t i = 0; i < num_expansions_max; i++) {
        size_t idx = poly_len - (1 << i);
        MatPoly ng1(1, 1, false);
        ng1.data[idx] = 1;
        MatPoly ng1_inv(1, 1, false);
        invert(ng1_inv, ng1);
        neg1s_mp.push_back(to_ntt(ng1_inv));
    }
    half_mp = to_ntt(half);

    constant_neg1s = neg1s_nttd.data;
    constant_half = half_mp.data;
}

static void add_pub_param(const MatPoly &mat) {
    total_offline_size_b += mat.rows * mat.cols * poly_len * logQ / 8;
}

/**
 * @brief For each element in the vector, calculate offline size
 *          in bytes and add it to a global count.
 */
static void add_pub_param(const vector<MatPoly> &vec) {
    for (size_t i = 0; i < vec.size(); i++) {
        add_pub_param(vec[i]);
    }
}

void print_summary() {
    double pt_mod = log2((double)p_db);
    size_t pt_elem_size = (size_t)((2.0 * 2 * poly_len * (double) pt_mod) / 8.0);
    size_t db_size_mb = (size_t)(((double)(pt_elem_size * ((double)total_n))) / 1000000.0);
    cout << endl;
    cout << "PIR over n=" << total_n << " elements of size " << pt_elem_size << " bytes each." << endl;
    // cout << "The total database size is " << db_size_mb << "mb." << endl;
    cout << "The database is structured as " << (1 << num_expansions) << " x 2^" << further_dims << "." << endl;
    cout << endl;

    size_t b_per_elem = (size_t)((double) poly_len * logQ / 8.0);
    size_t dim0_query_size_kb = 0;
    if (direct_upload) {
        dim0_query_size_kb = n1 * n0 * (1 << num_expansions) * b_per_elem;
    }
    size_t total_offline_size_kb = total_offline_size_b;
    dim0_query_size_kb = (query_elems_first + query_elems_rest) * n0 * b_per_elem;
    size_t dim1s_query_size_kb = 0;

    size_t total_query_size_kb = dim0_query_size_kb + dim1s_query_size_kb;

    size_t total_resp_size_kb = n1 * n2 * b_per_elem;
    if (modswitch_on_server) {
        // total_resp_size_kb = n1 * n2 * (double) poly_len * (double)bits_to_hold_arb_qprime / 8.0;
        total_resp_size_kb = ((n0 * n0 * (double) poly_len * (pt_mod + 2)) + (n0 * (double) poly_len * (double)bits_to_hold_arb_qprime)) / 8.0;
    }

    cout << fixed << setprecision(0);
    cout << "Communication" << endl;
    cout << endl;
    cout << "         Total offline query size (b): " << total_offline_size_kb << endl;
    cout << "                  First dimension (b): " << dim0_query_size_kb << endl;
    cout << "       Total for other dimensions (b): " << dim1s_query_size_kb << endl;
    cout << "          Total online query size (b): " << total_query_size_kb << endl;
    cout << "                    Response size (b): " << total_resp_size_kb  << endl;
    cout << endl;
    cout << endl;
    cout << "Database-independent computation" << endl;
    cout << endl;
    cout << "              Main expansion  (CPU·us): " << time_expansion_main << endl;
    cout << "  Further dimension expansion (CPU·us): " << time_expansion_further << endl;
    cout << "                   Conversion (CPU·us): " << time_conversion << endl;
    cout << "                        Total (CPU·us): " << (time_expansion_main + time_expansion_further + time_conversion) << endl;
    cout << endl;
    cout << "Database-dependent computation" << endl;
    cout << endl;
    cout << "     First dimension multiply (CPU·us): " << time_first_multiply << endl;
    cout << "                      Folding (CPU·us): " << time_folding << endl;
    cout << "                        Total (CPU·us): " << (time_first_multiply + time_folding) << endl;
    cout << endl;
    cout << "Client computation" << endl;
    cout << endl;
    cout << "               Key generation (CPU·us): " << time_key_gen << endl;
    cout << "             Query generation (CPU·us): " << time_query_gen << endl;
    cout << "                     Decoding (CPU·us): " << time_decoding << endl;
    cout << endl;
}

// in:  num_per,n1,n2,poly_len
// out: num_per,m2,n2,2,poly_len
// out: num_per,(n1,k),n2,2,poly_len
void split_and_crt(uint64_t * __restrict__ out, const uint64_t * __restrict__ in, size_t num_per) {
    size_t num_elems = m2 / n1;
    size_t bits_per = get_bits_per(num_elems);
    uint64_t mask = (1UL << bits_per) - 1;

    size_t real_idx = 0;
    for (size_t i = 0; i < num_per; i++) {
        for (size_t r = 0; r < n1; r++) {
            for (size_t c = 0; c < n2; c++) {
                for (size_t z = 0; z < poly_len; z++) {
                    uint64_t val = in[i * (n1 * n2 * poly_len) + r * (n2 * poly_len) + c * (poly_len) + z];

                    uint64_t carry = 0;
                    for (size_t k = 0; k < num_elems/2; k++) {
                        size_t row = r + k * n1;
                        size_t bit_offs = min(k * bits_per, 64);
                        uint64_t piece = (val >> bit_offs) & mask;
                        piece += carry;
                        carry = 0;
                        if (piece > ((1<< bits_per)/2) && k < (num_elems/2 - 1)) {
                            piece += Q_i - (1 << bits_per);
                            carry = 1;
                        }

                        size_t out_idx = i * (m2 * n2 * crt_count * poly_len) + row * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);

                        for (size_t n = 0; n < crt_count; n++) {
                            out[out_idx + n * poly_len + z] = barrett_coeff(piece, n);
                        }
                    }
                }

                for (size_t k = 0; k < num_elems/2; k++) {
                    size_t row = r + k * n1;
                    size_t out_idx = i * (m2 * n2 * crt_count * poly_len) + row * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);

                    ntt_forward(&out[out_idx]);
                }

                for (size_t z = 0; z < poly_len; z++) {
                    uint64_t val = in[i * (n1 * n2 * poly_len) + r * (n2 * poly_len) + c * (poly_len) + z];

                    uint64_t carry = 0;
                    for (size_t k = num_elems/2; k < num_elems; k++) {
                        size_t row = r + k * n1;
                        size_t bit_offs = min(k * bits_per, 64);
                        uint64_t piece = (val >> bit_offs) & mask;
                        piece += carry;
                        carry = 0;
                        if (piece > ((1<< bits_per)/2)) {
                            piece += Q_i - (1 << bits_per);
                            carry = 1;
                        }

                        size_t out_idx = i * (m2 * n2 * crt_count * poly_len) + row * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);

                        for (size_t n = 0; n < crt_count; n++) {
                            out[out_idx + n * poly_len + z] = barrett_coeff(piece, n);
                        }
                    }
                }

                for (size_t k = num_elems/2; k < num_elems; k++) {
                    size_t row = r + k * n1;
                    size_t out_idx = i * (m2 * n2 * crt_count * poly_len) + row * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);

                    ntt_forward(&out[out_idx]);
                }
            }
        }
    }
}

// reorient C : i  m  c nz
//         to : z  i  c m
void reorient_C(uint64_t * out, const uint64_t * inp, size_t num_per) {
    // size_t idx = 0;
    // for (size_t i = 0; i < num_per; i++) {
    //     for (size_t m = 0; m < m2; m++) {
    //         for (size_t c = 0; c < n2; c++) {
    //             for (size_t z = 0; z < poly_len; z++) {
    //                 size_t inp_idx = i * (m2 * n2 * crt_count * poly_len) + m * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);
    //                 size_t out_idx = z * (num_per*n2*m2) + i * (n2*m2) + c * (m2) + m;
    //                 out[out_idx] = inp[inp_idx + z] | (inp[inp_idx + poly_len + z] << packed_offset);
    //             }
    //         }
    //     }
    // }
    size_t blocksize_r = (2*num_per) < 64 ? (2*num_per) : 64;
    size_t blocksize_c = 128;
    size_t rows = num_per*m2*n2; // 18432
    size_t cols = poly_len;      //  4096
    // #pragma omp parallel for num_threads(6)
    for (size_t block_i = 0; block_i < rows; block_i += blocksize_r) {
        for (size_t block_j = 0; block_j < cols; block_j += blocksize_c) {
            // transpose the block beginning at [block_i,block_j]
            for (size_t row = block_i; row < block_i + blocksize_r; ++row) {
                for (size_t col = block_j; col < block_j + blocksize_c; ++col) {
                    size_t ridx = row;
                    size_t i = ridx / (m2*n2);
                    ridx -= i * (m2*n2);
                    size_t m = ridx / (n2);
                    ridx -= m * (n2);
                    size_t c = ridx;

                    size_t z = col;
                    size_t out_idx = z * (num_per*n2*m2) + i * (n2*m2) + c * (m2) + m;
                    size_t inp_idx = i * (m2 * n2 * crt_count * poly_len) + m * (n2 * crt_count * poly_len) + c * (crt_count * poly_len);
                    // out[out_idx] = inp[inp_idx + z] | (inp[inp_idx + poly_len + z] << packed_offset_1)  | (inp[inp_idx + 2*poly_len + z] << packed_offset_2);
                    out[out_idx] = inp[inp_idx + z] | (inp[inp_idx + poly_len + z] << 32);
                }
            }
        }
    }
}

// reorient Q : r  m  nz
//         to : nz r  m
void reorient_Q(uint64_t *out, const uint64_t *inp) {
    // #pragma omp parallel for
    for (size_t r = 0; r < n1; r++) {
        for (size_t m = 0; m < m2; m++) {
            for (size_t z = 0; z < poly_len; z++) {
                size_t inp_idx = r * (m2*crt_count*poly_len) + m * (crt_count*poly_len);
                size_t out_idx = z * (n1*m2) + r * (m2) + m;
                // out[out_idx] = inp[inp_idx + z] | (inp[inp_idx + poly_len + z] << packed_offset_1);//  | (inp[inp_idx + 2*poly_len + z] << packed_offset_2);
                out[out_idx] = inp[inp_idx + z] | (inp[inp_idx + poly_len + z] << 32);
            }
        }
    }
}

// Changes index order for expanded ciphertext data to allow coherent access
// patterns when running many threads. Also stores coefficients in the packed
// format using packed_offset
//
// Input:  dim0 ciphertexts of n1 x n0 polynomials, indexed as (j, r, m, n, z)
// Output: same data, packed and indexed as (z, j, m, r)
//
//
void reorientCiphertexts(uint64_t *out, const uint64_t *inp, size_t dim0, size_t n1_padded) {
    #pragma omp parallel for
    for (size_t j = 0; j < dim0; j++) {
        for (size_t r = 0; r < n1; r++) {
            for (size_t m = 0; m < 2; m++) {
                for (size_t z = 0; z < poly_len; z++) {
                    size_t idx_a_in = j * (n1*2*crt_count*poly_len)
                                        + r * (2*crt_count*poly_len)
                                        + m * (crt_count*poly_len);
                    size_t idx_a_out =  z * (dim0*2*n1_padded)
                                        + j * (2*n1_padded)
                                        + m * (n1_padded)
                                        + r;
                    // out[idx_a_out] = inp[idx_a_in + z] | (inp[idx_a_in + poly_len + z] << packed_offset_1)  | (inp[idx_a_in + 2*poly_len + z] << packed_offset_2);
                    // uint64_t val_pa = crt_compose_pa(inp[idx_a_in + z], inp[idx_a_in + poly_len + z]);
                    // out[idx_a_out] =  val_pa | (inp[idx_a_in + 2*poly_len + z] << packed_offset_2);
                    // uint64_t val = crt_compose(inp[idx_a_in + z], inp[idx_a_in + poly_len + z], inp[idx_a_in + 2*poly_len + z]);
                    // out[idx_a_out] =  val;
                    out[idx_a_out] = inp[idx_a_in + z] | (inp[idx_a_in + poly_len + z] << 32);
                }
            }
        }
    }
}

// Convert the `num_per` ciphertexts in `furtherDimsLocals.g_C_int` into
// 'standard' form by computing the inverse NTT and CRT lifting.
void nttInvAndCrtLiftCiphertexts(size_t num_per, FurtherDimsLocals furtherDimsLocals) {
    // #pragma omp parallel
    {
        // #pragma omp for
        for (size_t i = 0; i < num_per * n1 * n2; i++) {
            ntt_inverse(&furtherDimsLocals.scratch_cts1[crt_count*poly_len*i]);
        }

        size_t thr = omp_get_thread_num();
        size_t num_thr = omp_get_num_threads();
        cpu_crt(
            &furtherDimsLocals.cts[thr * num_per * n1 * n2 / num_thr * poly_len],
            &furtherDimsLocals.scratch_cts1[thr * num_per * n1 * n2 / num_thr * crt_count * poly_len],
            num_per * n1 * n2 / num_thr
        );
    }
}

// C:  i [num_per]   m [m2]        c [n2]   nz !!!!!!!!! wrong
// uint64_t *C       num_per,m2,n2,2,poly_len
// uint64_t *C_next  num_per,n1,n2,2,poly_len
// uint64_t *Q       n1,m2,2,poly_len

// Q:  nz            r [n1]        m [m2]
// C:  nz            i [num_per]   c [n2]   m [m2]
// Cn: i [num_per]   r [n1]        c [n2]   nz

void cpu_mul_query_by_ct(uint64_t *C_next, const uint64_t *Q, const uint64_t *C, size_t num_per) {
    // assumes: m2 * qq < 2^64
    // record("/a");
    uint64_t low_bits_mask_1 = (1UL << packed_offset_1) - 1;
    uint64_t low_bits_mask_2 = (1UL << packed_offset_diff) - 1;
    // __m256i mask_most = _mm256_set1_epi64x(-1);
    // __m256i mask_last = _mm256_set_epi64x(
    //     ((m2 % 4) >= 1) ? -1 : 0,
    //     ((m2 % 4) >= 2) ? -1 : 0,
    //     ((m2 % 4) >= 3) ? -1 : 0,
    //     ((m2 % 4) == 0) ? -1 : 0
    // );

    for (size_t z = 0; z < poly_len; z++) {
        for (size_t i = 0; i < num_per; i++) {
            for (size_t r = 0; r < n1; r++) {
                for (size_t c = 0; c < n2; c++) {
                    const uint64_t *C_p = &C[z * (num_per * n2 * m2) + i * (n2 * m2) + c * (m2)];
                    const uint64_t *Q_p = &Q[z * (n1*m2) + r * (m2)];

                    uint64_t sum_n_0 = 0;
                    uint64_t sum_n_1 = 0;

                    // size_t inner_limit = max_summed_pa_or_b_in_u64;
                    // size_t outer_limit = m2 / inner_limit;
                    // if (inner_limit > m2) {
                    //     inner_limit = 1;
                    //     outer_limit = m2;
                    // }

                    // __m256i sums_out_n0 = _mm256_setzero_si256();
                    // __m256i sums_out_n1 = _mm256_setzero_si256();
                    // for (size_t j = 0; j < outer_limit; j++) {
                    //     for (size_t k = 0; k < inner_limit; k++) {
                    //         size_t m = j * inner_limit + k;
                    //         uint64_t *a = &Q_p[m];
                    //         uint64_t *b = &C_p[m];


                    //     }
                    // }

                    // #ifdef MODULUS_56_BIT
                    //     uint64_t sum_n_2 = 0;
                    // #else
                    //     __uint128_t sum_n_2 = 0;
                    // #endif

                    #if defined(NO_CRT)
                    __uint128_t acc = 0;
                    for (size_t m = 0; m < m2; m++) {
                        uint64_t a = Q_p[m];
                        uint64_t b = C_p[m];

                        acc += (__uint128_t)a * b;
                    }
                    sum_n_0 = acc % p_i;
                    sum_n_1 = acc % b_i;
                    #else
                    #pragma unroll 8
                    for (size_t m = 0; m < m2; m++) {
                        uint64_t a = Q_p[m];
                        uint64_t b = C_p[m];

                        uint32_t a_lo = a;
                        uint32_t b_lo = b;

                        uint32_t a_hi = (a >> 32);
                        uint32_t b_hi = (b >> 32);

                        sum_n_0 += (uint64_t)a_lo * b_lo;
                        sum_n_1 += (uint64_t)a_hi * b_hi;
                        // #ifdef MODULUS_56_BIT
                        //     sum_n_2 += (uint64_t)a_hi_hi * b_hi_hi;
                        // #else
                        //     sum_n_2 += (__uint128_t)a_hi_hi * b_hi_hi;
                        // #endif
                    }
                    #endif

                    // __m256i sums_out_n0 = _mm256_setzero_si256();
                    // __m256i sums_out_n1 = _mm256_setzero_si256();

                    // for (size_t m = 0; m < m2; m+=4) {
                    //     const uint64_t *a = &Q_p[m];
                    //     const uint64_t *b = &C_p[m];
                    //     __m256i mask = (m + 4) < m2 ? mask_most : mask_last;
                    //     __m256i a_v = _mm256_and_si256(_mm256_loadu_si256((const __m256i *) a), mask);
                    //     __m256i b_v = _mm256_and_si256(_mm256_loadu_si256((const __m256i *) b), mask);
                    //     __m256i a_v_hi = _mm256_srli_epi64(a_v, 32);
                    //     __m256i b_v_hi = _mm256_srli_epi64(b_v, 32);

                    //     __m256i r = _mm256_mul_epu32(a_v, b_v);
                    //     __m256i r_hi = _mm256_mul_epu32(a_v_hi, b_v_hi);

                    //     sums_out_n0 = _mm256_add_epi64(sums_out_n0, r);
                    //     sums_out_n1 = _mm256_add_epi64(sums_out_n1, r_hi);
                    // }

                    // alignas(64) uint64_t sums_out_n0_u64[4];
                    // alignas(64) uint64_t sums_out_n1_u64[4];
                    // _mm256_store_si256 ((__m256i *)&sums_out_n0_u64, sums_out_n0);
                    // _mm256_store_si256 ((__m256i *)&sums_out_n1_u64, sums_out_n1);

                    // for (size_t idx = 0; idx < 4; idx++) {
                    //     sum_n_0 += sums_out_n0_u64[idx];
                    //     sum_n_1 += sums_out_n1_u64[idx];
                    // }

                    size_t C_next_idx = i * (n1*n2) + r * (n2) + c;
                    uint64_t *C_next_p = &C_next[C_next_idx * crt_count * poly_len];
                    C_next_p[z] = barrett_coeff(sum_n_0, 0);// % p_i;
                    C_next_p[poly_len + z] = barrett_coeff(sum_n_1, 1);
                    // C_next_p[2*poly_len + z] = sum_n_2 % b_i;
                }
            }
        }
    }
}

// inp: [num_polys * 2 * poly_len]
// out: [num_polys * poly_len]
void cpu_crt(uint64_t *out, const uint64_t *inp, size_t num_polys) {
    for (size_t i = 0; i < num_polys; i++) {
        for (size_t j = 0; j < poly_len; j++) {
            const uint64_t *base_inp = &inp[i * crt_count * poly_len];
            out[i * poly_len + j] = crt_compose(base_inp[j], base_inp[j + poly_len], 0);//, base_inp[j + 2 * poly_len]);
        }
    }
}

// inp: [num_polys * poly_len]
// out: [num_polys * 2 * poly_len]
void cpu_crt_to_ucompressed_and_ntt(uint64_t *out, const uint64_t *inp, size_t num_polys) {
    #pragma omp parallel for
    for (size_t i = 0; i < num_polys; i++) {
        for (size_t j = 0; j < poly_len; j++) {
            uint64_t val = inp[i * poly_len + j];
            out[i * crt_count * poly_len + j] = val % p_i;
            out[i * crt_count * poly_len + poly_len + j] = val % b_i;
            // out[i * crt_count * poly_len + 2*poly_len + j] = val % b_i;
        }

        ntt_forward(&out[i * crt_count * poly_len]);
    }
}

/**
 * @brief Performs first dimension processing.
 * @param output Output buffer for the result of the first dimension processing.
 *      @note output will get @param num_per ciphertexts,
 *            each of which is a matrix of polynomials in FFT form of size n1*n2.
 * @param reorientedCiphertexts Input buffer containing the reoriented ciphertexts.
 *      @note reorientedCipherTexts is @param dim0 ciphertexts of n1*n0 polynomials,
 *            indexed as (poly_len, dim0, n0, n1).
 * @param database Input buffer containing the database.
 *      @note database is the set of plaintexts as n0*n2 matrices,
 *            indexed as (poly_len, num_per, n2, dim0, n0).
 * @return void @param output.
*/
void multiplyQueryByDatabase(
    uint64_t * __restrict__ output,
    const uint64_t * __restrict__ reorientedCiphertexts,
    const uint64_t * __restrict__ database,
    size_t dim0,
    size_t num_per
) {
    size_t n1_padded = 4; // n1 rounded up to power of 2

    uint32_t low_bits_mask_1 = (1UL<< packed_offset_2) - 1;
    uint32_t low_bits_mask_2 = (1UL<< packed_offset_diff) - 1;

    #if defined(__AVX512F__) && !defined(NO_CRT)
        __m512i mask_1 = _mm512_set1_epi64(low_bits_mask_1);
        __m512i mask_2 = _mm512_set1_epi64(low_bits_mask_2);

        for (size_t z = 0; z < poly_len; z++) {
            size_t idx_a_base = z * (2*dim0*n1_padded);
            size_t idx_b_base = z * (num_per * n2 * dim0 * n0);
            if (random_data) idx_b_base = (z % dummyWorkingSet) * (num_per * n2 * dim0 * n0);

            for (size_t i = 0; i < num_per; i++) {
                for (size_t c = 0; c < n2; c++) {
                    // size_t idx_b = idx_b_base + i * (n2 * dim0 * n0) + c * (dim0 * n0);

                    size_t uroll_max = max_summed_pa_or_b_in_u64;
                    size_t inner_limit = uroll_max;
                    size_t outer_limit = dim0 * 2 / inner_limit;
                    if (dim0 * 2 < max_summed_pa_or_b_in_u64) {
                        inner_limit = dim0 * 2;
                        outer_limit = 1;
                    }

                    uint64_t sums_out_n0_u64_acc[6] = { 0, 0, 0 };
                    uint64_t sums_out_n2_u64_acc[6] = { 0, 0, 0 };

                    for (size_t o_jm = 0; o_jm < outer_limit; o_jm++) {
                        __m512i sums_out_n0 = _mm512_setzero_si512();
                        __m512i sums_out_n2 = _mm512_setzero_si512();

                        #pragma unroll 16
                        for (size_t i_jm = 0; i_jm < inner_limit/2; i_jm++) {
                            size_t jm = o_jm * inner_limit + (2*i_jm);

                            // __m128i b_inp = _mm_load_si128((__m128i const*)(&database[idx_b_base]));
                            // // __m128i b_inp = _mm_stream_load_si128((__m128i*)(&database[idx_b_base]));
                            // __m512i b_1_part = _mm512_maskz_broadcastq_epi64 (0b00001111, b_inp);
                            // __m512i b_2_part = _mm512_maskz_broadcastq_epi64 (0b11110000, _mm_bsrli_si128 (b_inp, 8));
                            // __m512i b = _mm512_or_epi64 (b_1_part, b_2_part);
                            // idx_b_base += 2;
                            uint64_t b_inp_1 = database[idx_b_base++];
                            uint64_t b_inp_2 = database[idx_b_base++];
                            __m512i b_1 = _mm512_set1_epi64(b_inp_1); // CPI: ?
                            __m512i b_2 = _mm512_set1_epi64(b_inp_2); // CPI: ?
                            __m512i b = _mm512_mask_blend_epi64(0b11110000, b_1, b_2);

                            // __m128i b_inp = _mm_load_si128((__m128i const*)(&database[idx_b_base]));
                            // __m512i b = _mm512_permutex_epi64 (_mm512_broadcast_i64x4(_mm512_castsi128_si512(b_inp)), 0b01010000);

                            const uint64_t *v_a = &reorientedCiphertexts[idx_a_base + jm * n1_padded];

                            __m512i a = _mm512_load_si512(v_a);
                            __m512i a_lo = a;//_mm512_and_epi64(a, mask_1); // CPI: 0.5
                            __m512i a_hi_hi = _mm512_srli_epi64(a, packed_offset_2); // CPI: 1
                            __m512i b_lo = b;//_mm512_and_epi64(b, mask_1);
                            __m512i b_hi_hi = _mm512_srli_epi64(b, packed_offset_2);

                            sums_out_n0 = _mm512_add_epi64(sums_out_n0, _mm512_mul_epu32(a_lo, b_lo));
                            sums_out_n2 = _mm512_add_epi64(sums_out_n2, _mm512_mul_epu32(a_hi_hi, b_hi_hi));
                        }

                        // reduce here, otherwise we will overflow
                        alignas(64) uint64_t sums_out_n0_u64[8];
                        alignas(64) uint64_t sums_out_n2_u64[8];
                        _mm512_store_si512(sums_out_n0_u64, sums_out_n0);
                        _mm512_store_si512(sums_out_n2_u64, sums_out_n2);

                        for (size_t idx = 0; idx < 3; idx++) {
                            uint64_t val = sums_out_n0_u64[idx] + sums_out_n0_u64[4 + idx];
                            sums_out_n0_u64_acc[idx] = (sums_out_n0_u64_acc[idx] + val) % q_intermediate;
                            // sums_out_n0_u64_acc[3 + idx] = (sums_out_n0_u64_acc[3 + idx] + sums_out_n0_u64[4 + idx]) % q_intermediate;
                        }
                        for (size_t idx = 0; idx < 3; idx++) {
                            uint64_t val = sums_out_n2_u64[idx] + sums_out_n2_u64[4 + idx];
                            sums_out_n2_u64_acc[idx] = (sums_out_n2_u64_acc[idx] + val) % b_i;
                            // sums_out_n2_u64_acc[3 + idx] = (sums_out_n2_u64_acc[3 + idx] + sums_out_n2_u64[4 + idx]) % b_i;
                        }
                    }

                    // output n0
                    size_t n = 0;
                    size_t idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = barrett_coeff(sums_out_n0_u64_acc[0], 0);
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = barrett_coeff(sums_out_n0_u64_acc[1], 0);
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = barrett_coeff(sums_out_n0_u64_acc[2], 0);

                    // output n1
                    n = 1;
                    idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = barrett_coeff(sums_out_n2_u64_acc[0], 1);
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = barrett_coeff(sums_out_n2_u64_acc[1], 1);
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = barrett_coeff(sums_out_n2_u64_acc[2], 1);

                    // // output n2
                    // n = 2;
                    // idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    // output[idx_c] = (sums_out_n2_u64_acc[0]);// % b_i;
                    // idx_c += (n2*crt_count*poly_len);
                    // output[idx_c] = (sums_out_n2_u64_acc[1]);// % b_i;
                    // idx_c += (n2*crt_count*poly_len);
                    // output[idx_c] = (sums_out_n2_u64_acc[2]);// % b_i;
                }
            }
        }
    #elif defined(__AVX2__) && !defined(NO_CRT)
        // static_assert(false, "No! Using AVX2 only! bad!");
        // __m256i mask_1 = _mm256_set1_epi64x(low_bits_mask_1);
        // __m256i mask_2 = _mm256_set1_epi64x(low_bits_mask_2);
        // std::cerr << "Using AVX2. Exiting." << std::endl;
        // exit(1);

        for (size_t z = 0; z < poly_len; z++) {
            size_t idx_a_base = z * (2*dim0*n1_padded);
            size_t idx_b_base = z * (num_per * n2 * dim0 * n0);
            if (random_data) idx_b_base = 0;//(rand() % 64) * (num_per * n2 * dim0 * n0);
            if (random_data) idx_a_base = 0;//(rand() % 64) * (num_per * n2 * dim0 * n0);
            // if (true) {//(z % 256 == 0) {
            //     b_inp = _mm256_stream_load_si256((__m256i const*)(&database[(z%4) * 4]));
            //     a1 = _mm256_stream_load_si256((__m256i const *) &reorientedCiphertexts[(z%4) * 4]);
            //     a2 = _mm256_stream_load_si256((__m256i const *) &reorientedCiphertexts[(z%4) * 4 + 4]);
            // }

            for (size_t i = 0; i < num_per; i++) {
                for (size_t c = 0; c < n2; c++) {
                    // size_t idx_b = idx_b_base + i * (n2 * dim0 * n0) + c * (dim0 * n0);

                    size_t uroll_max = max_summed_pa_or_b_in_u64;
                    size_t inner_limit = uroll_max;
                    size_t outer_limit = dim0 * 2 / inner_limit;

                    __uint128_t sums_out_n0_u64_acc[3] = { 0, 0, 0 };
                    __uint128_t sums_out_n2_u64_acc[3] = { 0, 0, 0 };

                    for (size_t o_jm = 0; o_jm < outer_limit; o_jm++) {
                        __m256i sums_out_n0_1 = _mm256_setzero_si256();
                        __m256i sums_out_n2_1 = _mm256_setzero_si256();
                        __m256i sums_out_n0_2 = _mm256_setzero_si256();
                        __m256i sums_out_n2_2 = _mm256_setzero_si256();
                        __m256i sums_out_n0_3 = _mm256_setzero_si256();
                        __m256i sums_out_n2_3 = _mm256_setzero_si256();
                        __m256i sums_out_n0_4 = _mm256_setzero_si256();
                        __m256i sums_out_n2_4 = _mm256_setzero_si256();

                        for (size_t i_jm = 0; i_jm < inner_limit/4; i_jm++) {
                            size_t jm = o_jm * inner_limit + 4*(i_jm);
                            // __m256i b_inp = _mm256_load_si256((__m256i const*)(&database[idx_b_base]));
                            // __m256i b_inp_hi = _mm256_shuffle_epi32(b_inp, 0b10110001);
                            // __m256i b1 = _mm256_permute4x64_epi64(b_inp, 0b00000000);
                            // __m256i b2 = _mm256_permute4x64_epi64(b_inp, 0b01010101);
                            // __m256i b3 = _mm256_permute4x64_epi64(b_inp, 0b10101010);
                            // __m256i b4 = _mm256_permute4x64_epi64(b_inp, 0b11111111);
                            // __m256i b1 = _mm256_set1_epi64x(database[idx_b_base]);
                            // __m256i b2 = _mm256_set1_epi64x(database[idx_b_base+1]);
                            // __m256i b3 = _mm256_set1_epi64x(database[idx_b_base+2]);
                            // __m256i b4 = _mm256_set1_epi64x(database[idx_b_base+3]);
                            uint32_t *db_ptr = (uint32_t *)&database[idx_b_base];
                            __m256i b1 = _mm256_set1_epi32(db_ptr[0]);
                            __m256i b2 = _mm256_set1_epi32(db_ptr[2]);
                            __m256i b3 = _mm256_set1_epi32(db_ptr[4]);
                            __m256i b4 = _mm256_set1_epi32(db_ptr[6]);
                            __m256i b1_hi = _mm256_set1_epi32(db_ptr[1]);
                            __m256i b2_hi = _mm256_set1_epi32(db_ptr[3]);
                            __m256i b3_hi = _mm256_set1_epi32(db_ptr[5]);
                            __m256i b4_hi = _mm256_set1_epi32(db_ptr[7]);
                            idx_b_base += 4;

                            const uint64_t *v_a1 = &reorientedCiphertexts[idx_a_base + jm * n1_padded];
                            __m256i a1 = _mm256_load_si256((__m256i const *) &v_a1[0*n1_padded]);
                            __m256i a2 = _mm256_load_si256((__m256i const *) &v_a1[1*n1_padded]);
                            __m256i a3 = _mm256_load_si256((__m256i const *) &v_a1[2*n1_padded]);
                            __m256i a4 = _mm256_load_si256((__m256i const *) &v_a1[3*n1_padded]);

                            __m256i r1 = _mm256_mul_epu32(a1, b1);
                            __m256i r2 = _mm256_mul_epu32(a2, b2);
                            __m256i r3 = _mm256_mul_epu32(a3, b3);
                            __m256i r4 = _mm256_mul_epu32(a4, b4);

                            __m256i a1_hi = _mm256_srli_epi64(a1, 32);
                            __m256i a2_hi = _mm256_srli_epi64(a2, 32);
                            __m256i a3_hi = _mm256_srli_epi64(a3, 32);
                            __m256i a4_hi = _mm256_srli_epi64(a4, 32);

                            __m256i r1_hi = _mm256_mul_epu32(a1_hi, b1_hi);
                            __m256i r2_hi = _mm256_mul_epu32(a2_hi, b2_hi);
                            __m256i r3_hi = _mm256_mul_epu32(a3_hi, b3_hi);
                            __m256i r4_hi = _mm256_mul_epu32(a4_hi, b4_hi);

                            sums_out_n0_1 = _mm256_add_epi64(sums_out_n0_1, r1);
                            sums_out_n0_2 = _mm256_add_epi64(sums_out_n0_2, r2);
                            sums_out_n0_3 = _mm256_add_epi64(sums_out_n0_3, r3);
                            sums_out_n0_4 = _mm256_add_epi64(sums_out_n0_4, r4);

                            sums_out_n2_1 = _mm256_add_epi64(sums_out_n2_1, r1_hi);
                            sums_out_n2_2 = _mm256_add_epi64(sums_out_n2_2, r2_hi);
                            sums_out_n2_3 = _mm256_add_epi64(sums_out_n2_3, r3_hi);
                            sums_out_n2_4 = _mm256_add_epi64(sums_out_n2_4, r4_hi);
                        }

                        sums_out_n0_1 = _mm256_add_epi64(_mm256_add_epi64(sums_out_n0_1, sums_out_n0_2), _mm256_add_epi64(sums_out_n0_3, sums_out_n0_4));
                        sums_out_n2_1 = _mm256_add_epi64(_mm256_add_epi64(sums_out_n2_1, sums_out_n2_2), _mm256_add_epi64(sums_out_n2_3, sums_out_n2_4));

                        // reduce here, otherwise we will overflow
                        alignas(64) uint64_t sums_out_n0_u64[4];
                        alignas(64) uint64_t sums_out_n2_u64[4];
                        _mm256_store_si256 ((__m256i *)&sums_out_n0_u64, sums_out_n0_1);
                        _mm256_store_si256 ((__m256i *)&sums_out_n2_u64, sums_out_n2_1);

                        for (size_t idx = 0; idx < 3; idx++)
                            sums_out_n0_u64_acc[idx] = (sums_out_n0_u64_acc[idx] + (__uint128_t)sums_out_n0_u64[idx]);// % q_intermediate;
                        for (size_t idx = 0; idx < 3; idx++)
                            sums_out_n2_u64_acc[idx] = (sums_out_n2_u64_acc[idx] + (__uint128_t)sums_out_n2_u64[idx]);// % b_i;
                    }

                    // for (size_t idx = 0; idx < 3; idx++)
                    //     output[idx] = (sums_out_n0_u64_acc[idx] + output[idx]);
                    // for (size_t idx = 0; idx < 3; idx++)
                    //     output[idx] = (sums_out_n2_u64_acc[idx] + output[idx]);

                    // output n0
                    size_t n = 0;
                    size_t idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_n0_u64_acc[0] % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_u64_acc[1] % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_u64_acc[2] % p_i;

                    // // output n1
                    // n = 1;
                    // idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    // output[idx_c] = sums_out_n0_u64_acc[0] % a_i;
                    // idx_c += (n2*crt_count*poly_len);
                    // output[idx_c] = sums_out_n0_u64_acc[1] % a_i;
                    // idx_c += (n2*crt_count*poly_len);
                    // output[idx_c] = sums_out_n0_u64_acc[2] % a_i;

                    // output n2
                    n = 1;
                    idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_n2_u64_acc[0] % b_i;//reduction_u128_qq(sums_out_n2_u64_acc[0]);// % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n2_u64_acc[1] % b_i;//reduction_u128_qq(sums_out_n2_u64_acc[1]);// % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n2_u64_acc[2] % b_i;//reduction_u128_qq(sums_out_n2_u64_acc[2]);// % b_i;
                }
            }
        }
    #elif defined(NO_CRT) // incorrect
        std::cerr << "Using NO_CRT. Exiting." << std::endl;
        exit(1);
        for (size_t z = 0; z < poly_len; z++) {
            size_t idx_a_base = z * (2*dim0*n1_padded);
            size_t idx_b_base = z * (num_per * n2 * dim0 * n0);
            if (random_data) idx_b_base = (rand() % dummyWorkingSet) * (num_per * n2 * dim0 * n0);

            for (size_t i = 0; i < num_per; i++) {
                for (size_t c = 0; c < n2; c++) {
                    __uint128_t sums_out_0 = 0, sums_out_1 = 0, sums_out_2 = 0;

                    // #pragma unroll 16
                    for (size_t jm = 0; jm < dim0*2; jm++) {
                        uint64_t b = database[idx_b_base++];

                        const uint64_t *v_a = &reorientedCiphertexts[idx_a_base + jm * n1_padded];
                        uint64_t v_a0 = v_a[0];
                        uint64_t v_a1 = v_a[1];
                        uint64_t v_a2 = v_a[2];

                        // do n0
                        sums_out_0 += (v_a0) * (__uint128_t)b;
                        sums_out_1 += (v_a1) * (__uint128_t)b;
                        sums_out_2 += (v_a2) * (__uint128_t)b;
                    }

                    // output n0
                    size_t n = 0;
                    size_t idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_0 % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_1 % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_2 % p_i;

                    // output n1
                    n = 1;
                    idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_0 % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_1 % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_2 % b_i;
                }
            }
        }
    #else
        // low_bits_mask_1 = (1UL<< packed_offset_1) - 1;
        for (size_t z = 0; z < poly_len; z++) {
            size_t idx_a_base = z * (2*dim0*n1_padded);
            size_t idx_b_base = z * (num_per * n2 * dim0 * n0);
            if (random_data) idx_b_base = (rand() % dummyWorkingSet) * (num_per * n2 * dim0 * n0);

            for (size_t i = 0; i < num_per; i++) {
                for (size_t c = 0; c < n2; c++) {
                    // size_t idx_b = idx_b_base + i * (n2 * dim0 * n0) + c * (dim0 * n0);

                    __uint128_t sums_out_n0_0 = 0, sums_out_n0_1 = 0, sums_out_n0_2 = 0;
                    __uint128_t sums_out_n1_0 = 0, sums_out_n1_1 = 0, sums_out_n1_2 = 0;

                    // #pragma unroll 16
                    for (size_t jm = 0; jm < dim0*2; jm++) {
                        uint64_t b = database[idx_b_base++];

                        const uint64_t *v_a = &reorientedCiphertexts[idx_a_base + jm * n1_padded];
                        uint64_t v_a0 = v_a[0];
                        uint64_t v_a1 = v_a[1];
                        uint64_t v_a2 = v_a[2];

                        uint32_t b_lo = b;
                        uint32_t b_hi = b >> 32L;

                        uint32_t v_a0_lo = v_a0;
                        uint32_t v_a0_hi = v_a0 >> 32L;

                        uint32_t v_a1_lo = v_a1;
                        uint32_t v_a1_hi = v_a1 >> 32L;

                        uint32_t v_a2_lo = v_a2;
                        uint32_t v_a2_hi = v_a2 >> 32L;

                        // do n0
                        sums_out_n0_0 += (v_a0_lo) * (uint64_t)b_lo;
                        sums_out_n0_1 += (v_a1_lo) * (uint64_t)b_lo;
                        sums_out_n0_2 += (v_a2_lo) * (uint64_t)b_lo;

                        // do n1
                        sums_out_n1_0 += ((uint64_t)v_a0_hi) * b_hi;
                        sums_out_n1_1 += ((uint64_t)v_a1_hi) * b_hi;
                        sums_out_n1_2 += ((uint64_t)v_a2_hi) * b_hi;
                    }

                    // output n0
                    size_t n = 0;
                    size_t idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_n0_0 % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_1 % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_2 % p_i;

                    // output n1
                    n = 1;
                    idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_n1_0 % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n1_1 % b_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n1_2 % b_i;
                }
            }
        }
    #endif
}

MatPoly G1(n0, m1, false);
MatPoly G2(n1, m2, false);
MatPoly G_hat(1, mh, false);

string dbFilename;
string dataFilename;
string outputErrFilename;
bool has_file = false;
bool has_data = false;
bool load = false;
bool server = false;
bool client = false;
bool checking_for_debug = false;
fstream fs;
fstream fsData;

uint64_t *B;
MatPoly pt(n0, n0);
MatPoly pts_encd(n0, n2);
MatPoly pt_correct(n0, n0);

void generate_gadgets() {
    get_gadget_1(G1);
    buildGadget(G2);
    buildGadget(G_hat);
}

enum Container{
    Hash,
    Sequence
};

namespace Process {
    // [DEBUG] const std::filesystem::path base("../../");
    const std::filesystem::path base("/tmp/Spiral");
    const std::filesystem::path processPath
        = base / std::filesystem::path("Database/Colouring/Data");
        //= base / std::filesystem::path("Database/PBC/Data");
    const std::filesystem::path queryStoragePath
        = base / std::filesystem::path("Query_Storage");

    std::filesystem::path dataSpace(const std::string& filename) {
        return processPath / std::filesystem::path(filename);
    }

    namespace Data {
        typedef boost::multi_index::multi_index_container<
            std::string,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_unique<
                    boost::multi_index::identity<std::string>
                >,
                boost::multi_index::sequenced<>
            >
        > HashStore;

        // Note: const is only modified during load.
        const HashStore& hashStore(const bool set = false) {
            static HashStore hashes;
            if (!set) assert(!hashes.get<Container::Hash>().empty());
            return hashes;
        }

        void useSIMDParser(const std::filesystem::path& jsonFile) {
            simdjson::ondemand::parser simdParser;
            auto jsonHash = simdjson::padded_string::load(jsonFile.c_str());
            auto& hashStoreRef = const_cast<HashStore&>(hashStore(true));
            for (simdjson::ondemand::object container : simdParser.iterate(jsonHash)) {
                for (auto element : container) {
                    simdjson::ondemand::value hash = element.value();
                    hashStoreRef.get<Container::Sequence>().emplace_back(
                            static_cast<std::string_view>(hash.get_string())
                    );
                }
            }
        }

        void useInsertionPreservedParser(const std::filesystem::path& jsonFile) {
            std::ifstream jsonFileStream(jsonFile);
            nlohmann::ordered_json jsonData;
            jsonFileStream >> jsonData;
            auto& hashStoreRef = const_cast<HashStore&>(hashStore(true));
            for (auto& element : jsonData[0].items()) {
                std::string hash = element.value().get<std::string>();
                std::transform(
                    hash.begin(), hash.end(), hash.begin(),
                    [](unsigned char c){ return std::tolower(c); }
                );
                hashStoreRef.get<Container::Sequence>().push_back(hash);
            }
        }

        void loadHashes(const std::filesystem::path& jsonFile, const bool insertionOrderLoad = false) {
            // [Note] Preserving insertion order tends to be notably
            //        slower on large files, which may not be ideal
            //        for development. Insertion order is only a requisite
            //        for the final architecture integration.
            Log::cout << "Hashes to be loaded in "
                      << (insertionOrderLoad ? "insertion" : "non-insertion")
                      << " order." << std::endl;
            if (insertionOrderLoad) {
                useInsertionPreservedParser(jsonFile);
            } else {
                useSIMDParser(jsonFile);
            }
        }

        std::string retrieveHashAtIndex(const size_t index) {
            const auto& hashStoreSequence = hashStore().get<Container::Sequence>();
            if (index < hashStoreSequence.size()) {
                auto iterator = hashStoreSequence.begin();
                std::advance(iterator, index);
                return *iterator;
            } else {
                throw std::out_of_range("Hash index out of range from hash store.");
            }
        }
    }
}

template <typename T>
bool isBiEvenlyDivisible(const T& a, const T& b) {
    return a % b == 0 or b % a == 0;
}

struct PlaintextConversionConfig {
    const size_t hashLength = 64;
    const size_t plaintextModulus = p_db;
    const size_t hexadecimalRange = 16;
    const std::unordered_set<size_t> supportedPlaintextModuli{4, 16, 256};
    const double coefficientsPerCharacter = determineCoefficientsPerCharacter();
    const size_t coefficientsPerHash = ceil(static_cast<double>(hashLength) * coefficientsPerCharacter);
    size_t totalCoefficients {};
    size_t hashesPerPoly {};

    explicit PlaintextConversionConfig(const size_t n) : PlaintextConversionConfig() {
        totalCoefficients = n * n * poly_len;
        assert(totalCoefficients > coefficientsPerHash);
        assert(totalCoefficients % coefficientsPerHash == 0);
        hashesPerPoly = totalCoefficients / coefficientsPerHash;
    }

    explicit PlaintextConversionConfig(MatPoly& out) : PlaintextConversionConfig() {
        assert(!out.isNTT);
        totalCoefficients = out.rows * out.cols * poly_len;
        assert(totalCoefficients > coefficientsPerHash);
        assert(totalCoefficients % coefficientsPerHash == 0);
        hashesPerPoly = totalCoefficients / coefficientsPerHash;
    }

private: PlaintextConversionConfig() {
        const bool isHexadecimalWritable = isBiEvenlyDivisible(plaintextModulus, hexadecimalRange);
        assert(isHexadecimalWritable);
        const bool isSupportedPlaintextModulus =
                supportedPlaintextModuli.find(plaintextModulus) != supportedPlaintextModuli.end();
        assert(isSupportedPlaintextModulus);
    }

private: double determineCoefficientsPerCharacter() const {
        assert(isBiEvenlyDivisible(plaintextModulus, hexadecimalRange));
        double writeSurface = -1.0;
        if (plaintextModulus < hexadecimalRange) {
            // writeSurface = static_cast<double>(hexadecimalRange) / static_cast<double>(plaintextModulus);
            writeSurface = 2;
        } else if (plaintextModulus == hexadecimalRange) {
            writeSurface = 1;
        } else if (plaintextModulus > hexadecimalRange) {
            double bitCount = log2(plaintextModulus);
            // Note: 16 hex characters can be represented in 4-bits.
            assert(hexadecimalRange == 16);
            double characterPerCoefficient = bitCount / 4;
            writeSurface = 1 / characterPerCoefficient;
        }
        assert(writeSurface > 0.0);
        return writeSurface;
    }
};

namespace HexToolkit {
    const char base16ToHex[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };

    int hexCharToBase16(char hexChar) {
        if (hexChar >= '0' && hexChar <= '9') return hexChar - '0';
        if (hexChar >= 'a' && hexChar <= 'f') return hexChar - 'a' + 10;
        if (hexChar >= 'A' && hexChar <= 'F') return hexChar - 'A' + 10;
        throw std::invalid_argument("Invalid hexadecimal character");
    }

    std::map<int, std::vector<int>> hexBase16ToBucketMap {
            {0, {0, 0}},
            {1, {0, 1}},
            {2, {0, 2}},
            {3, {0, 3}},
            {4, {1, 0}},
            {5, {1, 1}},
            {6, {1, 2}},
            {7, {1, 3}},
            {8, {2, 0}},
            {9, {2, 1}},
            {10, {2, 2}},
            {11, {2, 3}},
            {12, {3, 0}},
            {13, {3, 1}},
            {14, {3, 2}},
            {15, {3, 3}}
    };
}

void generatePaddedPoly(MatPoly& M, const int iterationCount, const int dummyValue = 0) {
    /*std::cout << "\r [" << iterationCount
              << "] Encoding dummy value into remaining polynomials."
              << std::string(100, ' ') << std::flush;*/
    for (size_t i = 0; i < M.rows * M.cols * poly_len; i++) {
        assert(dummyValue >= 0 and dummyValue < p_db);
        M.data[i] = dummyValue;
    }
}

void logHashEncoding(
    const std::string& hash,
    const size_t recordCount,
    const size_t hashCountInPoly,
    const PlaintextConversionConfig& config
) {
    /*std::cout << "\r [" << recordCount << " | "
              << hashCountInPoly << "]" << (hashCountInPoly < 100 ? " ": "") << (recordCount < 100 ? " ": "") << " Encoding "
              << hash << " into polynomials under " << config.coefficientsPerCharacter
              << " coefficients/character." << std::flush;*/
}

uint64_t performBitPacking(const uint8_t a, const uint8_t b) {
    return (a << 4) | b;
}

std::pair<uint8_t, uint8_t> unpackBit(const uint64_t packed) {
    return std::make_pair((packed >> 4) & 0xF, packed & 0xF);
}

// Returns: Index where write has ended.
size_t writeHashToPoly(
    MatPoly& out,
    const PlaintextConversionConfig& config,
    const std::string& hash,
    size_t writeHead,
    const size_t recordCount
) {
    assert(!out.isNTT);
    assert(writeHead < config.totalCoefficients);
    assert(writeHead + config.coefficientsPerHash <= config.totalCoefficients);
    const bool writeHeadIsAligned = isBiEvenlyDivisible(writeHead, config.coefficientsPerHash);
    assert(writeHeadIsAligned);
    logHashEncoding(hash, recordCount, writeHead / config.coefficientsPerHash, config);
    size_t characterPointer = 0;
    for (size_t coefficientIndex = 0; coefficientIndex < config.coefficientsPerHash; coefficientIndex++) {
        const bool isHashExhausted = characterPointer >= config.hashLength;
        if (!isHashExhausted) {
            const int hexToWrite = HexToolkit::hexCharToBase16(hash.at(characterPointer++));
            assert(hexToWrite >= 0 && hexToWrite < 16);
            if (config.plaintextModulus == 16) {
                out.data[writeHead + coefficientIndex] = hexToWrite;
            } else if (config.plaintextModulus == 256) {
                const int nextHexToWrite = HexToolkit::hexCharToBase16(hash.at(characterPointer++));
                out.data[writeHead + coefficientIndex] = performBitPacking(hexToWrite, nextHexToWrite);
            } else if (config.plaintextModulus == 4) {
                if (HexToolkit::hexBase16ToBucketMap.find(hexToWrite) == HexToolkit::hexBase16ToBucketMap.end()) {
                    throw std::runtime_error("Invalid hex character: " + std::to_string(hexToWrite));
                }
                const std::vector<int>& buckets = HexToolkit::hexBase16ToBucketMap[hexToWrite];
                for (size_t bucketIndex = 0;
                     bucketIndex < static_cast<size_t>(config.coefficientsPerCharacter); bucketIndex++) {
                    out.data[writeHead + coefficientIndex + bucketIndex] = buckets[bucketIndex];
                }
                // [NOTE] Advance the coefficient index by the number of buckets written.
                coefficientIndex += static_cast<size_t>(config.coefficientsPerCharacter) - 1;
            }
        }
    }
    const bool isHashExhausted = characterPointer == config.hashLength;
    if (!isHashExhausted) {
        throw std::runtime_error("Failed to encode entire hash into polynomial.");
    }
    return writeHead + config.coefficientsPerHash;
}

template <typename Iterator>
void generatePackedPoly(MatPoly& out, Iterator& hashIterator, const size_t recordCount) {
    const PlaintextConversionConfig config(out);
    // Note: Socket is the number of coefficients required to store a single char.
    for (size_t socketIndex = 0; socketIndex < config.hashesPerPoly; socketIndex++) {
        const bool hashStoreExhausted = hashIterator ==
                                        Process::Data::hashStore().get<Container::Sequence>().end();
        if (!hashStoreExhausted) {
            const std::string& hash = *hashIterator;
            size_t writeHead = socketIndex * config.coefficientsPerHash;
            writeHead = writeHashToPoly(out, config, hash, writeHead, recordCount);
            hashIterator++;
            if (writeHead > config.totalCoefficients) {
                throw std::runtime_error("Write head exceeded total coefficients.");
            }
        } else {
            // Note: This should ideally only happen in the very last poly.
            const int dummyValue = 0;
            for (size_t index = 0; index < config.coefficientsPerHash; index++) {
                out.data[socketIndex * config.coefficientsPerHash + index] = dummyValue;
            }
        }
    }
}

void load_db() {
    // Note: This is hard-coded as 256.
    size_t dim0 = 1 << num_expansions;
    // Note: This resolves to 128.
    size_t num_per = total_n / dim0;
    if (random_data) throw std::runtime_error("Random data has been deleted.");
    // Note: Size of the database (B).
    // Note:
    //      num_items = dim0 * num_per
    //      trials = n0 * n2
    size_t num_bytes_B = sizeof(uint64_t) * dim0 * num_per * n0 * n2 * poly_len;//2 * poly_len;
    NativeLog::cout << "num_bytes_B: " << num_bytes_B << endl;
    B = (uint64_t *)aligned_alloc(64, num_bytes_B);
    memset(B, 0, num_bytes_B);

    // Note: BB is temporary database?
    uint64_t *BB = (uint64_t *)malloc(n0 * n2 * crt_count * poly_len * sizeof(uint64_t));
    size_t numBytesPlaintextRaw = n0 * n0 * num_bits_q * poly_len / 8;  // Poly-len is 2048.
    // Note: Plain-text (pt) raw. Resolves to 13312 bytes.
    uint64_t *pt_raw = (uint64_t *)malloc(numBytesPlaintextRaw);
    memset(pt_raw, 0, numBytesPlaintextRaw);
    uint64_t *pt_buf = (uint64_t *)malloc(n0 * n0 * crt_count * poly_len * sizeof(uint64_t));
    memset(pt_buf, 0, n0 * n0 * crt_count * poly_len * sizeof(uint64_t));

    NativeLog::cout << "starting generation of db" << endl;
    // Note: Figure out the purpose of H and F. Don't think F is used anywhere other than to calculate HF.
    MatPoly H_nttd = to_ntt(H_mp);
    uint64_t *H_encd = H_nttd.data;
    MatPoly pt_tmp(n0, n0, false);
    MatPoly pt_encd_raw(n0, n2, false);
    pts_encd = MatPoly(n0, n2);
    pt = MatPoly(n0, n0);
    auto hashesToLoadIterator = Process::Data::hashStore().get<Container::Sequence>().begin();

    int loadedPolyCount = 0;
    int paddedPolynomialCount = 0;
    int databaseRecordCount = 0;
    // Note: For index i in the database.
    for (size_t i = 0; i < total_n; i++) {
        const bool hashStoreExhausted = hashesToLoadIterator ==
                                        Process::Data::hashStore().get<Container::Sequence>().end();
        if (!hashStoreExhausted) {
            generatePackedPoly(pt_tmp, hashesToLoadIterator, databaseRecordCount);
            loadedPolyCount++;
        } else {
            generatePaddedPoly(pt_tmp, databaseRecordCount);
            paddedPolynomialCount++;
        }
        databaseRecordCount++;
        // std::this_thread::sleep_for(std::chrono::milliseconds(5));
        // generateRandomPt(pt_tmp);
        pt_encd_raw = pt_tmp;
        // Note: For every randomly generated polynomial.
        for (size_t pol = 0; pol < n0 * n2 * poly_len; pol++) {
            int64_t val = (int64_t) pt_encd_raw.data[pol];
            assert(val >= 0 && val < p_db);
            // Note: Recenter mod.
            //      p_db = small_modulus
            //      Q_i  = large_modulus
            if (val >= (p_db / 2)) {
                val = val - (int64_t)p_db;
            }
            if (val < 0) {
                val += Q_i;
            }
            assert(val >= 0 && val < Q_i);
            pt_encd_raw.data[pol] = val;
        }
        // Note: pts_encd is a global variable that is only used in load_db.
        to_ntt(pts_encd, pt_encd_raw);
        // Note: If index is equal to the querying index, record it for verification.
        if (i == IDX_TARGET) {
            cop(pt_encd_correct, pts_encd);
            cop(pt_real, pt_tmp);
            to_ntt(pt, pt_tmp);
            cop(pt_correct, pt);
        }

        // b': i c n z j m
        size_t ii = i % num_per;  // Note: num_per is 128.
        size_t j = i / num_per;
        for (size_t m = 0; m < n0; m++) {
            for (size_t c = 0; c < n2; c++) {
                // Question: What is the coeff_count and how does that correspond to poly_len. They both are 2048.
                // Note: pts_encd is a temporary store for an NTT-compliant random polynomial.
                // Debug: crt_count is 2.
                // Question: What is it indexing?
                memcpy(BB, &pts_encd.data[(m * n2 + c) * crt_count * coeff_count], crt_count * coeff_count * sizeof(uint64_t));
                // Note: For the poly_len.
                for (size_t z = 0; z < poly_len; z++) {
                    size_t idx = z * (num_per * n2 * dim0 * n0) +
                                 ii * (n2 * dim0 * n0) +
                                 c * (dim0 * n0) +
                                 j * (n0) + m;
                    // Note: Packs two 32 bit integers
                    //       (BB[z] and (BB[poly_len + z] << 32))
                    //       into one 64 bit integer (B[idx]).
                    B[idx] = BB[z] | (BB[poly_len + z] << 32);
                }
            }
        }
    }
    std::cout << std::endl;  // For carriage return.
    free(BB);

    Log::cout << "Database N is " << total_n << "." << std::endl;
    Log::cout << "Encoded " << Process::Data::hashStore().size()
              << " hashes into " << loadedPolyCount << " polys and padded "
              << paddedPolynomialCount << " polynomials." << std::endl;

    if (hashesToLoadIterator != Process::Data::hashStore().get<Container::Sequence>().end()) {
        // throw std::runtime_error(
        //     "Failed to load all hashes into the database. "
        //     "Is the database too small?"
        // );
        std::cerr << "Failed to load all hashes into the database. "
                 << "Is the database too small?" << std::endl;
    }

    if (has_file) {
        fs.close();
    }

    NativeLog::cout << "done loading/generating db." << endl;
}

void do_test();
void do_server();
void runSeparationTest();
void performPreTests();

#ifdef MAKE_QUERY
void make_query();
#endif

void do_MatPol_test() {
    MatPoly A_mp(3, 6, false);
    uniform_matrix(A_mp);
    MatPoly A_mp_ntt(3, 6);
    to_ntt(A_mp_ntt, A_mp);
    MatPoly A_mp_back(3, 6, false);
    from_ntt(A_mp_back, A_mp_ntt);
    bool worked = is_eq(A_mp, A_mp_back);
    if (!worked) {
        cout << "do_MatPol_test failed" << endl;
        exit(1);
    }
}

void testScalToMatAndConversion();
#ifdef TIMERLOG
namespace GlobalTimer {
    inline std::map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> activeTimers {};

    void set(const std::string& timerName) {
        assert(!activeTimers.count(timerName));
        activeTimers[timerName] = std::chrono::high_resolution_clock::now();
        std::cout << "[" << UnixColours::YELLOW << "Begin"
                  << UnixColours::RESET << "] "
                  << UnixColours::GREEN << timerName
                  << "." << UnixColours::RESET << std::endl;
    }

    void stop(const std::string& timerName) {
        assert(activeTimers.count(timerName));
        auto start = activeTimers[timerName];
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "[" << UnixColours::BLUE << duration
                  << "µs" << UnixColours::RESET << "] "
                  << UnixColours::RED << timerName << "."
                  << UnixColours::RESET << std::endl;
        activeTimers.erase(timerName);
    }

};
#else
namespace GlobalTimer {
    void set(const std::string& timerName) {}
    void stop(const std::string& timerName) {}
};
#endif


namespace Evaluation {
    std::map<std::string, std::vector<double>> metrics {};
    const std::filesystem::path metricsPath("/tmp/Spiral/Evaluation/Data");

    void set(const std::string& metric, const double value) {
        metrics[metric].push_back(value);
    }

    inline namespace Timers {
        inline std::map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> activeTimers {};

        void set(const std::string& timerName) {
            assert(!activeTimers.count(timerName));
            activeTimers[timerName] = std::chrono::high_resolution_clock::now();
        }

        void stop(const std::string& timerName) {
            assert(activeTimers.count(timerName));
            auto start = activeTimers[timerName];
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            metrics[timerName].push_back(static_cast<double>(duration));
            activeTimers.erase(timerName);
        }
    }

    namespace Calculate {
        double queryCost() {
            auto b_per_elem = (size_t)((double) poly_len * logQ / 8.0);
            size_t dim0_query_size_kb = (query_elems_first + query_elems_rest) * n0 * b_per_elem;
            size_t dim1s_query_size_kb = 0;
            return static_cast<double>(dim0_query_size_kb + dim1s_query_size_kb);
        }

        double responseCost() {
            double pt_mod = log2((double) p_db);
            size_t total_resp_size_kb = ((n0 * n0 * (double) poly_len * (pt_mod + 2)) +
                                  (n0 * (double) poly_len * (double) bits_to_hold_arb_qprime)) / 8.0;
            return static_cast<double>(total_resp_size_kb);
        }

        double totalCost() {
            return queryCost() + responseCost();
        }

        double rate() {
            PlaintextConversionConfig run_configuration = PlaintextConversionConfig(n0);
            return 1.0 / static_cast<double>(run_configuration.hashesPerPoly);
        }
    }

    void saveStaticMetrics() {
        metrics["Rate.client"].push_back(Calculate::rate());
        metrics["Query_Cost.communication"].push_back(Calculate::queryCost());
        metrics["Response_Cost.communication"].push_back(Calculate::responseCost());
        metrics["Total_Cost.communication"].push_back(Calculate::totalCost());
    }

    void writeMetrics() {
        saveStaticMetrics();
        assert(!metrics.empty());
        for (const auto& [metricName, metricValues] : metrics) {
            assert(!metricValues.empty());
        }
        if (!std::filesystem::exists(metricsPath)) {
            std::filesystem::create_directory(metricsPath);
        }
        if (!std::filesystem::is_empty(metricsPath)) {
            std::string command = "rm -rf " + metricsPath.string() + "/*";
            system(command.c_str());
        }
        for (const auto& [metricName, metricValues] : metrics) {
            std::ofstream metricFileStream(metricsPath / std::filesystem::path(metricName));
            for (const auto& value : metricValues) {
                metricFileStream << value << " ";
            }
            double min = *std::min_element(metricValues.begin(), metricValues.end());
            double max = *std::max_element(metricValues.begin(), metricValues.end());
            metricFileStream << "\n" << min << "\n" << max;
        }
        std::ofstream metaFileStream(metricsPath / std::filesystem::path("Meta"));
        metaFileStream << DATA_FILENAME << std::endl;
    }
}

int main(int argc, char *argv[]) {
    #ifndef __EMSCRIPTEN__

    omp_set_num_threads(1);

    build_table();

    scratch = (uint64_t *)malloc(crt_count * poly_len * sizeof(uint64_t));

    ntt_qprime = new intel::hexl::NTT(2048, arb_qprime);

    bool ubench = false;
    bool high_rate = false;

    if (argc > 1) {
        num_expansions = strtol(argv[1], NULL, 10); // max = 7 //used to be 8
        further_dims = strtol(argv[2], NULL, 10);
        total_n = (1 << num_expansions) * (1 << further_dims);
        DATA_FILENAME = argv[3];
        IDX_TARGET = strtol(argv[3], NULL, 10);
        IDX_DIM0 = IDX_TARGET / (1 << further_dims);
        if (argc >= 5) {
            has_file = true;
            dbFilename = argv[4];
            if (dbFilename.length() <= 2) has_file = false;
            for (size_t i = 5; i < argc; i++) {
                if (strcmp(argv[i], "--input") == 0) {
                    load = true;
                }
                if (strcmp(argv[i], "--server") == 0) {
                    server = true;
                }
                if (strcmp(argv[i], "--client") == 0) {
                    client = true;
                }
                if (strcmp(argv[i], "--nonoise") == 0) {
                    cout << "Using no noise" << endl;
                    nonoise = true;
                }
                if (strcmp(argv[i], "--ubench") == 0) {
                    cout << "Microbenchmarking..." << endl;
                    ubench = true;
                }
                if (strcmp(argv[i], "--high-rate") == 0) {
                    cout << "Using high rate variant..." << endl;
                    high_rate = true;
                }
                if (strcmp(argv[i], "--random-data") == 0) {
                    cout << "Using random data..." << endl;
                    random_data = true;
                    dummyWorkingSet = min((1UL << 25) / (total_n), poly_len);
                    max_trials = (1L << 16) / (total_n);
                    if (max_trials == 0) {
                        max_trials = 1;
                    }
                }
                if (strcmp(argv[i], "--show-diff") == 0) {
                    cout << "Showing diff..." << endl;
                    show_diff = true;
                }
                if (strcmp(argv[i], "--output-err") == 0) {
                    cout << "Outputting errors..." << endl;
                    outputErrFilename = argv[++i];
                    output_err = true;
                }
                if (strcmp(argv[i], "--direct-upload") == 0) {
                    cout << "Direct uploading of query (no compression)" << endl;
                    direct_upload = true;
                }
                if (strcmp(argv[i], "--loaddata") == 0) {
                    has_data = true;
                    dataFilename = argv[i+1];
                    i++;
                }
            }
        }
    }
    // if (ubench) {
    //     testPacking();
    //     exit(0);
    // }
    // cout << "params:" << endl;
    // cout << "\tnum_expansions: " << num_expansions << endl;
    // cout << "\tfurther_dims: " << further_dims << endl;
    // cout << "\ttotal_n: " << total_n << endl;
    // cout << "\tIDX_TARGET: " << IDX_TARGET << endl;
    // cout << "\tIDX_DIM0: " << IDX_DIM0 << endl;
    // cout << "\tthreads: " << omp_get_num_threads() << endl;

    if (has_file) {
        if (load) {
            cout << "loading from file" << endl;
            fs.open(dbFilename, fstream::in | fstream::binary);
        } else {
            cout << "outputting to file" << endl;
            fs.open(dbFilename, fstream::out | fstream::binary);
        }
    }

    if (has_data) {
        cout << "loading data from file" << endl;
        fsData.open(dataFilename, fstream::in | fstream::binary);
    }

    do_MatPol_test();
    setup_constants();
    generate_gadgets();

    if (high_rate) {
        testHighRate(num_expansions, further_dims, IDX_TARGET);
        exit(0);
    }
    GlobalTimer::set("Run Separation Test");
    runSeparationTest();
    GlobalTimer::stop("Run Separation Test");
    #endif
}

// Halve the number of ciphertexts using a single query ciphertext for the 'further' dimensions.
/**
 * @brief Halves the number of ciphertexts using a single query ciphertext
 *        for the 'further' dimensions.
 * @param[in] cur_dim The current dimension the folding is taking place.
 * @param[in] num_per The number of elements to fold across.
 * @param[in] query_ct @see process_query_fast() @param further_dims_query_ct
 *       @file spiral.cpp for details.
 * @param[in] query_ct_neg @see process_query_fast() @param further_dims_query_ct_neg
 *       @file spiral.cpp for details.
 * @param[in, out] locals Working data container used to hold ciphertexts
 *       and related variables during query processing.
 * @returns The folded matrix Regev ciphertext stored in @param locals.
 *
 * @details
 * Uses the Regev-GSW external product to homomorphically multiply in the GSW
 * ciphertexts encrypting the subsequent queries.
 *
 * @see pg. 933, Folding in subsequent dimensions.
*/
void foldOneFurtherDimension(
    size_t cur_dim, size_t num_per,
    const uint64_t *query_ct, const uint64_t *query_ct_neg,
    FurtherDimsLocals locals
) {
    uint64_t *C_buf = locals.result;  // Not used.
    uint64_t *g_C = locals.cts;
    uint64_t *g_C_int = locals.scratch_cts1;
    uint64_t *g_C_int2 = locals.scratch_cts2;
    uint64_t *g_C_int_big1 = locals.scratch_cts_double1;
    uint64_t *g_C_int_big2 = locals.scratch_cts_double2;

    split_and_crt(g_C_int_big1, &g_C[num_per * n1 * n2 * poly_len], num_per);
    record ("split");
    reorient_C(g_C_int_big2, g_C_int_big1, num_per);
    record ("reorient");
    cpu_mul_query_by_ct(g_C_int2, &query_ct[cur_dim * (n1 * m2 * crt_count * poly_len)], g_C_int_big2, num_per);
    record ("mul");
    split_and_crt(g_C_int_big1, g_C, num_per);
    record ("split");
    reorient_C(g_C_int_big2, g_C_int_big1, num_per);
    record ("reorient");
    cpu_mul_query_by_ct(g_C_int, &query_ct_neg[cur_dim * (n1 * m2 * crt_count * poly_len)], g_C_int_big2, num_per);
    record ("mul");

    #pragma omp parallel for
    for (size_t i = 0; i < num_per*n1*n2; i++) {
        for (size_t z = 0; z < poly_len; z++) {
            for (size_t n = 0; n < crt_count; n++) {
                size_t idx = i*crt_count*poly_len + n*poly_len + z;
                g_C_int[idx] = barrett_coeff(g_C_int[idx] + g_C_int2[idx], n);
            }
        }
    }

    record ("reduce");

    #pragma omp parallel for
    for (size_t i = 0; i < num_per * n1 * n2; i++) {
        ntt_inverse(&g_C_int[crt_count*poly_len*i]);
    }

    record ("inv");

    // ntt inv then crt lift
    #pragma omp parallel
    {
        size_t num_thr = omp_get_num_threads();
        size_t thr = omp_get_thread_num();

        num_thr = (num_per * n1 * n2) % num_thr == 0 ? num_thr : 3;

        if (thr < num_thr) {
            cpu_crt(
                &g_C[num_per * n1 * n2 * poly_len / num_thr * thr],     // num_per * n1 * n2, with 2 wide coeffs
                &g_C_int[num_per * n1 * n2 * crt_count * poly_len / num_thr * thr], // num_per * n1 * n2 * 2 * 4096
                num_per * n1 * n2 / num_thr);
        }
    }

    record ("crt");
}

/**
 * @brief Holds and performs a collection of extracion algorithms.
 * @param[in] furtherDimsLocals Contains the processed ciphertexts.
 * @param[in] modswitch_on_server Whether modulus switching is done on the server.
 *
 * @details
 * - Rescales the final encoding (part of modulus switching).
 * - Recovers a new encoding Z = floor(q1/p) M + E' using the rescaled encoding
 *   and secret key Sp_mp.
 * - Decodes Z to get the encoded message C.
 * - Ensures C is a valid encoding of a plaintext matrix \hat{M}.
 *
 * @note A portion of this test would normally be done on the client.
*/
double check_final(FurtherDimsLocals furtherDimsLocals, bool modswitch_on_server) {
    MatPoly ct(n1, n2, false);

    MatPoly ct_inp(n1, n2, false);
    MatPoly total_resp(n1, n2, false);

    MatPoly r_end(S_mp.rows, ct.cols, false);

    MatPoly corr = pt_real;
    MatPoly M_result(n0, n0, false);

    if (!modswitch_on_server) {
        for (size_t i = 0; i < n1 * n2 * poly_len; i++) {
            ct.data[i] = furtherDimsLocals.cts[i];
        }
        // IGNORE FOR NOW.
        // MARK: Decode the ciphertexts directly if modulus switching
        //       is done on the client.
        dec_compressed(r_end, to_ntt(ct), to_ntt(S_mp), scale_k);
    } else {
        start_timing();
        MatPoly Sp_mp_nttd_qprime(n0, k_param, false);
        Sp_mp_nttd_qprime = Sp_mp;
        to_ntt_qprime(Sp_mp_nttd_qprime);
        time_key_gen += end_timing();

        for (size_t i = 0; i < n1 * n2 * poly_len; i++) {
            ct_inp.data[i] = furtherDimsLocals.cts[i];
        }
        uint64_t q_1 = 4*p_db;

        // MARK: Rescale the final encoding (PIR response)
        //       to a smaller ring Rq' to obtain compact representation.
        // @note Rescaling the Regev ciphertext is done in two components,
        //       each with their seperate modulus q1, q2
        //       @see pg. 936, D. Modulus Switching.
        MatPoly first_row = pick(ct_inp, 0, 0, 1, ct_inp.cols);
        MatPoly first_row_sw = getRescaled(first_row, Q_i, arb_qprime);
        MatPoly rest_rows = pick(ct_inp, 1, 0, ct_inp.rows - 1, ct_inp.cols);
        MatPoly rest_rows_sw = getRescaled(rest_rows, Q_i, q_1);

        place(total_resp, first_row_sw, 0, 0);
        place(total_resp, rest_rows_sw, 1, 0);

        // ~~transmit~~

        start_timing();
        // MARK: Recover takes in the rescaled encoding and the secret key
        //       and output a new encoding Z = floor(q1/p) M + E'.
        MatPoly first_row_decoded = pick(total_resp, 0, 0, 1, total_resp.cols);
        MatPoly rest_rows_decoded = pick(total_resp, 1, 0, total_resp.rows - 1, total_resp.cols);
        to_ntt_qprime(first_row_decoded);
        MatPoly s_prod = mul_over_qprime(Sp_mp_nttd_qprime, first_row_decoded);
        from_ntt_qprime(s_prod);  // Z.

        // MARK: Decode(S\topC) operation to get the encoded message.
        // @details
        // If E' is sufficently small, then we can recover M from Z.
        // Recovers M by computing p/q\dotS\topC and rounding to the nearest integer.
        //
        // @note C is a redundant encoding of a plaintext matrix \hat{M}.
        // @see pg 934, Col 2, Paragraph 3.
        for (size_t i = 0; i < s_prod.rows * s_prod.cols * poly_len; i++) {
            int64_t val_first = s_prod.data[i];
            if (val_first >= arb_qprime/2) val_first -= arb_qprime;
            int64_t val_rest = rest_rows_decoded.data[i];
            if (val_rest >= q_1/2) val_rest -= q_1;

            uint64_t denom = arb_qprime * (q_1/p_db);

            int64_t r = val_first * q_1;
            r +=  val_rest * arb_qprime;
            // divide r by arb_qprime, rounding
            int64_t sign = r >= 0 ? 1 : -1;
            __int128_t val = r;
            __int128_t result = (r + sign*((int64_t)denom/2)) / (__int128_t)denom;
            result = (result + (denom/p_db)*p_db + 2*p_db) % p_db;

            s_prod.data[i] = (uint64_t)result;
        }
        M_result = s_prod;  // M.
        time_decoding += end_timing();
    }

    // MatPoly S_mp_nttd_sprime(n0, n1, false);
    // S_mp_nttd_sprime = S_mp;
    // to_ntt_qprime(S_mp_nttd_sprime);
    // MatPoly ct_nttd_qprime(n1, n2, false);
    // ct_nttd_qprime = ct;
    // start_timing();
    // to_ntt_qprime(ct_nttd_qprime);

    // MatPoly Z_uncrtd = mul_over_qprime(S_mp_nttd_sprime, ct_nttd_qprime);
    // from_ntt_qprime(Z_uncrtd);
    // divide_by_const(r_end, Z_uncrtd, p_db, arb_qprime, p_db);
    // time_decoding += end_timing();

    // cop(M_result, r_end);//, 0, 1);

    // MARK: Check correctness. If E' is too large, then the message would
    //       be irretrievable.
    cout << "Is correct?: " << is_eq(corr, M_result) << endl;
    if (show_diff) {
        for (size_t i = 0; i < M_result.rows * M_result.cols * coeff_count; i++) {
            if (corr.data[i] != M_result.data[i]) {
                cout << i << " " << corr.data[i] << ", " << M_result.data[i] << endl;
                // exit(1);
            }
        }
    }

    // MatPoly Z(S_mp.rows, ct.cols);
    // dec(Z, to_ntt(S_mp), to_ntt(ct));

    // Z_uncrtd = mul_over_integers(S_mp, ct, arb_qprime);

    // MatPoly scaled_pt(n0, n2, false);
    // for (size_t i = 0; i < n0 * n2 * coeff_count; i++) {
    //     scaled_pt.data[i] = (pt_real.data[i] * (__uint128_t)arb_qprime) / p_db;
    // }

    // double log_var = get_log_var(Z_uncrtd, scaled_pt, show_diff, arb_qprime);
    // cout << "variance of diff: 2^" << log_var << endl;

    // MARK: On error, output the diffs into a file for debugging.
    if (output_err) {
        for (size_t i = 0; i < n1 * n2 * poly_len; i++) {
            ct.data[i] = furtherDimsLocals.cts[i];
        }
        MatPoly Z = multiply(S_mp, ct);
        MatPoly Z_uncrtd = from_ntt(Z);
        divide_by_const(Z_uncrtd, Z_uncrtd, p_db, Q_i, p_db);
        double log_var = get_log_var(Z_uncrtd, corr, show_diff, p_db);
        cout << "variance of diff w/o modswitching: 2^" << log_var << endl;

        MatPoly scaled_pt = mul_by_const(to_ntt(single_poly(scale_k)), pt_encd_correct);
        vector<int64_t> diffs = get_diffs(from_ntt(Z), from_ntt(scaled_pt), Q_i);
        cout << "diffs: ";
        fstream output;
        output.open(outputErrFilename, fstream::out | fstream::app | fstream::ate);
        output_diffs(output, diffs);
        output.close();
        cout << endl;
    }

    return 0;
}

/**
 * @brief Handles server key setup.
 * @param g_Ws_fft FFT compliant of the key switch matrix W.
 *      @see pg. 935 ScalToMatSetup
 * @param encodeCompressedSetupData  Whether to encode the setup data.
 *      @warning not implemented here.
 * @param[in] setNewSecret Whether to create a new secret key.
 * @param num_expansions_h Number of expansions to use.
 * @param G The gadget matrix.
*/
void generate_setup(
    uint64_t **g_Ws_fft,          // use this setup data
    bool encodeCompressedSetupData,
    bool setNewSecret = true,
    int num_expansions_h = num_expansions,
    const MatPoly &G = G_hat
) {
    if (setNewSecret) {
        S_mp = MatPoly(n0, n1, false);
        Sp_mp = MatPoly(n0, k_param, false);
        sr_mp = MatPoly(1, 1, false);

        start_timing();
        // MARK: Create keys.
        keygen(S_mp, Sp_mp, sr_mp);
        time_key_gen += end_timing();
    }

    if (direct_upload) return;
}

/**
 * @brief Allocates heap space for client query.
 * @param idx index to search.  @warning Not used here.
 * @param[out] g_C_fft_crtd
 *      @warning Possible bug. Ignored and overwritten during query processing.
 *               Otherwise, this was meant to be the ciphertext that got expanded.
 * @param[out] g_Q_crtd Single scalar Regev ciphertext representing the client's query.
 *      @see pg.932 Query Generation.
*/
void generate_query(
    size_t idx,
    uint64_t **g_C_fft_crtd,      // expand this ct
    uint64_t **g_Q_crtd           // further dims query
) {
    *g_C_fft_crtd = (uint64_t *)malloc((n1 * m1 * coeff_count) * sizeof(uint64_t));
    *g_Q_crtd = (uint64_t *)malloc(further_dims * (n1 * m2 * coeff_count) * sizeof(uint64_t));
}

/**
 * @brief Handler for server setup for query testing.
 * @param idx index to search.
 *      @see generate_query()
 * @param[out] g_C_fft_crtd
 *      @see generate_query()
 * @param[out] g_Q_crtd
 *      @see generate_query()
 * @param g_Ws_fft
 *      @see generate_setup()
*/
void generate_setup_and_query(
    size_t idx,
    uint64_t **g_C_fft_crtd,      // expand this ct
    uint64_t **g_Q_crtd,          // further dims query
    uint64_t **g_Ws_fft,          // use this setup data
    bool encodeCompressedSetupData
) {
    generate_setup(g_Ws_fft, encodeCompressedSetupData);
    generate_query(
        idx,
        g_C_fft_crtd,      // expand this ct
        g_Q_crtd           // further dims query
    );
}

/**
 * @brief Process the query ciphertext against the database and perform
 *        folding to obtain a single Regev ciphertext.
 *
 * @param expansion_query_ct
 *        @warning Possible bug. Not used.
 *        @see @param expansion_locals.reoriented_ciphertexts for actual
 *          query ciphertexts.
 * @param setup_data Not used.
 * @param[in] further_dims_query_ct Further dimensions query ciphertext.
 *      Generated during the GSW expansion phase during @see runConversionImproved()
 *      @see pg. 933 Folding in subsequent dimensions.
 * @param[in] further_dims_query_ct_neg A compressed, NTT optimised and negated version
 *       of the further dimensions query ciphertext.
 * @param[in, out] expansion_locals Contains the expanded query ciphertexts
 *       and related data.
 * @param[in, out] further_dims_locals Allocated space for the 'processing' phase
 *       of the query. Should be empty. @see doTest() for allocation.
*/
void process_query_fast(
    const uint64_t *expansion_query_ct,         // expand this ct
    const uint64_t *setup_data,
    const uint64_t *further_dims_query_ct,      // further dims query
    const uint64_t *further_dims_query_ct_neg,
    ExpansionLocals expansion_locals,           // must be cleared
    FurtherDimsLocals further_dims_locals       // must be cleared
) {
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    // WARN: Missing end_timing() call?
    start_timing();
    reorientCiphertexts(
        expansion_locals.reoriented_ciphertexts,
        expansion_locals.cts,
        dim0,
        expansion_locals.n1_padded
    );
    time_expansion_main += end_timing();
    if (direct_upload) time_expansion_main = 0;

    // MARK: Process the first dimension.
    start_timing();
    multiplyQueryByDatabase(
        further_dims_locals.scratch_cts1,
        expansion_locals.reoriented_ciphertexts,
        B,
        dim0,
        num_per
    );
    // NOTE: Remove NTT and CRT formatting.
    nttInvAndCrtLiftCiphertexts(
        num_per,
        further_dims_locals
    );
    time_first_multiply = end_timing();

    size_t cur_dim = 0;
    start_timing();
    // MARK: Folding in subsequent dimensions.
    while (num_per >= 2) {
        num_per = num_per / 2;
        foldOneFurtherDimension(
            cur_dim, num_per,
            further_dims_query_ct,
            further_dims_query_ct_neg, further_dims_locals
        );
        cur_dim++;
    }
    time_folding = end_timing();
    NativeLog::cout << "done folding" << endl;
}

void answer_process_query_fast(
        const uint64_t *further_dims_query_ct,      // further dims query
        const uint64_t *further_dims_query_ct_neg,
        ExpansionLocals expansion_locals,           // must be cleared
        FurtherDimsLocals further_dims_locals       // must be cleared
) {
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    // WARN: Missing end_timing() call?
    start_timing();
    reorientCiphertexts(
            expansion_locals.reoriented_ciphertexts,
            expansion_locals.cts,
            dim0,
            expansion_locals.n1_padded
    );
    time_expansion_main += end_timing();
    if (direct_upload) time_expansion_main = 0;

    // MARK: Process the first dimension.
    start_timing();
    GlobalTimer::set("Process first dimension via query-database multiplication");
    Evaluation::set("Answer.server");
    multiplyQueryByDatabase(
            further_dims_locals.scratch_cts1,
            expansion_locals.reoriented_ciphertexts,
            B,
            dim0,
            num_per
    );
    GlobalTimer::stop("Process first dimension via query-database multiplication");
    // NOTE: Remove NTT and CRT formatting.
    nttInvAndCrtLiftCiphertexts(
            num_per,
            further_dims_locals
    );
    time_first_multiply = end_timing();

    size_t cur_dim = 0;
    start_timing();
    // MARK: Folding in subsequent dimensions.
    GlobalTimer::set("Folding in subsequent dimensions");
    while (num_per >= 2) {
        num_per = num_per / 2;
        foldOneFurtherDimension(
                cur_dim, num_per,
                further_dims_query_ct,
                further_dims_query_ct_neg, further_dims_locals
        );
        cur_dim++;
    }
    Evaluation::stop("Answer.server");
    GlobalTimer::stop("Folding in subsequent dimensions");
    time_folding = end_timing();
    NativeLog::cout << "done folding" << endl;
}


void printScalarMat(MatPoly &A) {
    for (size_t i = 0; i < A.rows; i++) {
        for (size_t j = 0; j < A.cols; j++) {
            cout << (uint64_t)A[i][j * poly_len] << " ";
        }
        cout << endl;
    }
    cout << endl;
}

void buildSpacedIdentity(size_t offs, MatPoly &G) {
    assert(!G.isNTT);
    size_t nx = G.rows;
    size_t m = G.cols;
    size_t chunk = nx / n0;
    assert(chunk * n1 == m);
    assert(offs < chunk);

    size_t col = 0;
    for (size_t i = 0; i < nx; i++) {
        G[i][col * poly_len] = 1;

        // insert a column of zeros
        if (i % n0 == offs) {
            col++;
        }

        col++;
    }

    // printScalarMat(G);
}

/**
 * @brief Performs the initial expansion on the query ciphertext using the
 *        coefficent expansion algorithm.
 *
 * @param cv_v The query ciphertext.
 * @param W_left_v The left half of automorphism keys (W0, ...).
 * @param W_right_v The right half of automorphism keys (..., Wp-1).
 * @return Time required to expand the query.
 *
 * @see Algorithm 1, Appendix C, C.1, Coefficient expansion algorithm.
*/
double expandImproved(
    vector<MatPoly> &cv_v, // first element is n0 x 1
    size_t g,
    size_t m_exp,
    const vector<MatPoly> &W_left_v,   // g matrices, each n0 x m_exp
    const vector<MatPoly> &W_right_v,   // g matrices, each n0 x m_exp
    size_t max_bits_to_gen_right = 0,
    size_t stopround = 0
) {
    assert(cv_v[0].rows == n0);
    assert(cv_v[0].cols == 1);

    MatPoly c(n0, 1, false);
    MatPoly c_automorphed(n0, 1, false);
    MatPoly c_automorphed_0(1, 1, false);
    MatPoly c_automorphed_1(1, 1, false);
    MatPoly c_automorphed_1_nttd(1, 1);
    MatPoly c_automorphed_1_padded(n0, 1);
    MatPoly ginv_c(m_exp, 1, false);
    MatPoly ginv_c_nttd(m_exp, 1);
    MatPoly ginv_c_right(m_exp_right, 1, false);
    MatPoly ginv_c_right_nttd(m_exp_right, 1);

    MatPoly W_times_ginv_c(n0, 1);

    start_timing();
    for (size_t r = 0; r < g; r++) {
        size_t num_in = 1 << r;
        size_t num_out = 2 * num_in;

        size_t t = (poly_len / (1 << r)) + 1;

        const MatPoly &W_left = W_left_v[r];
        const MatPoly &W_right = W_right_v[r];
        const MatPoly &neg1 = neg1s_mp[r];

        for (size_t i = 0; i < num_out; i++) {
            if (stopround > 0 && r > stopround && (i % 2) == 1) continue;
            if (stopround > 0 && r == stopround && (i % 2) == 1 && i/2 > max_bits_to_gen_right) continue;

            const MatPoly &W        = (i % 2) == 0 ? W_left : W_right;
            MatPoly &gi_c           = (i % 2) == 0 ? ginv_c : ginv_c_right;
            MatPoly &gi_c_nttd      = (i % 2) == 0 ? ginv_c_nttd : ginv_c_right_nttd;
            size_t gadget_dim       = (i % 2) == 0 ? m_exp : m_exp_right;

            if (i < num_in) mul_by_const(cv_v[num_in + i], neg1, cv_v[i]);
            // start_timing();
            from_ntt(c, cv_v[i]);
            // record("ntti");
            automorph(c_automorphed, c, t);
            pick(c_automorphed_0, c_automorphed, 0, 0);
            pick(c_automorphed_1, c_automorphed, 1, 0);
            to_ntt(c_automorphed_1_nttd, c_automorphed_1);
            place(c_automorphed_1_padded, c_automorphed_1_nttd, 1, 0);
            // record("auto");
            gadget_invert(gadget_dim, gi_c, c_automorphed_0, 1);
            // record("ginv");
            // to_ntt(gi_c_nttd, gi_c);
            to_ntt_no_reduce(gi_c_nttd, gi_c);
            // record("ntt");
            multiply(W_times_ginv_c, W, gi_c_nttd);
            // record("mul");
            // add(cv_v[i], cv_v[i], W_times_ginv_c);
            // add(cv_v[i], cv_v[i], c_automorphed_1_padded);
            size_t idx = 0;
            for (size_t j = 0; j < n0; j++) {
                for (size_t n = 0; n < crt_count; n++) {
                    for (size_t z = 0; z < coeff_count; z++) {
                        cv_v[i].data[idx] = barrett_coeff(cv_v[i].data[idx] + W_times_ginv_c.data[idx] + j*c_automorphed_1_nttd.data[n * coeff_count + z], n);
                        // cv_v[i].data[idx] = barrett_coeff(cv_v[i].data[idx] + W_times_ginv_c.data[idx] + c_automorphed_1_padded.data[idx], n);
                        idx++;
                    }
                }
            }
            // record("add");
            // end_timing();
        }
    }
    return end_timing();
}

void convertRegevToGswImproved(
    size_t m,
    size_t m_conv,
    MatPoly &out_gsw,     // n1 x m
    const vector<MatPoly> &cv_v, // l  matrices, each n0 x 1
    size_t offs_cv_v,
    MatPoly &c_ntti,        // n0 x 1
    MatPoly &ginv_c,        // m_conv x 1
    MatPoly &ginv_c_ntt,    // m_conv x 1, ntt
    MatPoly &W_switched,    // n0 x 1, ntt
    MatPoly &Y_switched,    // n0 x 1, ntt
    MatPoly &C_v_0,         // n0 x m, ntt
    MatPoly &C_v_1,         // n0 x m, ntt
    MatPoly &C_v_0_ntti,    // n0 x m
    MatPoly &C_v_1_ntti,    // n0 x m
    MatPoly &ginv_Ct,       // m_conv x m
    MatPoly &ginv_Ct_ntt,   // m_conv x m, ntt
    MatPoly &prod0,         // n1 x m, ntt
    MatPoly &prod1,         // n1 x m, ntt
    const vector<MatPoly> &X_v,  // n0 matrices, each n1 x m_conv
    const vector<MatPoly> &Y_v,  // n0 matrices, each n0 x m_conv
    const MatPoly &W             // n0 x m_conv
) {
    assert(m % n1 == 0);
    size_t ell = m / n1;

    // i = 0
    for (size_t j = 0; j < ell; j++) {
        size_t offs = n1 * j;
        const MatPoly& c = cv_v[offs_cv_v + j];
        place(C_v_0, c,             0, offs + 1);

        from_ntt(c_ntti, c);
        gadget_invert(m_conv, ginv_c, c_ntti, n0);
        to_ntt(ginv_c_ntt, ginv_c);

        multiply(W_switched, W, ginv_c_ntt);
        place(C_v_1, W_switched,    0, offs + 1 + 1);

        multiply(Y_switched, Y_v[0], ginv_c_ntt);
        place(C_v_0, Y_switched,    0, offs);

        multiply(Y_switched, Y_v[1], ginv_c_ntt);
        place(C_v_1, Y_switched,    0, offs);
    }

    from_ntt(C_v_0_ntti, C_v_0);
    from_ntt(C_v_1_ntti, C_v_1);

    gadget_invert(m_conv, ginv_Ct, C_v_0_ntti, n0);
    to_ntt(ginv_Ct_ntt, ginv_Ct);
    multiply(prod0, X_v[0], ginv_Ct_ntt);

    gadget_invert(m_conv, ginv_Ct, C_v_1_ntti, n0);
    to_ntt(ginv_Ct_ntt, ginv_Ct);
    multiply(prod1, X_v[1], ginv_Ct_ntt);

    add(out_gsw, prod0, prod1);
}

void composeRegevToLargerN(
    size_t m_conv,
    MatPoly &out_reg,               // n1 x n0
    const MatPoly &cv,              // n0 x 1
    MatPoly &cv_ntti,               // n0 x 1, ntti
    MatPoly &ginv_c,                // m_conv x 1, ntti
    MatPoly &ginv_c_nttd,           // m_conv x 1, ntt
    MatPoly &prod_X_ginv,           // n1 x 1, ntt
    MatPoly &prod_W_ginv,           // n0 x 1, ntt
    const vector<MatPoly> &X_v,     // n0 matrices, each n1 x m_conv
    const MatPoly &W                // n0 x m_conv
) {
    assert(cv.rows == n0);
    assert(cv.cols == 1);

    from_ntt(cv_ntti, cv);
    gadget_invert(m_conv, ginv_c, cv_ntti, n0);
    to_ntt(ginv_c_nttd, ginv_c);
    multiply(prod_X_ginv, X_v[0], ginv_c_nttd);
    place(out_reg, prod_X_ginv, 0, 0);

    // multiply(prod_W_ginv, W, ginv_c_nttd);
    // from_ntt(cv_ntti, prod_W_ginv);
    // gadget_invert(m_conv, ginv_c, cv_ntti, n0);
    // to_ntt(ginv_c_nttd, ginv_c);
    multiply(prod_X_ginv, X_v[1], ginv_c_nttd);
    place(out_reg, prod_X_ginv, 0, 1);
}

static void special_distribute(MatPoly &out, const MatPoly &a) {
    // a: m_conv x 1 nttd
    // out: 2*m_conv x 2, nttd
    for (size_t i = 0; i < m_conv; i++) {
        for (size_t nz = 0; nz < crt_count * coeff_count; nz++) {
            uint64_t val = a.data[i * (crt_count * coeff_count) + nz];
            size_t r = i * 2;
            size_t c = 0;
            out.data[(r * 2 + c) * crt_count * coeff_count + nz] = val;
            r = i*2 + 1;
            c = 1;
            out.data[(r * 2 + c) * crt_count * coeff_count + nz] = val;
        }
    }
}

/**
 * @brief Takes a Regev ciphertext encrypting a bit x \in {0, 1} and outputs
 *        a matrix Regev ciphertext that encrpyts the matrix xIn, where In is
 *        the n x n identity matrix.
 * @param[out] out_reg The output Regev ciphertext.
 * @param[in] cv The input Regev ciphertext encrypting a bit.
 * @param[in] W The key-switching matrix.
 * @param[in] cv_0 Scratch matrix.
 * @param[in] cv_1 Scratch matrix.
 * @param[in] cv_ntti Scratch matrix.
 * @param[in] square_cv Scratch matrix.
 * @param[in] ginv_c Scratch matrix.
 * @param[in] ginv_c_nttd Scratch matrix.
 * @param[in] prod_W_ginv Scratch matrix.
 * @param[in] padded_cv_1 Scratch matrix.
 * @param[in] ginv_c_raw Scratch matrix.
 * @param[in] ginv_c_raw_nttd Scratch matrix.
 *
 * @see Section 2.A, Appendix B.
*/
void scalToMat(
    size_t m_conv,
    MatPoly &out_reg,               // n1 x n0
    const MatPoly &cv,              // n0 x 1
    const MatPoly &W,               // n1 x (n0 * m_conv)
    MatPoly &cv_0,
    MatPoly &cv_1,
    MatPoly &cv_ntti,
    MatPoly &square_cv,
    MatPoly &ginv_c,
    MatPoly &ginv_c_nttd,
    MatPoly &prod_W_ginv,
    MatPoly &padded_cv_1,
    MatPoly &ginv_c_raw,
    MatPoly &ginv_c_raw_nttd
) {
    // assert(out_reg.isNTT);
    // assert(cv.isNTT);
    // assert(W.isNTT);
    // assert(cv.rows == n0);
    // assert(cv.cols == 1);
    // assert(W.rows == n1);
    // assert(W.cols == n0 * m_conv);

    pick(cv_0, cv, 0, 0);
    pick(cv_1, cv, 1, 0);
    from_ntt(cv_ntti, cv_0);
    gadget_invert(m_conv, ginv_c_raw, cv_ntti, 1);
    to_ntt_no_reduce(ginv_c_raw_nttd, ginv_c_raw);
    special_distribute(ginv_c_nttd, ginv_c_raw_nttd);
    multiply(prod_W_ginv, W, ginv_c_nttd);
    place(padded_cv_1, cv_1, 1, 0);
    place(padded_cv_1, cv_1, 2, 1);

    add(out_reg, prod_W_ginv, padded_cv_1);
}

void scalToMatFast(
    size_t m_conv,
    MatPoly &out_reg,               // n1 x n0
    const MatPoly &cv,              // n0 x 1
    const MatPoly &W,               // n1 x (n0 * m_conv)
    MatPoly &cv_0,
    MatPoly &cv_1,
    MatPoly &ginv_c,
    MatPoly &ginv_c_nttd,
    MatPoly &prod_W_ginv,
    MatPoly &padded_cv_1,
    const MatPoly &ginv_c_raw_nttd
) {
    // assert(out_reg.isNTT);
    // assert(cv.isNTT);
    // assert(W.isNTT);
    // assert(cv.rows == n0);
    // assert(cv.cols == 1);
    // assert(W.rows == n1);
    // assert(W.cols == n0 * m_conv);

    pick(cv_0, cv, 0, 0);
    pick(cv_1, cv, 1, 0);
    special_distribute(ginv_c_nttd, ginv_c_raw_nttd);
    multiply(prod_W_ginv, W, ginv_c_nttd);
    place(padded_cv_1, cv_1, 1, 0);
    place(padded_cv_1, cv_1, 2, 1);

    add(out_reg, prod_W_ginv, padded_cv_1);
}

void scalToMat(
    size_t m_conv,
    MatPoly &out_reg,               // n1 x n0
    const MatPoly &cv,              // n0 x 1
    const MatPoly &W                // n1 x (n0 * m_conv)
) {
    MatPoly cv_0(1, 1);
    MatPoly cv_1(1, 1);
    MatPoly cv_ntti(1, 1, false);
    MatPoly square_cv(n0, n0, false);
    MatPoly ginv_c(n0 * m_conv, n0, false);
    MatPoly ginv_c_nttd(n0 * m_conv, n0);
    MatPoly prod_W_ginv(n1, n0);
    MatPoly padded_cv_1(n1, n0);
    MatPoly ginv_c_raw(m_conv, 1, false);
    MatPoly ginv_c_raw_nttd(m_conv, 1);

    scalToMat(
        m_conv,
        out_reg,
        cv,
        W,
        cv_0,
        cv_1,
        cv_ntti,
        square_cv,
        ginv_c,
        ginv_c_nttd,
        prod_W_ginv,
        padded_cv_1,
        ginv_c_raw,
        ginv_c_raw_nttd
    );
}

void scalToMatFast(
    size_t m_conv,
    MatPoly &out_reg,               // n1 x n0
    const MatPoly &cv,              // n0 x 1
    const MatPoly &ginv_c_raw_nttd, // m_conv x 1
    const MatPoly &W                // n1 x (n0 * m_conv)
) {
    MatPoly cv_0(1, 1);
    MatPoly cv_1(1, 1);
    MatPoly cv_ntti(1, 1, false);
    MatPoly square_cv(n0, n0, false);
    MatPoly ginv_c(n0 * m_conv, n0, false);
    MatPoly ginv_c_nttd(n0 * m_conv, n0);
    MatPoly prod_W_ginv(n1, n0);
    MatPoly padded_cv_1(n1, n0);
    MatPoly ginv_c_raw(m_conv, 1, false);

    scalToMatFast(
        m_conv,
        out_reg,
        cv,
        W,
        cv_0,
        cv_1,
        ginv_c,
        ginv_c_nttd,
        prod_W_ginv,
        padded_cv_1,
        ginv_c_raw_nttd
    );
}

/**
 * @brief Takes the decoposition dimension t_GSW Regev Encodings
 *        and outputs a single GSW encoding.
 * @param[in] m_conv The number of columns in the matrix.
 * @param[in] t The number of iterations.
 * @param[out] out The output GSW encoding.
 * @param[in] cv_v The input Regev encodings.
 * @param[in] cv_v_offset The offset into @param cv_v.
 * @param[in] W The key-switching matrix.
 * @param[in] V @see pg. 936, 2nd point.
 *
 * @details
 * t_GSW encodings consists of 2 \dot t_GSW elements of Rq.
 * Output GSW encoding is of the form:
 *        (n+1)m_GSW = (n+1)^2t_GSW
 *
 * @see pg. 936, Remark 3.1.
*/
void regevToGSW(
    size_t m_conv, size_t t, MatPoly &out,
    const vector<MatPoly> &cv_v, size_t cv_v_offset, const MatPoly &W, const MatPoly &V
) {
    MatPoly cv_ntti(2, 1, false);
    MatPoly cv0_ntti(1, 1, false);
    MatPoly cv1_ntti(1, 1, false);
    MatPoly cv(m_conv, 1);
    MatPoly scalToMatResult(n1, n0);
    MatPoly ginv_tmp(m_conv, 1, false);
    MatPoly ginv_Chat_nttd(2 * m_conv, t);
    MatPoly prod(n1, t);
    MatPoly result(n1, n1 * t);
    MatPoly result_permuted(n1, n1 * t);
    for (size_t i = 0; i < t; i++) {
        from_ntt(cv_ntti, cv_v[cv_v_offset + i]);
        pick(cv0_ntti, cv_ntti, 0, 0);
        pick(cv1_ntti, cv_ntti, 1, 0);
        gadget_invert(m_conv, ginv_tmp, cv0_ntti, 1);
        to_ntt_no_reduce(cv, ginv_tmp);
        place(ginv_Chat_nttd, cv, 0, i);

        // MARK: Compute C_i <- ScalToMat(W, c_i) for each i \in [t_GSW]
        scalToMatFast(m_conv, scalToMatResult, cv_v[cv_v_offset + i], cv, W);
        place(result, scalToMatResult, 0, t + (n0 * i));

        gadget_invert(m_conv, ginv_tmp, cv1_ntti, 1);
        to_ntt_no_reduce(cv, ginv_tmp);
        place(ginv_Chat_nttd, cv, m_conv, i);
        // scalToMat(m_conv, scalToMatResult, cv_v[cv_v_offset + i], W);
    }
    multiply(prod, V, ginv_Chat_nttd);
    place(result, prod, 0, 0);

    // MARK: Apply the permutation matrix?
    // NOTE: Need to double-check.
    //       The permutation matrix intends to rearrange the rows of
    //       the left-hand side matrix so that it becomes equal to G_n+1,z_GSW
    //       on the right-hand side.
    // @see pg. 936, 2nd dot point.
    // permute
    for (size_t i = 0; i < t; i++) {
        cop(result_permuted, result, 0, i, 0, (n0+1)*i, n1, 1);
        cop(result_permuted, result, 0, t + n0*i, 0, (n0+1)*i + 1, n1, n0);
    }

    out = result_permuted;
}

vector<MatPoly> reorderFromStopround(const vector<MatPoly> &round_cv_v, size_t even_coeffs, size_t odd_coeffs) {
    vector<MatPoly> copy_v;
    for (size_t i = 0; i < even_coeffs; i++) {
        copy_v.push_back(round_cv_v[2*i]);
    }
    for (size_t i = 0; i < odd_coeffs; i++) {
        copy_v.push_back(round_cv_v[2*i + 1]);
    }
    return copy_v;
}

/**
 * @brief Populates, generates and expands the query.
 * @param[in, out] expansionLocals @see process_crtd_query().
 * @param[in] g_Q_nttd The converted (NTT-compliant) query GSW ciphertexts.
 * @param[in, out] g_Q_crtd The converted (CRT-compliant) query ciphertext.
 *       @note This should be empty at the start, and will be populated with
 *       the expanded query ciphertext at the end of this function.
 *
 * @details
 * Completes the following:
 * - Generates automorphism keys.
 * - Creates a template query.
 * - Performs first dimension encoding.
 * - Performs subsequent dimension encoding.
 * - Encrypts the query.
 * - Performs initial query expansion.
 * - Performs first dimension expansion.
 * - Performs GSW ciphertext expansion.
 *
 * @note Original comments:
 *  goal: fill expansionLocals.cts with the 'composed' Regev ciphertexts,
 *  and g_Q(nttd/crtd) with the converted GSW ciphertexts.
*/
void runConversionImproved(
    ExpansionLocals expansionLocals,
    uint64_t *g_Q_nttd,
    uint64_t *g_Q_crtd
) {
    size_t num_expanded = 1 << num_expansions;
    size_t idx_dim0     = IDX_TARGET / (1 << further_dims);
    size_t idx_further  = IDX_TARGET % (1 << further_dims);

    size_t m = m2;
    size_t ell = m / n1;
    size_t num_bits_to_gen = ell * further_dims + num_expanded;

    size_t qe_first = query_elems_first;
    size_t qe_rest = query_elems_rest;
    size_t bits_per = get_bits_per(ell);

    double total_time = 0;
    size_t num_expansion_rounds = query_elems_rest > 0 ? 2 : 1;

    bool directly_upload_first_cts = qe_first >= (1 << num_expansions);
    bool directly_upload_later_cts = qe_rest >= further_dims * ell;
    if(directly_upload_later_cts) cout << "directly uploading Regev -> GSW ciphertexts" << endl;
    vector<MatPoly> cv_v;

    for (size_t round = 0; round < num_expansion_rounds; round++) {
        if (round == 0 && directly_upload_first_cts) continue;
        if (round == 1 && directly_upload_later_cts) continue;
        size_t num_subrounds;
        if (qe_rest > 0) {
            if (round == 0) {
                num_subrounds = qe_first;
                num_bits_to_gen = num_expanded / num_subrounds;
            } else {
                num_subrounds = qe_rest;
                num_bits_to_gen = (ell * further_dims) / num_subrounds;
            }
        } else {
            num_subrounds = 1;
            num_bits_to_gen = ell * further_dims + num_expanded;
        }
        size_t g = (size_t) ceil(log2((double)( num_bits_to_gen )));
        cout << "g = " << g << endl;

        size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
        if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions
        NativeLog::cout << "stopround = " << stopround << endl;

        vector<MatPoly> dummy_X_v, dummy_Y_v, dummy_W_v, W_exp_right_v;
        // WARN: X_v, Y_v, W_v is not used.
        vector<MatPoly> X_v, Y_v, W_v, W_exp_v;

        start_timing();
        // MARK: Generate automorphism keys (W0, ..., Wp-1) as W_exp_right_v and W_exp_v.
        getPublicEncryptions(
            g, dummy_X_v, dummy_Y_v, dummy_W_v, W_exp_right_v,
            m_conv, m_exp_right, false, true,
            stopround > 0 ? stopround+1 : 0
        );
        getPublicEncryptions(
            g, dummy_X_v, dummy_Y_v, dummy_W_v, W_exp_v,
            m_conv, m_exp, false, true
        );
        time_key_gen += end_timing();
        add_pub_param(W_exp_right_v);
        add_pub_param(W_exp_v);

        uint64_t scal_const = 1;//inv_mod(1 << g, Q_i_u128);

        for (size_t subround = 0; subround < num_subrounds; subround++) {
            start_timing();
            MatPoly sigma(1, 1, false);
            uint64_t init_val = scale_k;//ab_i;//factor_init;

            if (stopround != 0) {
                // encode first dimension bits in even coeffs
                // encode rest of the scalars in odd coeffs

                // MARK: Generate a template query as sigma. Sigma gets processed
                //       and converted into @param g_Q_crtd at the end of this function.
                //       sigma > cv > round_cv_v > cv_v > Cp > Cp_raw > g_Q_crtd
                sigma.data[2*idx_dim0] = (scal_const * (__uint128_t)init_val) % Q_i;
                for (size_t i = 0; i < further_dims; i++) {
                    uint64_t bit = (idx_further & (1 << i)) >> i;
                    for (size_t j = 0; j < ell; j++) {
                        size_t idx = i * ell + j;
                        uint64_t val = (1UL << (bits_per * j)) * bit;
                        sigma.data[2*idx+1] = (scal_const * (__uint128_t)val) % Q_i;
                    }
                }
            } else {
                // MARK: First dimensions encoding.
                if (round == 0) {
                    bool in_subround = (idx_dim0 / (1 << g)) == subround;
                    size_t idx_for_subround = idx_dim0 % (1 << g);
                    if (in_subround) {
                        sigma.data[idx_for_subround] = (scal_const * (__uint128_t)init_val) % Q_i;
                    }
                }
                // MARK: Encoding subsequent dimensions.
                if (qe_rest == 0 || round == 1) {
                    size_t offset = qe_rest == 0 ? num_expanded : 0;
                    size_t start_of_encoding = num_bits_to_gen * subround;
                    size_t end_of_encoding = start_of_encoding + num_bits_to_gen;
                    size_t ctr = 0;
                    for (size_t i = 0; i < further_dims; i++) {
                        uint64_t bit = (idx_further & (1 << i)) >> i;
                        for (size_t j = 0; j < ell; j++) {
                            size_t idx = i * ell + j;
                            uint64_t val = (1UL << (bits_per * j)) * bit;
                            if ((idx >= start_of_encoding) && (idx < end_of_encoding)) {
                                sigma.data[offset + ctr] = (scal_const * (__uint128_t)val) % Q_i;
                                ctr++;
                            }
                        }
                    }
                }
            }
            // Question: Spiral and SpiralStream uses the same testing function.
            //           Do we pack over here?
            if (stopround != 0) {
                uint64_t inv_2_g_first = inv_mod(1 << g, Q_i);
                uint64_t inv_2_g_rest = inv_mod(1 << (stopround+1), Q_i);
                for (size_t i = 0; i < coeff_count/2; i++) {
                    sigma.data[2*i] = (sigma.data[2*i] * (__uint128_t)inv_2_g_first) % Q_i;
                    sigma.data[2*i+1] = (sigma.data[2*i+1] * (__uint128_t)inv_2_g_rest) % Q_i;
                }
            } else {
                uint64_t inv_2_g = inv_mod(1 << g, Q_i);
                for (size_t i = 0; i < coeff_count; i++) {
                    sigma.data[i] = (sigma.data[i] * (__uint128_t)inv_2_g) % Q_i;
                }
            }
            // MARK: Query encryption. Encode polynomial into Regev for homomorphic operations.
            //      pg. 936 Section 3.C
            MatPoly cv = encryptSimpleRegev(sigma);

            vector<MatPoly> round_cv_v;
            round_cv_v.push_back(cv);
            for (size_t i = 0; i < (1<<g) - 1; i++) {
                round_cv_v.emplace_back(n0, 1);
            }
            time_query_gen += end_timing();

            // MARK: Initial Expansion.
            double quick_expansion_time = expandImproved(
                round_cv_v, g, m_exp,
                W_exp_v, W_exp_right_v,
                ell * further_dims, stopround
            );
            total_time += quick_expansion_time;

            // reorder ciphertexts if using stopround
            if (stopround != 0) {
                start_timing();
                round_cv_v = reorderFromStopround(round_cv_v, num_expanded, ell * further_dims);
                total_time += end_timing();
            }

            cv_v.insert(cv_v.end(), round_cv_v.begin(), round_cv_v.begin() + num_bits_to_gen);
        }
    }
    cout << "Expansion took (CPU·us): " << total_time << endl;
    time_expansion_main += total_time;

    if (directly_upload_first_cts) {
        cout << "directly uploading Regev ciphertexts" << endl;

        start_timing();
        for (size_t i = 0; i < num_expanded; i++) {
            MatPoly sigma(1, 1, false);
            if (i == idx_dim0) sigma.data[0] = (scale_k) % Q_i;
            MatPoly cv = encryptSimpleRegev(sigma);
            cv_v.push_back(cv);
        }
        time_query_gen += end_timing();
    }

    // vector<MatPoly> X_v, Y_v, W_v, W_exp_v;
    // getPublicEncryptions(1, X_v, Y_v, W_v, W_exp_v, m_conv, m_exp, true);

    bool debugging_here = false;

    // Run 'composition'
    size_t composed_ct_size_coeffs = n1 * n0 * crt_count * poly_len;
    MatPoly C_reg(n1, n0);

    // scratch matrices
    MatPoly cv_0(1, 1);
    MatPoly cv_1(1, 1);
    MatPoly cv_ntti(1, 1, false);
    MatPoly square_cv(n0, n0, false);
    MatPoly ginv_c(n0 * m_conv, n0, false);
    MatPoly ginv_c_nttd(n0 * m_conv, n0);
    MatPoly prod_W_ginv(n1, n0);
    MatPoly padded_cv_1(n1, n0);
    MatPoly ginv_c_raw(m_conv, 1, false);
    MatPoly ginv_c_raw_nttd(m_conv, 1);

    // MARK: ScalToMatSetup to find W. Part of RegevToGSWSetup
    //       to retrieve the conversion key.
    // @see pg. 935, Section A.
    size_t m_conv_n0 = n0 * m_conv;
    MatPoly G_scale = buildGadget(n0, m_conv_n0);
    MatPoly s0 = sr_mp;
    MatPoly s0G = mul_by_const(to_ntt(s0), to_ntt(G_scale));
    MatPoly s0G_padded(n1, m_conv_n0);
    place(s0G_padded, s0G, 1, 0);
    start_timing();
    MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_mp, m_conv_n0));
    MatPoly W(n1, m_conv_n0);
    add(W, P, s0G_padded);
    time_key_gen += end_timing();
    add_pub_param(W);

    // MARK: First dimension expansion.
    start_timing();
    for (size_t i = 0; i < num_expanded; i++) {
        scalToMat(
            m_conv,
            C_reg,
            cv_v[i],
            W,
            cv_0,
            cv_1,
            cv_ntti,
            square_cv,
            ginv_c,
            ginv_c_nttd,
            prod_W_ginv,
            padded_cv_1,
            ginv_c_raw,
            ginv_c_raw_nttd
        );

        memcpy(
            &expansionLocals.cts[i * composed_ct_size_coeffs],
            C_reg.data,
            composed_ct_size_coeffs * sizeof(uint64_t)
        );
    }
    double composition_time = end_timing();
    cout << "ScalToMat took (CPU·us): " << composition_time << endl;
    time_conversion += composition_time;

    // getPublicEncryptions(1, X_v, Y_v, W_v, W_exp_v, m_conv, m_exp);
    // Run 'conversion'
    MatPoly c_ntti(n0, 1, false);           // n0 x 1
    MatPoly W_switched(n0, 1);                       // n0 x 1, ntt
    MatPoly Y_switched(n0, 1);                       // n0 x 1, ntt
    MatPoly C_v_0(n0, m);                            // n0 x m, ntt
    MatPoly C_v_1(n0, m);                            // n0 x m, ntt
    MatPoly C_v_0_ntti(n0, m, false);       // n0 x m
    MatPoly C_v_1_ntti(n0, m, false);       // n0 x m
    MatPoly ginv_Ct(m_conv, m, false);      // m_conv x m
    MatPoly ginv_Ct_ntt(m_conv, m);                  // m_conv x m, ntt
    MatPoly prod0(n1, m);                            // n1 x m, ntt
    MatPoly prod1(n1, m);                            // n1 x m, ntt
    MatPoly Cp(n1, m);
    MatPoly Cp_raw(n1, m, false);

    // WARN: Redundant. Gadget matrix is not used.
    MatPoly G(n1, m, false);
    buildGadget(G);
    MatPoly G_nttd = to_ntt(G);

    // MARK: Calculate V. Part of RegevToGSWSetup
    //       to retrieve the conversion key.
    size_t m_conv_2 = m_conv * 2;
    MatPoly V(n1, m_conv_2);
    MatPoly Sp_mp_ntt = to_ntt(Sp_mp);
    start_timing();
    {
        MatPoly gv = to_ntt(buildGadget(1, m_conv));
        // NOTE: Sp_mp is s_gsw.
        MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_mp, m_conv_2));
        // MatPoly P(n1, m_conv_2);
        // NOTE: s0 = sr_mp = s_regev. s0 * g_z_conv.
        MatPoly scaled_gv = mul_by_const(to_ntt(s0), gv);
        MatPoly together(1, m_conv_2);
        place(together, scaled_gv, 0, 0);
        place(together, gv, 0, m_conv);
        // NOTE: -s_gsw * (s0 * g_z_conv)
        MatPoly result = multiply(Sp_mp_ntt, together);
        MatPoly result_padded(n1, m_conv_2);
        place(result_padded, result, 1, 0);
        // NOTE: s_gsw_public + [s_gsw * (s0 * g_z_conv)]
        add(V, P, result_padded);
    }
    time_key_gen += end_timing();

    if (directly_upload_later_cts) {
        start_timing();
        for (size_t i = 0; i < further_dims; i++) {
            size_t cv_v_offset = num_expanded + i * ell;
            uint64_t bit = (idx_further & (1 << i)) >> i;

            for (size_t j = 0; j < ell; j++) {
                uint64_t val = bit ? (1UL << (j * bits_per)) : 0;
                cv_v.push_back(encryptSimpleRegev(single_poly(val)));
            }
        }
        time_query_gen += end_timing();
    } else {
        // MARK: Look into this.
        add_pub_param(V);
    }

    // Mark: GSW ciphertext expansion.
    start_timing();
    for (size_t i = 0; i < further_dims; i++) {
        size_t cv_v_offset = num_expanded + i * ell;

        regevToGSW(
            m_conv, m2 / n1, Cp,
            cv_v, cv_v_offset, W, V
        );

        memcpy(
            &g_Q_nttd[(further_dims - 1 - i) * n1 * m * crt_count * poly_len],
            Cp.data,
            n1 * m * crt_count * poly_len * sizeof(uint64_t)
        );

        from_ntt(Cp_raw, Cp);
        // NOTE: Look into this further.
        to_simple_crtd(&g_Q_crtd[(further_dims - 1 - i) * n1 * m * poly_len], Cp_raw);
    }
    double conversion_time = end_timing();
    cout << "RegevToGSW took (CPU·us): " << conversion_time << endl;
    time_conversion += conversion_time;
}

/**
 * @brief Performs the end-to-end PIR test of Spiral/SpiralStream.
 * @param[in, out] expansionLocals Storage container for variables used during
 *        query expansion.
 * @param[in, out] furtherDimsLocals Storage container for variables used during
 *       query processing.
 * @param g_C_fft_crtd
 *       @warning Possible bug. @see process_query_fast() @param g_C_fft_crtd
 * @param g_Q_crtd Single scalar Regev ciphertext representing the client's query
 *       @related generate_query() @param g_Q_crtd
 * @param g_Ws_fft Not used.
 *
 * @details
 * Completes the following:
 * - Latter half of Setup()
 * - Query()
 * - Answer()
 * - Extract()
*/
void process_crtd_query(
    ExpansionLocals expansionLocals,
    FurtherDimsLocals furtherDimsLocals,
    const uint64_t *g_C_fft_crtd,      // expand this ct
    uint64_t *g_Q_crtd,                // further dims query
    const uint64_t *g_Ws_fft           // use this setup data
) {
    size_t setupData_size_bytes = num_expansions * (n1 * mh * poly_len * sizeof(uint64_t));  // WARN: Not used.
    size_t query_C_crtd_size_bytes = n1 * m1 * poly_len * sizeof(uint64_t);
    size_t query_later_inp_cts_crtd_size_bytes = further_dims * n1 * m2 *poly_len * sizeof(uint64_t);

    size_t num_bytes_per_Q = n1 * m2 * crt_count * poly_len * sizeof(uint64_t);
    uint64_t *g_Q = (uint64_t *)malloc(further_dims * num_bytes_per_Q);
    uint64_t *g_Q_neg = (uint64_t *)malloc(further_dims * num_bytes_per_Q);

    cout << "Beginning query processing..." << endl;

    uint64_t *g_Q_nttd = (uint64_t *)malloc(crt_count * query_later_inp_cts_crtd_size_bytes);
    // MARK: Perform expansion on the query ciphertext.
    runConversionImproved(
        expansionLocals,
        g_Q_nttd,
        g_Q_crtd
    );

    start_timing();
    // NOTE: Perform representation optimisation and negate further dimensions
    //       query. Look into this further.
    uint64_t *g_Q_neg_crtd = (uint64_t *)malloc(query_later_inp_cts_crtd_size_bytes);
    #pragma omp parallel for
    for (size_t j = 0; j < further_dims; j++) {
        for (size_t r = 0; r < n1; r++) {
            for (size_t m = 0; m < m2; m++) {
                for (size_t z = 0; z < poly_len; z++) {
                    size_t idx_base = j*(n1 * m2 * poly_len);
                    size_t idx = r*(m2 * poly_len) + m*(poly_len) + z;
                    long val = (long)(G2.data[(r * G2.cols + m) * coeff_count + z]) - (long)g_Q_crtd[idx_base + idx];
                    if (val < 0) val += Q_i_u128;
                    g_Q_neg_crtd[idx_base + idx] = val;
                }
            }
        }
    }
    uint64_t *g_Q_neg_nttd = (uint64_t *)malloc(crt_count * query_later_inp_cts_crtd_size_bytes);
    cpu_crt_to_ucompressed_and_ntt(g_Q_neg_nttd, g_Q_neg_crtd, further_dims * n1 * m2);
    free(g_Q_neg_crtd);

    #pragma omp parallel for
    for (size_t j = 0; j < further_dims; j++) {
        size_t idx = j*(n1 * m2 * crt_count * poly_len);
        reorient_Q(&g_Q[idx],       &g_Q_nttd[idx]);
        reorient_Q(&g_Q_neg[idx],   &g_Q_neg_nttd[idx]);
    }
    time_conversion += end_timing();

    free(g_Q_nttd);
    free(g_Q_neg_nttd);

    // WARN: g_C_fft_crtd from setup is ignored over here.
    uint64_t *g_C_fft = (uint64_t *)malloc(crt_count * query_C_crtd_size_bytes);

    record("preprocess query");

    // MARK: Process the query.
    process_query_fast(
        g_C_fft,      // expand this ct
        g_Ws_fft,
        g_Q,          // further dims query
        g_Q_neg,
        expansionLocals,
        furtherDimsLocals
    );

    cout << "Done with query processing!" << endl;
}


/**
 * @brief Performs an end-to-end PIR test of the basic Spiral/SpiralStream
 *        scheme.
 *
 * @note SpiralPack is not tested here, but in @file testing.cpp
 *       @see testHighRate()
*/
void do_test() {
    // =====================================================================
    // do setup
    // =====================================================================

    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    cout << "dim0: " << dim0 << endl;
    cout << "num_per: " << num_per << endl;
    if (total_n % dim0 != 0)
        exit(1);

    // MARK: Allocate heap space to hold ciphertexts and related data during
    //       the expansion and folding process.
    ExpansionLocals expansionLocals;
    expansionLocals.allocate();
    FurtherDimsLocals furtherDimsLocals(num_per);
    furtherDimsLocals.allocate();

    size_t num_expanded = 1 << num_expansions;
    size_t n1_padded = expansionLocals.n1_padded;

    // =====================================================================
    // generate query and setup data
    // =====================================================================

    // MARK: Indicate the number of runs to perform.
    size_t num_trials = 1;
    checking_for_debug = true;
    std::vector<double> vars;
    for (size_t trial = 0; trial < num_trials; trial++) {
        uint64_t *g_C_fft_crtd;
        uint64_t *g_Q_crtd;
        uint64_t *g_Ws_fft;
        // MARK: Initialise the server for testing.
        generate_setup_and_query(
            IDX_TARGET,
            &g_C_fft_crtd,      // expand this ct
            &g_Q_crtd,          // further dims query
            &g_Ws_fft,          // use this setup data
            false               // no need to compress setup data
        );
        // MARK: Perform the end-to-end PIR test.
        process_crtd_query(
            expansionLocals,
            furtherDimsLocals,
            g_C_fft_crtd,      // expand this ct
            g_Q_crtd,          // further dims query
            g_Ws_fft           // use this setup data
        );

        if (checking_for_debug) {
            double log_var;
            if (modswitch_on_server) {
                // MARK: Modulus Switching.
                modswitch(furtherDimsLocals.result, furtherDimsLocals.cts);
                // MARK: Decoding.
                log_var = check_final(furtherDimsLocals, modswitch_on_server);
            } else {
                log_var = check_final(furtherDimsLocals, modswitch_on_server);
            }

            vars.push_back(log_var);
        }

        print_summary();
    }
}

// _________________________________________________________________________________

void saveToFile(const std::vector<MatPoly>& keyArray, const std::string &fileName) {
    std::ofstream outFile(fileName, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        return;
    }
    auto writeMatPoly = [&outFile](const MatPoly &mat) {
        outFile.write(reinterpret_cast<const char*>(&mat.rows), sizeof(mat.rows));
        outFile.write(reinterpret_cast<const char*>(&mat.cols), sizeof(mat.cols));
        outFile.write(reinterpret_cast<const char*>(&mat.isNTT), sizeof(mat.isNTT));

        size_t dataSize = mat.rows * mat.cols * (mat.isNTT ? crt_count : 1) * coeff_count;
        outFile.write(reinterpret_cast<const char*>(mat.data), dataSize * sizeof(uint64_t));
    };
    for (const auto & key: keyArray) {
        writeMatPoly(key);
    }
    if (!outFile.good()) {
        std::cerr << "An error occurred while writing to the file: " << fileName << std::endl;
    }
    outFile.close();
}

void saveToFile(const FurtherDimsLocals& obj, const std::string &fileName) {
    std::ofstream outFile(fileName, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        return;
    }
    outFile.write(reinterpret_cast<const char*>(&obj.num_per), sizeof(obj.num_per));
    outFile.write(reinterpret_cast<const char*>(&obj.num_bytes_C), sizeof(obj.num_bytes_C));
    // Write uint64_t arrays.
    outFile.write(reinterpret_cast<const char*>(obj.result), 2 * obj.num_bytes_C);
    outFile.write(reinterpret_cast<const char*>(obj.cts), 2 * obj.num_bytes_C);
    outFile.write(reinterpret_cast<const char*>(obj.scratch_cts1), obj.num_bytes_C);
    outFile.write(reinterpret_cast<const char*>(obj.scratch_cts2), obj.num_bytes_C);
    outFile.write(reinterpret_cast<const char*>(obj.scratch_cts_double1), (m2 / n1) * obj.num_bytes_C);
    outFile.write(reinterpret_cast<const char*>(obj.scratch_cts_double2), (m2 / n1) * obj.num_bytes_C);

    outFile.close();
}

void readFromFileStream(
        std::ifstream& inFile,
        std::initializer_list<std::reference_wrapper<MatPoly>> mats
    ) {
    auto readMatPoly = [&inFile](MatPoly &mat) {
        inFile.read(reinterpret_cast<char*>(&mat.rows), sizeof(mat.rows));
        inFile.read(reinterpret_cast<char*>(&mat.cols), sizeof(mat.cols));
        inFile.read(reinterpret_cast<char*>(&mat.isNTT), sizeof(mat.isNTT));

        size_t dataSize = mat.rows * mat.cols * (mat.isNTT ? crt_count : 1) * coeff_count;
        if (mat.data != nullptr) {
            free(mat.data);
        }
        mat.data = (uint64_t*) calloc(dataSize, sizeof(uint64_t));
        inFile.read(reinterpret_cast<char*>(mat.data), dataSize * sizeof(uint64_t));
    };
    for (auto&& matRef : mats) {
        readMatPoly(matRef.get());
    }
}

void loadFromFile(std::vector<MatPoly>& keyArray, const std::string& fileName) {
    std::ifstream inFile(fileName, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        return;
    }
    while (inFile.good()) {
        MatPoly key;
        readFromFileStream(inFile, {key});
        if (inFile.eof()) break;
        keyArray.push_back(key);
    }
    inFile.close();
}

void loadFromFile(
        std::initializer_list<std::reference_wrapper<MatPoly>> matsToLoadInto,
        const std::string& fileName
    ) {
    std::ifstream inFile(fileName, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        return;
    }
    readFromFileStream(inFile, matsToLoadInto);
    inFile.close();
}

void loadFromFile(FurtherDimsLocals& obj, const std::string &fileName) {
    std::ifstream inFile(fileName, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        return;
    }
    inFile.read(reinterpret_cast<char*>(&obj.num_per), sizeof(obj.num_per));
    inFile.read(reinterpret_cast<char*>(&obj.num_bytes_C), sizeof(obj.num_bytes_C));
    obj.allocate();
    inFile.read(reinterpret_cast<char*>(obj.result), 2 * obj.num_bytes_C);
    inFile.read(reinterpret_cast<char*>(obj.cts), 2 * obj.num_bytes_C);
    inFile.read(reinterpret_cast<char*>(obj.scratch_cts1), obj.num_bytes_C);
    inFile.read(reinterpret_cast<char*>(obj.scratch_cts2), obj.num_bytes_C);
    inFile.read(reinterpret_cast<char*>(obj.scratch_cts_double1), (m2 / n1) * obj.num_bytes_C);
    inFile.read(reinterpret_cast<char*>(obj.scratch_cts_double2), (m2 / n1) * obj.num_bytes_C);
    inFile.close();
}


bool areMatsEqual(const std::vector<MatPoly>& keyArray1,
                  const std::vector<MatPoly>& keyArray2) {
    if (keyArray1.size() != keyArray2.size()) {
        std::cerr << "Key array sizes are not equal." << std::endl;
        return false;
    }
    auto compareMatPoly = [](const MatPoly &mat1, const MatPoly &mat2) -> bool {
        if (mat1.rows != mat2.rows || mat1.cols != mat2.cols || mat1.isNTT != mat2.isNTT) {
            return false;
        }
        size_t dataSize = mat1.rows * mat1.cols * (mat1.isNTT ? crt_count : 1) * coeff_count;
        return memcmp(mat1.data, mat2.data, dataSize * sizeof(uint64_t)) == 0;
    };

    for (int index = 0; index < keyArray1.size(); index++) {
        if (!compareMatPoly(keyArray1[index], keyArray2[index])) {
            return false;
        }
    }
    return true;
}

bool areFDLEqual(const FurtherDimsLocals& fdl1, const FurtherDimsLocals& fdl2) {
    if (fdl1.num_per != fdl2.num_per || fdl1.num_bytes_C != fdl2.num_bytes_C) {
        std::cerr << "FurtherDimsLocals properties are not equal." << std::endl;
        return false;
    }
    // Compare uint64_t arrays.
    if (memcmp(fdl1.result, fdl2.result, 2 * fdl1.num_bytes_C) != 0) return false;
    if (memcmp(fdl1.cts, fdl2.cts, 2 * fdl1.num_bytes_C) != 0) return false;
    if (memcmp(fdl1.scratch_cts1, fdl2.scratch_cts1, fdl1.num_bytes_C) != 0) return false;
    if (memcmp(fdl1.scratch_cts2, fdl2.scratch_cts2, fdl1.num_bytes_C) != 0) return false;
    if (memcmp(fdl1.scratch_cts_double1, fdl2.scratch_cts_double1, m2 / n1 * fdl1.num_bytes_C) != 0) return false;
    if (memcmp(fdl1.scratch_cts_double2, fdl2.scratch_cts_double2, m2 / n1 * fdl1.num_bytes_C) != 0) return false;
    return true;
}


/**
 * @brief Save S, Sp, sr and automorphism keys as a JSON file.
 *
 * @returns Query key, qk=(s, S)
 */
void setup_main(MatPoly& S_Setup, MatPoly& Sp_Setup, MatPoly& sr_Setup) {
    // Create querying keys.
    GlobalTimer::set("Query key creation");
    S_Setup = MatPoly(n0, n1, false);
    Sp_Setup = MatPoly(n0, k_param, false);
    sr_Setup = MatPoly(1, 1, false);
    keygen(S_Setup, Sp_Setup, sr_Setup);
    GlobalTimer::stop("Query key creation");
    saveToFile({S_Setup, Sp_Setup, sr_Setup}, "querying_keys.bin");

    MatPoly S_Setup_Load, Sp_Setup_Load, sr_Setup_Load;
    loadFromFile({S_Setup_Load, Sp_Setup_Load, sr_Setup_Load}, "querying_keys.bin");
    assert(areMatsEqual({S_Setup, Sp_Setup, sr_Setup},
                        {S_Setup_Load, Sp_Setup_Load, sr_Setup_Load}));
    // Create automorphism keys.
    GlobalTimer::set("Automorphism key creation");
    size_t num_expanded = 1 << num_expansions;
    size_t m = m2;
    size_t ell = m / n1;
    size_t num_bits_to_gen = ell * further_dims + num_expanded;
    auto g = (size_t) ceil(log2((double)( num_bits_to_gen )));
    // Determine stop round.
    size_t qe_rest = query_elems_rest;
    size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
    if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions

    vector<MatPoly> W_exp_right_v, W_exp_v;
    setup_GetPublicEncryptions(
        g, sr_Setup_Load, W_exp_right_v,
        m_exp_right, stopround > 0 ? stopround+1 : 0
    );
    setup_GetPublicEncryptions(
        g, sr_Setup_Load, W_exp_v, m_exp
    );
    GlobalTimer::stop("Automorphism key creation");

    saveToFile(W_exp_right_v, "automorphism_right.bin");
    saveToFile(W_exp_v, "automorphism_left.bin");

    vector<MatPoly> test_W_exp_right_v, test_W_exp_v;
    loadFromFile(test_W_exp_right_v, "automorphism_right.bin");
    loadFromFile(test_W_exp_v, "automorphism_left.bin");
    assert(areMatsEqual(W_exp_right_v, test_W_exp_right_v));
    assert(areMatsEqual(W_exp_v, test_W_exp_v));

    // Create conversion keys W, V.
    GlobalTimer::set("Conversion key creation");
    size_t m_conv_n0 = n0 * m_conv;
    MatPoly G_scale = buildGadget(n0, m_conv_n0);
    MatPoly s0 = sr_Setup_Load;
    MatPoly s0G = mul_by_const(to_ntt(s0), to_ntt(G_scale));
    MatPoly s0G_padded(n1, m_conv_n0);
    place(s0G_padded, s0G, 1, 0);
    MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_Setup_Load, m_conv_n0));
    MatPoly W(n1, m_conv_n0);
    add(W, P, s0G_padded);

    size_t m_conv_2 = m_conv * 2;
    MatPoly V(n1, m_conv_2);
    MatPoly Sp_Setup_Load_NTT = to_ntt(Sp_Setup_Load);
    start_timing();
    {
        MatPoly gv = to_ntt(buildGadget(1, m_conv));
        // NOTE: Sp_mp is s_gsw.
        MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_Setup_Load, m_conv_2));
        // MatPoly P(n1, m_conv_2);
        // NOTE: s0 = sr_mp = s_regev. s0 * g_z_conv.
        MatPoly scaled_gv = mul_by_const(to_ntt(s0), gv);
        MatPoly together(1, m_conv_2);
        place(together, scaled_gv, 0, 0);
        place(together, gv, 0, m_conv);
        // NOTE: -s_gsw * (s0 * g_z_conv)
        MatPoly result = multiply(Sp_Setup_Load_NTT, together);
        MatPoly result_padded(n1, m_conv_2);
        place(result_padded, result, 1, 0);
        // NOTE: s_gsw_public + [s_gsw * (s0 * g_z_conv)]
        add(V, P, result_padded);
    }
    GlobalTimer::stop("Conversion key creation");

    saveToFile({W, V}, "conversion_keys.bin");

    MatPoly W_Load, V_Load;
    loadFromFile({W_Load, V_Load}, "conversion_keys.bin");
    assert(areMatsEqual({W, V}, {W_Load, V_Load}));
}

void query_main(
    const MatPoly& S_Query, const MatPoly& Sp_Query, const MatPoly& sr_Query
) {
    Evaluation::set("Query_Generation.client");
    // Initialise.
    size_t idx_dim0 = IDX_TARGET / (1 << further_dims);
    uint64_t scal_const = 1;
    uint64_t init_val = scale_k;
    size_t m = m2;
    size_t ell = m / n1;
    size_t num_expanded = 1 << num_expansions;
    size_t num_bits_to_gen = ell * further_dims + num_expanded;
    size_t qe_rest = query_elems_rest;
    size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
    if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions
    NativeLog::cout << "stopround = " << stopround << endl;
    auto g = (size_t) ceil(log2((double)( num_bits_to_gen )));
    // Perform query dimension encoding.
    MatPoly sigma(1, 1, false);
    size_t idx_further  = IDX_TARGET % (1 << further_dims);
    size_t bits_per = get_bits_per(ell);
    if (stopround != 0) {
        // Encode first dimension bits in even coefficients.
        // Encode subsequent scalars in odd coefficients.
        GlobalTimer::set("First and subsequent dimension query encoding");
        sigma.data[2*idx_dim0] = (scal_const * (__uint128_t)init_val) % Q_i;
        for (size_t i = 0; i < further_dims; i++) {
            uint64_t bit = (idx_further & (1 << i)) >> i;
            for (size_t j = 0; j < ell; j++) {
                size_t idx = i * ell + j;
                uint64_t val = (1UL << (bits_per * j)) * bit;
                sigma.data[2*idx+1] = (scal_const * (__uint128_t)val) % Q_i;
            }
        }
        GlobalTimer::stop("First and subsequent dimension query encoding");
    } else {
        // First dimension encoding.
        GlobalTimer::set("First dimension query encoding");
        size_t idx_for_subround = idx_dim0 % (1 << g);
        sigma.data[idx_for_subround] = (scal_const * (__uint128_t)init_val) % Q_i;
        GlobalTimer::stop("First dimension query encoding");
        // Encode subsequent dimensions.
        GlobalTimer::set("Subsequent dimension query encoding");
        size_t offset = qe_rest == 0 ? num_expanded : 0;
        size_t subround = 0;  // WARN: Confirm subround is not needed here.
        size_t start_of_encoding = num_bits_to_gen * subround;
        size_t end_of_encoding = start_of_encoding + num_bits_to_gen;
        size_t ctr = 0;
        for (size_t i = 0; i < further_dims; i++) {
            uint64_t bit = (idx_further & (1 << i)) >> i;
            for (size_t j = 0; j < ell; j++) {
                size_t idx = i * ell + j;
                uint64_t val = (1UL << (bits_per * j)) * bit;
                if ((idx >= start_of_encoding) && (idx < end_of_encoding)) {
                    sigma.data[offset + ctr] = (scal_const * (__uint128_t)val) % Q_i;
                    ctr++;
                }
            }
        }
        GlobalTimer::stop("Subsequent dimension query encoding");
    }
    // Possible query packing?
    GlobalTimer::set("Query packing");
    if (stopround != 0) {
        uint64_t inv_2_g_first = inv_mod(1 << g, Q_i);
        uint64_t inv_2_g_rest = inv_mod(1 << (stopround+1), Q_i);
        for (size_t i = 0; i < coeff_count/2; i++) {
            sigma.data[2*i] = (sigma.data[2*i] * (__uint128_t)inv_2_g_first) % Q_i;
            sigma.data[2*i+1] = (sigma.data[2*i+1] * (__uint128_t)inv_2_g_rest) % Q_i;
        }
    } else {
        uint64_t inv_2_g = inv_mod(1 << g, Q_i);
        for (size_t i = 0; i < coeff_count; i++) {
            sigma.data[i] = (sigma.data[i] * (__uint128_t)inv_2_g) % Q_i;
        }
    }
    GlobalTimer::stop("Query packing");
    // Query encryption.
    GlobalTimer::set("Query encryption");
    MatPoly cv = query_encryptSimpleRegev(sr_Query, sigma);
    std::vector<MatPoly> round_cv_v;
    round_cv_v.push_back(cv);
    for (size_t i = 0; i < (1<<g) - 1; i++) {
        round_cv_v.emplace_back(n0, 1);
    }
    GlobalTimer::stop("Query encryption");
    Evaluation::stop("Query_Generation.client");
    // Save the query to a file.
    saveToFile(round_cv_v, "query.bin");

    std::vector<MatPoly> round_cv_Load;
    loadFromFile({round_cv_Load}, "query.bin");
    assert(areMatsEqual({round_cv_v}, {round_cv_Load}));
}

void answer_main() {
    // Initialize the server.
    size_t m = m2;
    size_t ell = m / n1;
    size_t num_expanded = 1 << num_expansions;
    size_t num_bits_to_gen = ell * further_dims + num_expanded;
    uint64_t *g_Q_crtd = (uint64_t *)malloc(further_dims * (n1 * m2 * coeff_count) * sizeof(uint64_t));
    size_t query_later_inp_cts_crtd_size_bytes = further_dims * n1 * m2 *poly_len * sizeof(uint64_t);
    auto* g_Q_nttd = (uint64_t *)malloc(crt_count * query_later_inp_cts_crtd_size_bytes);
    ExpansionLocals expansionLocals;
    expansionLocals.allocate();
    auto g = (size_t) ceil(log2((double)( num_bits_to_gen )));
    // Load the public parameters.
    GlobalTimer::set("Public parameter loading");
    std::vector<MatPoly> W_exp_right_v_Answer, W_exp_v_Answer;
    loadFromFile(W_exp_right_v_Answer, "automorphism_right.bin");
    loadFromFile(W_exp_v_Answer, "automorphism_left.bin");
    MatPoly W_Answer, V_Answer;
    loadFromFile({W_Answer, V_Answer}, "conversion_keys.bin");
    GlobalTimer::stop("Public parameter loading");
    // Load the query.
    GlobalTimer::set("Query loading");
    std::vector<MatPoly> round_cv_v_Answer;
    loadFromFile({round_cv_v_Answer}, "query.bin");
    GlobalTimer::stop("Query loading");
    // Determine stop round.
    size_t qe_rest = query_elems_rest;
    size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
    if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions
    // Initial expansion.
    GlobalTimer::set("Initial query expansion");
    (void)expandImproved(
        round_cv_v_Answer, g, m_exp,
        W_exp_v_Answer, W_exp_right_v_Answer,
        ell * further_dims, stopround
    );
    GlobalTimer::stop("Initial query expansion");
    // Reorder ciphertexts when using stop round.
    if (stopround != 0) {
        round_cv_v_Answer = reorderFromStopround(round_cv_v_Answer, num_expanded, ell * further_dims);
    }
    // Note: The cv_v vector might be a side effect of stream testing.
    //       cv_v is used here for compatibility.
    std::vector<MatPoly> cv_v_Answer;
    cv_v_Answer.insert(
        cv_v_Answer.end(), round_cv_v_Answer.begin(),
        round_cv_v_Answer.begin() + num_bits_to_gen
    );
    // Note: Run 'composition'.
    size_t composed_ct_size_coeffs = n1 * n0 * crt_count * poly_len;
    MatPoly C_reg(n1, n0);
    // Scratch matrices.
    MatPoly cv_0(1, 1);
    MatPoly cv_1(1, 1);
    MatPoly cv_ntti(1, 1, false);
    MatPoly square_cv(n0, n0, false);
    MatPoly ginv_c(n0 * m_conv, n0, false);
    MatPoly ginv_c_nttd(n0 * m_conv, n0);
    MatPoly prod_W_ginv(n1, n0);
    MatPoly padded_cv_1(n1, n0);
    MatPoly ginv_c_raw(m_conv, 1, false);
    MatPoly ginv_c_raw_nttd(m_conv, 1);
    // First dimension expansion.
    GlobalTimer::set("First dimension query expansion");
    for (size_t i = 0; i < num_expanded; i++) {
        scalToMat(
            m_conv,
            C_reg,
            cv_v_Answer[i],
            W_Answer,
            cv_0,
            cv_1,
            cv_ntti,
            square_cv,
            ginv_c,
            ginv_c_nttd,
            prod_W_ginv,
            padded_cv_1,
            ginv_c_raw,
            ginv_c_raw_nttd
        );

        memcpy(
            &expansionLocals.cts[i * composed_ct_size_coeffs],
            C_reg.data,
            composed_ct_size_coeffs * sizeof(uint64_t)
        );
    }
    GlobalTimer::stop("First dimension query expansion");
    // Run 'conversion'
    MatPoly c_ntti(n0, 1, false);           // n0 x 1
    MatPoly W_switched(n0, 1);                       // n0 x 1, ntt
    MatPoly Y_switched(n0, 1);                       // n0 x 1, ntt
    MatPoly C_v_0(n0, m);                            // n0 x m, ntt
    MatPoly C_v_1(n0, m);                            // n0 x m, ntt
    MatPoly C_v_0_ntti(n0, m, false);       // n0 x m
    MatPoly C_v_1_ntti(n0, m, false);       // n0 x m
    MatPoly ginv_Ct(m_conv, m, false);      // m_conv x m
    MatPoly ginv_Ct_ntt(m_conv, m);                  // m_conv x m, ntt
    MatPoly prod0(n1, m);                            // n1 x m, ntt
    MatPoly prod1(n1, m);                            // n1 x m, ntt
    MatPoly Cp(n1, m);
    MatPoly Cp_raw(n1, m, false);
    // GSW ciphertext expansion.
    GlobalTimer::set("GSW ciphertext expansion");
    for (size_t i = 0; i < further_dims; i++) {
        size_t cv_v_offset = num_expanded + i * ell;
        regevToGSW(
            m_conv, m2 / n1, Cp,
            cv_v_Answer, cv_v_offset, W_Answer, V_Answer
        );
        memcpy(
            &g_Q_nttd[(further_dims - 1 - i) * n1 * m * crt_count * poly_len],
            Cp.data,
            n1 * m * crt_count * poly_len * sizeof(uint64_t)
        );
        from_ntt(Cp_raw, Cp);
        // NOTE: Look into this further.
        to_simple_crtd(&g_Q_crtd[(further_dims - 1 - i) * n1 * m * poly_len], Cp_raw);
    }
    GlobalTimer::stop("GSW ciphertext expansion");
    // Perform representation optimisation? and negate further dimensions query.
    auto* g_Q_neg_crtd = (uint64_t *)malloc(query_later_inp_cts_crtd_size_bytes);
    #pragma omp parallel for
    for (size_t j = 0; j < further_dims; j++) {
        for (size_t r = 0; r < n1; r++) {
            for (size_t m = 0; m < m2; m++) {
                for (size_t z = 0; z < poly_len; z++) {
                    size_t idx_base = j*(n1 * m2 * poly_len);
                    size_t idx = r*(m2 * poly_len) + m*(poly_len) + z;
                    long val = (long)(G2.data[(r * G2.cols + m) * coeff_count + z]) - (long)g_Q_crtd[idx_base + idx];
                    if (val < 0) val += Q_i_u128;
                    g_Q_neg_crtd[idx_base + idx] = val;
                }
            }
        }
    }
    auto* g_Q_neg_nttd = (uint64_t *)malloc(crt_count * query_later_inp_cts_crtd_size_bytes);
    cpu_crt_to_ucompressed_and_ntt(g_Q_neg_nttd, g_Q_neg_crtd, further_dims * n1 * m2);
    free(g_Q_neg_crtd);
    // Reorient query matrix.
    size_t num_bytes_per_Q = n1 * m2 * crt_count * poly_len * sizeof(uint64_t);
    auto* g_Q = (uint64_t *)malloc(further_dims * num_bytes_per_Q);
    auto* g_Q_neg = (uint64_t *)malloc(further_dims * num_bytes_per_Q);
    #pragma omp parallel for
    for (size_t j = 0; j < further_dims; j++) {
        size_t idx = j*(n1 * m2 * crt_count * poly_len);
        reorient_Q(&g_Q[idx], &g_Q_nttd[idx]);
        reorient_Q(&g_Q_neg[idx], &g_Q_neg_nttd[idx]);
    }
    free(g_Q_nttd);
    free(g_Q_neg_nttd);
    // Perform query processing.
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    FurtherDimsLocals furtherDimsLocals(num_per);
    furtherDimsLocals.allocate();
    answer_process_query_fast(
        g_Q,
        g_Q_neg,
        expansionLocals,
        furtherDimsLocals
    );
    // Rescale the response via modulus switching.
    GlobalTimer::set("Rescale response via modulus switching");
    modswitch(furtherDimsLocals.result, furtherDimsLocals.cts);
    GlobalTimer::stop("Rescale response via modulus switching");
    // Save the rescaled response to a file.
    saveToFile(furtherDimsLocals, "response.bin");
    FurtherDimsLocals furtherDimsLocals_Load(num_per);
    loadFromFile(furtherDimsLocals_Load, "response.bin");
    assert(areFDLEqual(furtherDimsLocals, furtherDimsLocals_Load));
}

void decodePoly(MatPoly& decodedMessage, const size_t queryIndex) {
    const PlaintextConversionConfig config(decodedMessage);
    std::stringstream decodedMessageStream;
    const size_t recordIndex = IDX_TARGET;
    const size_t readHeadOffset = queryIndex - (config.hashesPerPoly * recordIndex);
    const size_t readHeadStart = config.coefficientsPerHash * readHeadOffset;
    const size_t databaseCapacity = config.hashesPerPoly * total_n;
    const size_t readHeadEnd = readHeadStart + config.coefficientsPerHash;
    assert(readHeadEnd < databaseCapacity);
    for (size_t readHead = readHeadStart; readHead < readHeadEnd; readHead++) {
        if (config.coefficientsPerCharacter == 1) {
            decodedMessageStream << HexToolkit::base16ToHex[decodedMessage.data[readHead]];
        } else if (config.coefficientsPerCharacter < 1) {
            assert(config.plaintextModulus == 256);
            auto [char1, char2] = unpackBit(decodedMessage.data[readHead]);
            decodedMessageStream << HexToolkit::base16ToHex[char1] << HexToolkit::base16ToHex[char2];
        } else if (config.coefficientsPerCharacter > 1) {
            std::vector<int> read_buckets;
            for (size_t j = 0; j < static_cast<size_t>(config.coefficientsPerCharacter); j++) {
                read_buckets.push_back(static_cast<int>(decodedMessage.data[readHead + j]));
            }
            for (const auto& [hexBase16, bucket] : HexToolkit::hexBase16ToBucketMap) {
                if (bucket == read_buckets) {
                    decodedMessageStream << HexToolkit::base16ToHex[hexBase16];
                    break;
                }
            }
            // [NOTE] Advance the read head to the start of the next character bucket.
            readHead += static_cast<size_t>(config.coefficientsPerCharacter) - 1;
        }
    }
    bool showMessages = false;
    if (showMessages) {
        Log::cout << "Message dump: \n" << std::endl;
        bool allZeros = true;
        for (size_t i = 0; i < decodedMessage.rows * decodedMessage.cols * coeff_count; i++) {
            if (decodedMessage.data[i] != 0) {
                allZeros = false;
            }
            if (i >= readHeadStart && i < readHeadEnd) {
                std::cout << UnixColours::GREEN;
            }
            if (i == readHeadEnd) {
                std::cout << UnixColours::RESET;
            }
            std::cout << decodedMessage.data[i] << ",";
        }
        std::cout << std::endl << std::endl;
        Log::cout << "Message is " << (allZeros ? "empty." : "NOT empty.") << std::endl;
    }
    Log::cout << "Decoded hash: " << UnixColours::MAGENTA
              << decodedMessageStream.str() << UnixColours::RESET
              << std::endl;
    // Validate hash.
    const bool hashExistsInFile =
            Process::Data::hashStore().get<Container::Hash>().find(decodedMessageStream.str()) !=
            Process::Data::hashStore().end();
    std::cout << "[" << UnixColours::MAGENTA << "Check"
              << UnixColours::RESET << "] Hash is " << UnixColours::MAGENTA
              << (hashExistsInFile ? "valid." : "invalid.")
              << UnixColours::RESET << std::endl;
    const bool hashIsPositionedCorrectly =
            Process::Data::retrieveHashAtIndex(queryIndex) == decodedMessageStream.str();
    std::cout << "[" << UnixColours::MAGENTA << "Check"
              << UnixColours::RESET << "] Hash is positioned " << UnixColours::MAGENTA
              << (hashExistsInFile ? "correctly." : "incorrectly.")
              << UnixColours::RESET << std::endl;
}

void extract_main(
    const MatPoly& S_Extract,
    const MatPoly& Sp_Extract,
    const size_t queryIndex
) {
    // Load server response.
    GlobalTimer::set("Load server response");
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    FurtherDimsLocals furtherDimsLocals(num_per);
    loadFromFile(furtherDimsLocals, "response.bin");
    GlobalTimer::stop("Load server response");
    Evaluation::set("Extract_Response.client");
    // Setup for extraction.
    MatPoly Sp_mp_nttd_qprime(n0, k_param, false);
    Sp_mp_nttd_qprime = Sp_Extract;
    to_ntt_qprime(Sp_mp_nttd_qprime);
    time_key_gen += end_timing();

    MatPoly ct_inp(n1, n2, false);
    for (size_t i = 0; i < n1 * n2 * poly_len; i++) {
        ct_inp.data[i] = furtherDimsLocals.cts[i];
    }
    uint64_t q_1 = 4*p_db;
    // Rescale the final encoding (PIR response).
    GlobalTimer::set("Rescale final encoding (server response)");
    MatPoly first_row = pick(ct_inp, 0, 0, 1, ct_inp.cols);
    MatPoly first_row_sw = getRescaled(first_row, Q_i, arb_qprime);
    MatPoly rest_rows = pick(ct_inp, 1, 0, ct_inp.rows - 1, ct_inp.cols);
    MatPoly rest_rows_sw = getRescaled(rest_rows, Q_i, q_1);
    MatPoly total_resp(n1, n2, false);
    place(total_resp, first_row_sw, 0, 0);
    place(total_resp, rest_rows_sw, 1, 0);
    GlobalTimer::stop("Rescale final encoding (server response)");
    // Recover takes in rescaled encoding and the secret key and outputs Z.
    GlobalTimer::set("Perform Z <- Recover_q1_q2(S, r) operation to calculate Z");
    MatPoly first_row_decoded = pick(total_resp, 0, 0, 1, total_resp.cols);
    MatPoly rest_rows_decoded = pick(total_resp, 1, 0, total_resp.rows - 1, total_resp.cols);
    to_ntt_qprime(first_row_decoded);
    MatPoly s_prod = mul_over_qprime(Sp_mp_nttd_qprime, first_row_decoded);
    from_ntt_qprime(s_prod);  // Z.
    GlobalTimer::stop("Perform Z <- Recover_q1_q2(S, r) operation to calculate Z");
    // Decode the response to get the encoded message.
    GlobalTimer::set("Perform C <- Decode(Z) operation to get the encoded message");
    MatPoly M_result(n0, n0, false);
    for (size_t i = 0; i < s_prod.rows * s_prod.cols * poly_len; i++) {
        int64_t val_first = s_prod.data[i];
        if (val_first >= arb_qprime/2) val_first -= arb_qprime;
        int64_t val_rest = rest_rows_decoded.data[i];
        if (val_rest >= q_1/2) val_rest -= q_1;

        uint64_t denom = arb_qprime * (q_1/p_db);

        int64_t r = val_first * q_1;
        r +=  val_rest * arb_qprime;
        // divide r by arb_qprime, rounding
        int64_t sign = r >= 0 ? 1 : -1;
        __int128_t val = r;
        __int128_t result = (r + sign*((int64_t)denom/2)) / (__int128_t)denom;
        result = (result + (denom/p_db)*p_db + 2*p_db) % p_db;

        s_prod.data[i] = (uint64_t)result;
    }
    M_result = s_prod;  // M.
    GlobalTimer::stop("Perform C <- Decode(Z) operation to get the encoded message");
    decodePoly(M_result, queryIndex);
    Evaluation::stop("Extract_Response.client");
}

size_t retrieveRecordIndex(const size_t queryIndex) {
    const PlaintextConversionConfig config(2);
    const size_t databaseRecordCount = total_n;
    const size_t hashesPerRecord = config.hashesPerPoly;
    const size_t recordIndex = queryIndex / hashesPerRecord;
    assert(recordIndex < databaseRecordCount);
    return recordIndex;
}


void processExitStrategy(int signal) {
    Log::cout << "Received signal " << signal
              << ". Exiting process." << std::endl;
    free(B);
    exit(signal);
}

void parseQueryFile(const std::filesystem::path& filePath, std::vector<int>& numbers) {
    std::ifstream file(filePath);
    std::string line {};
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath.string());
    }
    while (getline(file, line)) {
        std::istringstream iss(line);
        int number {};
        if (iss >> number) {
            numbers.push_back(number);
        }
    }
}

std::vector<int> loadQueryDirectory(const std::string& queryDirectory) {
    std::vector<int> loadedQueryIndices {};
    for (const auto& entry : std::filesystem::directory_iterator(queryDirectory)) {
        if (!entry.is_regular_file()) continue;
        const auto& path = entry.path();
        if (path.extension() == ".query") {
            parseQueryFile(path, loadedQueryIndices);
        }
    }
    return loadedQueryIndices;
}

void runSeparationTest() {
    system("pwd");
    #if defined(__AVX512F__)
        std::cout << "AVX512F is enabled." << std::endl;
    #else
        std::cerr << "AVX512F is not enabled." << std::endl;
    #endif
    #if !defined(NO_CRT)
        std::cout << "CRT is enabled." << std::endl;
    #else
        std::cerr << "CRT is not enabled. Please recompile with CRT enabled." << std::endl;
        exit(1);
    #endif
    #if defined(__AVX2__)
        std::cout << "AVX2 is enabled." << std::endl;
    #else
        std::cerr << "AVX2 is not enabled." << std::endl;
    #endif
    std::cout << "Database File: " << Process::dataSpace(DATA_FILENAME) << "\n";

    std::signal(SIGINT, processExitStrategy);
    std::signal(SIGTERM, processExitStrategy);
    Log::cout << "Plaintext modulus is " << p_db
              << ", which allows for \"" << log2(p_db) / 4
              << "\" 4-bit character(s) in every coefficient." << std::endl;
    MatPoly S_Main, Sp_Main, sr_Query;
    GlobalTimer::set("Fig.2: Setup");
    setup_main(S_Main, Sp_Main, sr_Query);
    GlobalTimer::stop("Fig.2: Setup");

    Log::cout << "Database will load from " << DATA_FILENAME << "." << std::endl;
    std::vector<int> queryIndexes = loadQueryDirectory(Process::queryStoragePath);
    Process::Data::loadHashes(Process::dataSpace(DATA_FILENAME));
    GlobalTimer::set("Database Generation");
    Evaluation::set("Database_Generation.server");
    load_db();
    Evaluation::stop("Database_Generation.server");
    GlobalTimer::stop("Database Generation");
    for (int index : queryIndexes) {
        std::cout << "[" << UnixColours::CYAN << "Input"
                  << UnixColours::RESET << "] " << "Using query index: " << index << std::endl;
        const size_t loadedHashCount = Process::Data::hashStore().get<Container::Sequence>().size();
        if (index < 0 or index >= loadedHashCount) {
            std::cerr << "Query index is out of bounds." << std::endl;
            continue;
        }
        IDX_TARGET = retrieveRecordIndex(index);
        IDX_DIM0 = IDX_TARGET / (1 << further_dims);
        GlobalTimer::set("Fig.2: Query");
        query_main(S_Main, Sp_Main, sr_Query);
        GlobalTimer::stop("Fig.2: Query");
        GlobalTimer::set("Fig.2: Answer");
        answer_main();
        GlobalTimer::stop("Fig.2: Answer");
        GlobalTimer::set("Fig.2: Extract");
        extract_main(S_Main, Sp_Main, index);
        GlobalTimer::stop("Fig.2: Extract");
        system("clear");
    }
    Evaluation::writeMetrics();
    PlaintextConversionConfig logConfig = PlaintextConversionConfig(n0);
    std::cout << "Evaluation on core " << sched_getcpu() << " is complete." << std::endl;
    std::cout << "\nRun properties" << std::endl;
    std::cout << "--------------" << std::endl;
    std::cout << "Database filename: " << DATA_FILENAME << " with " << Process::Data::hashStore().get<Container::Sequence>().size() << " hashes." << std::endl;
    /*std::cout << "Actual DB size: 2^" << num_expansions + further_dims
              << " or " << total_n << " records which can store "
              << logConfig.hashesPerPoly * total_n
              << " hashes under " << logConfig.hashesPerPoly << " hashes per record." << std::endl;*/
    std::cout << "Number of folds: " << num_expansions << "." << std::endl;
    std::cout << "Further dimensions: " << further_dims << "." << std::endl;
    std::cout << "Number of queries: " << queryIndexes.size() << "." << std::endl;
}

void populateConfigurations() {
    for (int database_size = 30; database_size <= 40; database_size++) {
        Log::cout << "Generating configuration for " << database_size << "." << std::endl;
        const std::string command = "python3 ./../../Seperated/select_params.py " +
                                    std::to_string(database_size) + " 32 --explicit-db > /dev/null";
        system(command.c_str());
    }
}

// size_t queryIndex {};
// while (true) {
//     std::cout << "[" << UnixColours::CYAN << "Input"
//               << UnixColours::RESET << "] " << "Enter query index: " << std::flush;
//     std::cin >> queryIndex;
//     IDX_TARGET = retrieveRecordIndex(queryIndex);
//     IDX_DIM0 = IDX_TARGET / (1 << further_dims);
//     GlobalTimer::set("Fig.2: Query");
//     query_main(S_Main, Sp_Main, sr_Query);
//     GlobalTimer::stop("Fig.2: Query");
//     GlobalTimer::set("Fig.2: Answer");
//     answer_main();
//     GlobalTimer::stop("Fig.2: Answer");
//     GlobalTimer::set("Fig.2: Extract");
//     extract_main(S_Main, Sp_Main, queryIndex);
//     GlobalTimer::stop("Fig.2: Extract");
// }
