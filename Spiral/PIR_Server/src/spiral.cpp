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
 *            inp: n1 * n2 * poly_len * sizeof(uint64_t)
 * @return out
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
    for (size_t r = 0; r < rs; r++) {
        for (size_t c = 0; c < cs; c++) {
            for (size_t m = 0; m < poly_len; m++) {
                int64_t val = inp[r * cs * poly_len + c * poly_len + m];
                long double dbl_val = ((long double)val) * ((long double) arb_qprime) / ((long double) Q_i);
                int64_t result = (int64_t)round(dbl_val);
                assert(result <= (int64_t)arb_qprime);
                write_arbitrary_bits(out, result, bit_offs, bit_width);
                bit_offs += bit_width;
            }
        }
    }
}

auto start_stage = chrono::high_resolution_clock::now();
auto start = chrono::high_resolution_clock::now();
size_t stage = 0;
void start_timing() {
    stage = 0;
    start_stage = chrono::high_resolution_clock::now();
    start = chrono::high_resolution_clock::now();
}
void record(const string &s) {
    // Note: This has been disabled and been commented for documentation.
    // cout << "#" << s << "\t\t\t\t\t";
    // cout << " "
    //      << (chrono::high_resolution_clock::now() - start).count() / 1000
    //      << endl;
    // start = chrono::high_resolution_clock::now();
    // stage++;
}
double end_timing() {
    uint64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now() - start_stage).count();
    start_stage = chrono::high_resolution_clock::now();
    start = chrono::high_resolution_clock::now();
    stage = 0;
    return duration_us;
}

void setup_H_F_for_n2_eq_2() {
    F_mp = MatPoly(n2, n2, false);
    memset(F_inv_vals, 0, sizeof(F_inv_vals));

    H_mp = MatPoly(n0, n2, false);
    build_from_constants(H_mp, {
        {1, 0},
        {0, 1}
    });
}

void setup_constants() {
    if (n2 == 2) {
        setup_H_F_for_n2_eq_2();
    } else {
        cout << "unsupported value of n2" << endl;
        exit(1);
    }

    MatPoly ZO(n0, n2, false);
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
    size_t blocksize_r = (2*num_per) < 64 ? (2*num_per) : 64;
    size_t blocksize_c = 128;
    size_t rows = num_per*m2*n2; // 18432
    size_t cols = poly_len;      //  4096
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
    for (size_t z = 0; z < poly_len; z++) {
        for (size_t i = 0; i < num_per; i++) {
            for (size_t r = 0; r < n1; r++) {
                for (size_t c = 0; c < n2; c++) {
                    const uint64_t *C_p = &C[z * (num_per * n2 * m2) + i * (n2 * m2) + c * (m2)];
                    const uint64_t *Q_p = &Q[z * (n1*m2) + r * (m2)];

                    uint64_t sum_n_0 = 0;
                    uint64_t sum_n_1 = 0;
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
                    }
                    #endif
                    size_t C_next_idx = i * (n1*n2) + r * (n2) + c;
                    uint64_t *C_next_p = &C_next[C_next_idx * crt_count * poly_len];
                    C_next_p[z] = barrett_coeff(sum_n_0, 0);// % p_i;
                    C_next_p[poly_len + z] = barrett_coeff(sum_n_1, 1);
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
            out[i * poly_len + j] = crt_compose(base_inp[j], base_inp[j + poly_len], 0);
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
        }
        ntt_forward(&out[i * crt_count * poly_len]);
    }
}

/**
 * @brief Performs first dimension processing.
 * @param output Output buffer for the result of the first dimension processing.
 *      Note: output will get num_per ciphertexts,
 *            each of which is a matrix of polynomials in FFT form of size n1*n2.
 * @param reorientedCiphertexts Input buffer containing the reoriented ciphertexts.
 *      Note: reorientedCipherTexts is dim0 ciphertexts of n1*n0 polynomials,
 *            indexed as (poly_len, dim0, n0, n1).
 * @param database Input buffer containing the database.
 *      Note: database is the set of plaintexts as n0*n2 matrices,
 *            indexed as (poly_len, num_per, n2, dim0, n0).
 * @return void output.
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

                            uint64_t b_inp_1 = database[idx_b_base++];
                            uint64_t b_inp_2 = database[idx_b_base++];
                            __m512i b_1 = _mm512_set1_epi64(b_inp_1); // CPI: ?
                            __m512i b_2 = _mm512_set1_epi64(b_inp_2); // CPI: ?
                            __m512i b = _mm512_mask_blend_epi64(0b11110000, b_1, b_2);

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
                }
            }
        }
    #elif defined(__AVX2__) && !defined(NO_CRT)
        for (size_t z = 0; z < poly_len; z++) {
            size_t idx_a_base = z * (2*dim0*n1_padded);
            size_t idx_b_base = z * (num_per * n2 * dim0 * n0);
            if (random_data) idx_b_base = 0;
            if (random_data) idx_a_base = 0;
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
                    // output n0
                    size_t n = 0;
                    size_t idx_c = i * (n1*n2*crt_count*poly_len) + c * (crt_count*poly_len) + n * (poly_len) + z;
                    output[idx_c] = sums_out_n0_u64_acc[0] % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_u64_acc[1] % p_i;
                    idx_c += (n2*crt_count*poly_len);
                    output[idx_c] = sums_out_n0_u64_acc[2] % p_i;
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

enum Container{
    Hash,
    Sequence
};

namespace Process {
    const std::filesystem::path processPath("/home/ubuntu/Process_Workspace/");
    std::filesystem::path workspace(const std::string& filename) {
        return processPath / std::filesystem::path(filename);
    }

    const std::filesystem::path dataPath("/tmp/Spiral/Database/Development");
    std::filesystem::path dataSpace(const std::string& filename) {
        return dataPath / std::filesystem::path(filename);
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
                hashStoreRef.get<Container::Sequence>().push_back(element.value().get<string>());
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

namespace HexToolkit {
    const char base16ToHex[16] = {
            '0', '1', '2', '3',
            '4', '5', '6', '7',
            '8', '9', 'a', 'b',
            'c', 'd', 'e', 'f'
    };

    int hexCharToBase16(char hexChar) {
        if (hexChar >= '0' && hexChar <= '9') return hexChar - '0';
        if (hexChar >= 'a' && hexChar <= 'f') return hexChar - 'a' + 10;
        if (hexChar >= 'A' && hexChar <= 'F') return hexChar - 'A' + 10;
        throw std::invalid_argument("Invalid hexadecimal character");
    }

    std::map<int, std::vector<int>> hexBase16ToBucketMap {
            {0, {0, 0, 0, 0}},
            {1, {0, 0, 0, 1}},
            {2, {0, 0, 1, 1}},
            {3, {0, 1, 1, 1}},
            {4, {1, 1, 1, 1}},
            {5, {1, 1, 1, 0}},
            {6, {1, 1, 0, 0}},
            {7, {1, 0, 0, 0}},
            {8, {1, 0, 1, 0}},
            {9, {1, 0, 0, 1}},
            {10, {0, 1, 0, 1}},
            {11, {0, 1, 1, 0}},
            {12, {1, 0, 1, 1}},
            {13, {1, 1, 0, 1}},
            {14, {0, 0, 1, 0}},
            {15, {0, 1, 0, 0}}
    };
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
        if (plaintextModulus <= hexadecimalRange) {
            writeSurface = static_cast<double>(hexadecimalRange) / static_cast<double>(plaintextModulus);
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

void generatePaddedPoly(MatPoly& M, const int iterationCount, const int dummyValue = 0) {
    std::cout << "\r [" << iterationCount
              << "] Encoding dummy value into remaining polynomials."
              << std::string(100, ' ') << std::flush;
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
    std::cout << "\r [" << recordCount << " | "
              << hashCountInPoly << "]" << (hashCountInPoly < 100 ? " ": "") << (recordCount < 100 ? " ": "") << " Encoding "
              << hash << " into polynomials under " << config.coefficientsPerCharacter
              << " coefficients/character." << std::flush;
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
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    if (random_data) {
        size_t num_words = dummyWorkingSet * dim0 * num_per * n0 * n2;
        B = (uint64_t *)aligned_alloc(64, sizeof(uint64_t) * num_words);
        random_device rd;
        mt19937_64 gen2(rd()); // NOT SECURE
        uniform_int_distribution<uint64_t> dist(numeric_limits<uint64_t>::min(), numeric_limits<uint64_t>::max());
        uint64_t val = dist(gen2) % (p_db/4);

        for (size_t r = 0; r < n0; r++) {
            for (size_t c = 0; c < n2; c++) {
                pt_real.data[(r * n2 + c) * poly_len] = val;
            }
        }

        MatPoly pt_encd_raw = pt_real;
        for (size_t pol = 0; pol < n0 * n2 * poly_len; pol++) {
            int64_t val = (int64_t) pt_encd_raw.data[pol];
            assert(val >= 0 && val < p_db);
            if (val >= (p_db / 2)) {
                val = val - (int64_t)p_db;
            }
            if (val < 0) {
                val += Q_i;
            }
            assert(val >= 0 && val < Q_i);
            pt_encd_raw.data[pol] = val;
        }
        to_ntt(pts_encd, pt_encd_raw);
        for (size_t dws = 0; dws < dummyWorkingSet; dws++) {
            for (size_t i = 0; i < num_per; i++) {
                for (size_t j = 0; j < n2; j++) {
                    for (size_t k = 0; k < dim0; k++) {
                        for (size_t l = 0; l < n0; l++) {
                            size_t idx = dws * (num_per * n2 * dim0 * n0)
                                        + i * (n2 * dim0 * n0)
                                        + j * (dim0 * n0)
                                        + k * (n0)
                                        + l;
                            uint64_t val1 = pts_encd.data[(j * n0 + l)*crt_count*poly_len];
                            uint64_t val2 = pts_encd.data[(j * n0 + l)*crt_count*poly_len + poly_len];
                            B[idx] = val1 + (val2 << 32);
                        }
                    }
                }
            }
        }
        pt_encd_correct = MatPoly(n0, n2);
        pt_correct = MatPoly(n0, n0);
        return;
    }
    size_t num_bytes_B = sizeof(uint64_t) * dim0 * num_per * n0 * n2 * poly_len;//2 * poly_len;
    NativeLog::cout << "num_bytes_B: " << num_bytes_B << endl;
    B = (uint64_t *)aligned_alloc(64, num_bytes_B);
    memset(B, 0, num_bytes_B);

    uint64_t *BB = (uint64_t *)malloc(n0 * n2 * crt_count * poly_len * sizeof(uint64_t));
    size_t numBytesPlaintextRaw = n0 * n0 * num_bits_q * poly_len / 8;
    uint64_t *pt_raw = (uint64_t *)malloc(numBytesPlaintextRaw);
    memset(pt_raw, 0, numBytesPlaintextRaw);
    uint64_t *pt_buf = (uint64_t *)malloc(n0 * n0 * crt_count * poly_len * sizeof(uint64_t));
    memset(pt_buf, 0, n0 * n0 * crt_count * poly_len * sizeof(uint64_t));

    NativeLog::cout << "starting generation of db" << endl;
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
        pt_encd_raw = pt_tmp;
        for (size_t pol = 0; pol < n0 * n2 * poly_len; pol++) {
            int64_t val = (int64_t) pt_encd_raw.data[pol];
            assert(val >= 0 && val < p_db);
            if (val >= (p_db / 2)) {
                val = val - (int64_t)p_db;
            }
            if (val < 0) {
                val += Q_i;
            }
            assert(val >= 0 && val < Q_i);
            pt_encd_raw.data[pol] = val;
        }
        to_ntt(pts_encd, pt_encd_raw);
        if (i == IDX_TARGET) {
            cop(pt_encd_correct, pts_encd);
            cop(pt_real, pt_tmp);
            to_ntt(pt, pt_tmp);
            cop(pt_correct, pt);
        }
        // b': i c n z j m
        size_t ii = i % num_per;
        size_t j = i / num_per;
        for (size_t m = 0; m < n0; m++) {
            for (size_t c = 0; c < n2; c++) {
                memcpy(BB, &pts_encd.data[(m * n2 + c) * crt_count * coeff_count], crt_count * coeff_count * sizeof(uint64_t));
                for (size_t z = 0; z < poly_len; z++) {
                    size_t idx = z * (num_per * n2 * dim0 * n0) +
                                 ii * (n2 * dim0 * n0) +
                                 c * (dim0 * n0) +
                                 j * (n0) + m;

                    B[idx] = BB[z] | (BB[poly_len + z] << 32);
                }
            }
        }
    }
    free(BB);
    std::cout << "\r Database load complete." << std::string(94, ' ') << std::flush;
    std::cout << std::endl;  // For carriage return.

    Log::cout << "Database N is " << total_n << "." << std::endl;
    Log::cout << "Encoded " << Process::Data::hashStore().size()
              << " hashes into " << loadedPolyCount << " polys and padded "
              << paddedPolynomialCount << " polynomials." << std::endl;

    if (hashesToLoadIterator != Process::Data::hashStore().get<Container::Sequence>().end()) {
        std::cerr << "Failed to load all hashes into the database. "
                  << "Is the database too small?" << std::endl;
    }

    if (has_file) {
        fs.close();
    }

    NativeLog::cout << "done loading/generating db." << endl;
}

void runServer();

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
        IDX_TARGET = 0;
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
    GlobalTimer::set("Running Server");
    runServer();
    GlobalTimer::stop("Running Server");
    #endif
}

// Halve the number of ciphertexts using a single query ciphertext for the 'further' dimensions.
/**
 * @brief Halves the number of ciphertexts using a single query ciphertext
 *        for the 'further' dimensions.
 * @param[in] cur_dim The current dimension the folding is taking place.
 * @param[in] num_per The number of elements to fold across.
 * @param[in] query_ct See: process_query_fast() param: further_dims_query_ct
 *       file: spiral.cpp for details.
 * @param[in] query_ct_neg See: process_query_fast() param: further_dims_query_ct_neg
 *       file: spiral.cpp for details.
 * @param[in, out] locals Working data container used to hold ciphertexts
 *       and related variables during query processing.
 * @returns The folded matrix Regev ciphertext stored in param: locals.
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

void answer_process_query_fast(
        const uint64_t *further_dims_query_ct,      // further dims query
        const uint64_t *further_dims_query_ct_neg,
        ExpansionLocals expansion_locals,           // must be cleared
        FurtherDimsLocals further_dims_locals       // must be cleared
) {
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
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
    GlobalTimer::set("Processing the first dimension using query-database multiplication");
    multiplyQueryByDatabase(
            further_dims_locals.scratch_cts1,
            expansion_locals.reoriented_ciphertexts,
            B,
            dim0,
            num_per
    );
    GlobalTimer::stop("Processing the first dimension using query-database multiplication");
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
    GlobalTimer::stop("Folding in subsequent dimensions");
    time_folding = end_timing();
    NativeLog::cout << "done folding" << endl;
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
            from_ntt(c, cv_v[i]);
            automorph(c_automorphed, c, t);
            pick(c_automorphed_0, c_automorphed, 0, 0);
            pick(c_automorphed_1, c_automorphed, 1, 0);
            to_ntt(c_automorphed_1_nttd, c_automorphed_1);
            place(c_automorphed_1_padded, c_automorphed_1_nttd, 1, 0);
            gadget_invert(gadget_dim, gi_c, c_automorphed_0, 1);
            to_ntt_no_reduce(gi_c_nttd, gi_c);
            multiply(W_times_ginv_c, W, gi_c_nttd);
            size_t idx = 0;
            for (size_t j = 0; j < n0; j++) {
                for (size_t n = 0; n < crt_count; n++) {
                    for (size_t z = 0; z < coeff_count; z++) {
                        cv_v[i].data[idx] = barrett_coeff(cv_v[i].data[idx] + W_times_ginv_c.data[idx] + j*c_automorphed_1_nttd.data[n * coeff_count + z], n);
                        idx++;
                    }
                }
            }
        }
    }
    return end_timing();
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
    pick(cv_0, cv, 0, 0);
    pick(cv_1, cv, 1, 0);
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
 * @param[in] cv_v_offset The offset into param: cv_v.
 * @param[in] W The key-switching matrix.
 * @param[in] V see: pg. 936, 2nd point.
 *
 * @details
 * t_GSW encodings consists of 2 \\dot t_GSW elements of Rq.
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
    }
    multiply(prod, V, ginv_Chat_nttd);
    place(result, prod, 0, 0);
    // MARK: Apply the permutation matrix?
    // NOTE: Need to double-check.
    //       The permutation matrix intends to rearrange the rows of
    //       the left-hand side matrix so that it becomes equal to G_n+1,z_GSW
    //       on the right-hand side.
    // @see pg. 936, 2nd dot point.
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

void sendToPipe(const FurtherDimsLocals& obj, const std::filesystem::path& filePath) {
    int pipeFileDescriptor {};
    mkfifo(filePath.c_str(), 0666);
    Log::cout << "Connecting to client on " << UnixColours::MAGENTA
              << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    pipeFileDescriptor = open(filePath.c_str(), O_WRONLY);
    if (pipeFileDescriptor < 0) {
        std::cerr << "Failed to open pipe: " << filePath << std::endl;
        return;
    }
    auto writeFDL = [&pipeFileDescriptor](const void* buf, const size_t n) {
        return write(pipeFileDescriptor, buf, n);
    };
   
    ssize_t status {};
    status = writeFDL(&obj.num_per, sizeof(obj.num_per));
    status = writeFDL(&obj.num_bytes_C, sizeof(obj.num_bytes_C));
    // Send uint64_t arrays.
    status = writeFDL(obj.result, 2 * obj.num_bytes_C);
    status = writeFDL(obj.cts, 2 * obj.num_bytes_C);
    status = writeFDL(obj.scratch_cts1, obj.num_bytes_C);
    status = writeFDL(obj.scratch_cts2, obj.num_bytes_C);
    status = writeFDL(obj.scratch_cts_double1, (m2 / n1) * obj.num_bytes_C);
    status = writeFDL(obj.scratch_cts_double2, (m2 / n1) * obj.num_bytes_C);
    if (status < 0) {
        std::cerr << "Failed to write to pipe: " << strerror(errno)
                  << " (generalised)." << std::endl;
    } else {
        Log::cout << "Sent FDL of " << obj.num_per << "x" << obj.num_bytes_C
                  << " bytes to client through the "<< UnixColours::MAGENTA
                  << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    }
    close(pipeFileDescriptor);
}

ssize_t readAllBytes(int fd, void* buffer, size_t n) {
    size_t totalBytes = 0;
    char* buf = static_cast<char*>(buffer);
    while (totalBytes < n) {
        ssize_t bytesRead = read(fd, buf + totalBytes, n - totalBytes);
        bool checkPipeStatus = bytesRead < 0;
        if (checkPipeStatus) {
            // Interrupted, try again.
            if (errno == EINTR) continue;
            // Other errors.
            return -1;
        }
        const bool isEOF = bytesRead == 0;
        if (isEOF) break;
        totalBytes += bytesRead;
    }
    return totalBytes;
}

size_t readFromPipe(
        int pipeFileDescriptor,
        std::initializer_list<std::reference_wrapper<MatPoly>> mats
) {
    auto readField = [&pipeFileDescriptor](void* buffer, const size_t size) {
        ssize_t status = readAllBytes(pipeFileDescriptor, buffer, size);
        if (status == 0) {
            throw std::out_of_range("Reached end of pipe.");
        } else if (status < 0) {
            return false;
        } return true;
    };
    auto readMatPoly = [&readField](MatPoly& mat) {
        bool isOK {};
        isOK = readField(&mat.rows, sizeof(mat.rows));
        isOK = readField(&mat.cols, sizeof(mat.cols));
        isOK = readField(&mat.isNTT, sizeof(mat.isNTT));
        size_t dataSize = mat.rows * mat.cols * (mat.isNTT ? crt_count : 1) * coeff_count;
        if (mat.data != nullptr) {
            free(mat.data);
        }
        mat.data = (uint64_t*) calloc(dataSize, sizeof(uint64_t));
        isOK = readField(mat.data, dataSize * sizeof(uint64_t));
        if (!isOK) {
            std::cerr << "Failed to read from pipe: " << strerror(errno) << " (generalised)." << std::endl;
            return 0;
        } else { return 1; }
    };
    int count = 0;
    for (auto&& matRef : mats) {
        count += readMatPoly(matRef.get());
    }
    return count;
}

void loadFromPipe(
        std::initializer_list<std::reference_wrapper<MatPoly>> matsToLoadInto,
        const std::filesystem::path& filePath
) {
    int pipeFileDescriptor {};
    mkfifo(filePath.c_str(), 0666);
    Log::cout << "Server ready on " << UnixColours::MAGENTA
              << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    pipeFileDescriptor = open(filePath.c_str(), O_RDONLY);
    if (pipeFileDescriptor < 0) {
        std::cerr << "Failed to open the pipe: " << filePath << std::endl;
        return;
    }
    size_t ok_retrievals = 0;
    try {
        ok_retrievals = readFromPipe(pipeFileDescriptor, matsToLoadInto);
    } catch (const std::out_of_range&) {
        // Ignore as we know the size of the mats we want to load into.
    }
    Log::cout << "Received " << ok_retrievals << " Matrices through the "
              << UnixColours::MAGENTA << filePath.filename() << UnixColours::RESET
              << " pipe." << std::endl;
    close(pipeFileDescriptor);
}

void loadFromPipe(
    std::vector<MatPoly>& keyArray,
    const std::filesystem::path& filePath,
    const bool clearTerminal = false
) {
    int pipeFileDescriptor {};
    mkfifo(filePath.c_str(), 0666);
    Log::cout << "Server ready on " << UnixColours::MAGENTA
              << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    pipeFileDescriptor = open(filePath.c_str(), O_RDONLY);
    if (pipeFileDescriptor < 0) {
        std::cerr << "Failed to open the pipe: " << filePath << std::endl;
        return;
    }
    size_t ok_retrievals = 0;
    do {
        MatPoly key;
        try {
            ok_retrievals += readFromPipe(pipeFileDescriptor, {key});
        } catch (const std::out_of_range&) {
            break;  // Reached end of pipe.
        }
        keyArray.push_back(key);
    } while (true);
    if (clearTerminal) system("clear");
    Log::cout << "Received " << ok_retrievals << " Matrices through the "
              << UnixColours::MAGENTA << filePath.filename() << UnixColours::RESET
              << " pipe." << std::endl;
    close(pipeFileDescriptor);
}

void answer_main(
    std::vector<MatPoly>& W_exp_right_v_Answer,
    std::vector<MatPoly>& W_exp_v_Answer,
    MatPoly& W_Answer,
    MatPoly& V_Answer
) {
    // Initialize server.
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
    // Load the query.
    GlobalTimer::set("Loading the query (q)");
    std::vector<MatPoly> round_cv_v_Answer;
    /** Attach networking here.
     * @section Answer
     * @step 3.1
     *
     * @note Load the encrypted query form the client into an std::vector of
     *       MatPoly objects.
     */
    loadFromPipe(round_cv_v_Answer, Process::workspace("query"), true);
    GlobalTimer::stop("Loading the query (q)");
    // Determine stop round.
    size_t qe_rest = query_elems_rest;
    size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
    if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions
    // Initial expansion.
    GlobalTimer::set("Performing initial expansion through the coefficient algo");
    (void)expandImproved(
        round_cv_v_Answer, g, m_exp,
        W_exp_v_Answer, W_exp_right_v_Answer,
        ell * further_dims, stopround
    );
    GlobalTimer::stop("Performing initial expansion through the coefficient algo");
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
    GlobalTimer::set("Performing first dimension query expansion");
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
    GlobalTimer::stop("Performing first dimension query expansion");
    // Run 'conversion'
    MatPoly c_ntti(n0, 1, false);
    MatPoly W_switched(n0, 1);
    MatPoly Y_switched(n0, 1);
    MatPoly C_v_0(n0, m);
    MatPoly C_v_1(n0, m);
    MatPoly C_v_0_ntti(n0, m, false);
    MatPoly C_v_1_ntti(n0, m, false);
    MatPoly ginv_Ct(m_conv, m, false);
    MatPoly ginv_Ct_ntt(m_conv, m);
    MatPoly prod0(n1, m);
    MatPoly prod1(n1, m);
    MatPoly Cp(n1, m);
    MatPoly Cp_raw(n1, m, false);
    // GSW ciphertext expansion.
    GlobalTimer::set("Performing GSW ciphertext expansion");
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
    GlobalTimer::stop("Performing GSW ciphertext expansion");
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
    GlobalTimer::set("Rescaling response using modulus switching");
    modswitch(furtherDimsLocals.result, furtherDimsLocals.cts);
    GlobalTimer::stop("Rescaling response using modulus switching");
    /** Attach networking here.
     * @section Answer
     * @step 3.2
     *
     * @note Send the server response to the client. Response is encoded within a
     *       FurtherDimsLocals object constructed with @param num_per.
     */
    sendToPipe(furtherDimsLocals, Process::workspace("response"));
}

void processExitStrategy(int signal) {
    std::cout << "Received signal " << signal
              << ". Exiting server process." << std::endl;
    free(B);
    exit(signal);
}

void runServer() {
    system("pwd");
    std::signal(SIGINT, processExitStrategy);
    std::signal(SIGTERM, processExitStrategy);
    Log::cout << "Database assigned to " << DATA_FILENAME << "." << std::endl;
    Process::Data::loadHashes(Process::dataSpace(DATA_FILENAME));
    GlobalTimer::set("Database Generation");
    load_db();
    GlobalTimer::stop("Database Generation");
    GlobalTimer::set("Load public parameters and conversion keys");
    std::vector<MatPoly> W_Exp_V, W_Exp_Right_V;
    /** Attach networking here.
     * @section Setup
     * @step 1.1
     *
     * @note Load the automorphism keys from the server into an std::vector
     *       of MatPoly objects.
     */
    loadFromPipe(W_Exp_Right_V, Process::workspace("automorphism_right"));
    loadFromPipe(W_Exp_V, Process::workspace("automorphism_left"));
    MatPoly W, V;
    /** Attach networking here.
     * @section Setup
     * @step 1.2
     *
     * @note Load W and V from the server into separate MatPoly objects.
     */
    loadFromPipe({W, V}, Process::workspace("conversion_keys"));
    GlobalTimer::stop("Load public parameters and conversion keys");
    while (true) {
        GlobalTimer::set("Fig.2: Answer");
        answer_main(W_Exp_Right_V, W_Exp_V, W, V);
        GlobalTimer::stop("Fig.2: Answer");
    }
}
