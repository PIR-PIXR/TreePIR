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

auto start_stage = chrono::high_resolution_clock::now();
auto start = chrono::high_resolution_clock::now();
size_t stage = 0;
void start_timing() {
    stage = 0;
    start_stage = chrono::high_resolution_clock::now();
    start = chrono::high_resolution_clock::now();
}
void record(const string &s) {
    // Note: This has been disabled and left in for documentation.
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

void runClient();

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
                std::string hash = element.value().get<std::string>();
                std::transform(
                        hash.begin(), hash.end(), hash.begin(),
                        [](unsigned char c){ return std::tolower(c); }
                );
                hashStoreRef.get<Container::Sequence>().push_back(hash);
            }
        }

        void loadHashes(const std::filesystem::path& jsonFile, const bool insertionOrderLoad = true) {
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

std::pair<uint8_t, uint8_t> unpackBit(const uint64_t packed) {
    return std::make_pair((packed >> 4) & 0xF, packed & 0xF);
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
        DATA_FILENAME = argv[3];
        total_n = (1 << num_expansions) * (1 << further_dims);
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

    GlobalTimer::set("Running Client");
    runClient();
    GlobalTimer::stop("Running Client");
    #endif
}

// _________________________________________________________________________________

void sendToPipe(const std::vector<MatPoly>& keyArray, const std::filesystem::path& filePath) {
    int pipeFileDescriptor {};
    mkfifo(filePath.c_str(), 0666);
    Log::cout << "Connecting to server on " << UnixColours::MAGENTA
              << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    pipeFileDescriptor = open(filePath.c_str(), O_WRONLY);
    if (pipeFileDescriptor < 0) {
        std::cerr << "Failed to open pipe: " << filePath << std::endl;
        return;
    }
    auto writeMatPoly = [&pipeFileDescriptor](const void* buf, const size_t n) {
        return write(pipeFileDescriptor, buf, n);
    };
    auto sendMatPoly = [&writeMatPoly](const MatPoly &mat) {
        ssize_t status {};
        status = writeMatPoly(&mat.rows, sizeof(mat.rows));
        status = writeMatPoly(&mat.cols, sizeof(mat.cols));
        status = writeMatPoly(&mat.isNTT, sizeof(mat.isNTT));
        size_t dataSize = mat.rows * mat.cols * (mat.isNTT ? crt_count : 1) * coeff_count;
        status = writeMatPoly(mat.data, dataSize * sizeof(uint64_t));
        if (status < 0) {
            std::cerr << "Failed to write to pipe: " << strerror(errno)
                      << " (generalised)." << std::endl;
            return 0;
        } else { return 1; }
    };
    int count = 0;
    for (const auto & mat: keyArray) {
        count += sendMatPoly(mat);
    }
    Log::cout << "Sent " << count << "/" << keyArray.size() << " Matrices through the "
             << UnixColours::MAGENTA << filePath.filename() << UnixColours::RESET
              << " pipe." << std::endl;
    close(pipeFileDescriptor);
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

void loadFromPipe(FurtherDimsLocals& obj, const std::filesystem::path& filePath) {
    int pipeFileDescriptor {};
    mkfifo(filePath.c_str(), 0666);
    Log::cout << "Connecting to server on " << UnixColours::MAGENTA
              << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    pipeFileDescriptor = open(filePath.c_str(), O_RDONLY);
    if (pipeFileDescriptor < 0) {
        std::cerr << "Failed to open the pipe: " << filePath << std::endl;
        return;
    }
    ssize_t status {};
    status = readAllBytes(pipeFileDescriptor, &obj.num_per, sizeof(obj.num_per));
    status = readAllBytes(pipeFileDescriptor, &obj.num_bytes_C, sizeof(obj.num_bytes_C));
    obj.allocate();
    status = readAllBytes(pipeFileDescriptor, obj.result, 2 * obj.num_bytes_C);
    status = readAllBytes(pipeFileDescriptor, obj.cts, 2 * obj.num_bytes_C);
    status = readAllBytes(pipeFileDescriptor, obj.scratch_cts1, obj.num_bytes_C);
    status = readAllBytes(pipeFileDescriptor, obj.scratch_cts2, obj.num_bytes_C);
    status = readAllBytes(pipeFileDescriptor, obj.scratch_cts_double1, (m2 / n1) * obj.num_bytes_C);
    status = readAllBytes(pipeFileDescriptor, obj.scratch_cts_double2, (m2 / n1) * obj.num_bytes_C);
    if (status < 0) {
        std::cerr << "Failed to read from pipe: " << strerror(errno)
                  << " (generalised)." << std::endl;
    } else {
        Log::cout << "Received FDL of " << obj.num_per << "x" << obj.num_bytes_C
                  << " bytes from server through the "<< UnixColours::MAGENTA
                  << filePath.filename() << UnixColours::RESET << " pipe." << std::endl;
    }
    close(pipeFileDescriptor);
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


void setup_main(MatPoly& S_Setup, MatPoly& Sp_Setup, MatPoly& sr_Setup) {
    // Create querying keys.
    GlobalTimer::set("Creating query keys (S, s)");
    S_Setup = MatPoly(n0, n1, false);
    Sp_Setup = MatPoly(n0, k_param, false);
    sr_Setup = MatPoly(1, 1, false);
    keygen(S_Setup, Sp_Setup, sr_Setup);
    GlobalTimer::stop("Creating query keys (S, s)");
    // Create automorphism keys.
    GlobalTimer::set("Creating automorphism keys (W_i)");
    size_t num_expanded = 1 << num_expansions;
    size_t m = m2;
    size_t ell = m / n1;
    size_t num_bits_to_gen = ell * further_dims + num_expanded;
    auto g = (size_t) ceil(log2((double)( num_bits_to_gen )));
    // Determine stop round.
    size_t qe_rest = query_elems_rest;
    size_t stopround = qe_rest == 0 ? ((size_t) ceil(log2((double)( ell * further_dims )))) : 0;
    if (ell * further_dims > num_expanded) stopround = 0; // don't use this trick for these weird dimensions
    // Generate automorphism keys.
    std::vector<MatPoly> W_exp_right_v, W_exp_v;
    setup_GetPublicEncryptions(
        g, sr_Setup, W_exp_right_v,
        m_exp_right, stopround > 0 ? stopround+1 : 0
    );
    setup_GetPublicEncryptions(
        g, sr_Setup, W_exp_v, m_exp
    );
    GlobalTimer::stop("Creating automorphism keys (W_i)");
    /** Attach networking here.
     * @section Setup
     * @step 1.1
     *
     * @note Send the automorphism keys to the server. Automorphism keys are
     *       a std::vector of MatPoly objects.
     */
    sendToPipe(W_exp_right_v, Process::workspace("automorphism_right"));
    sendToPipe(W_exp_v, Process::workspace("automorphism_left"));
    // Create conversion key W.
    GlobalTimer::set("Creating conversion keys (W, V)");
    size_t m_conv_n0 = n0 * m_conv;
    MatPoly G_scale = buildGadget(n0, m_conv_n0);
    MatPoly s0 = sr_Setup;
    MatPoly s0G = mul_by_const(to_ntt(s0), to_ntt(G_scale));
    MatPoly s0G_padded(n1, m_conv_n0);
    place(s0G_padded, s0G, 1, 0);
    MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_Setup, m_conv_n0));
    MatPoly W(n1, m_conv_n0);
    add(W, P, s0G_padded);
    // Create conversion key V.
    size_t m_conv_2 = m_conv * 2;
    MatPoly V(n1, m_conv_2);
    MatPoly Sp_Setup_NTT = to_ntt(Sp_Setup);
    start_timing();
    {
        MatPoly gv = to_ntt(buildGadget(1, m_conv));
        // NOTE: Sp_mp is s_gsw.
        MatPoly P = to_ntt(get_fresh_public_key_raw(Sp_Setup, m_conv_2));
        // MatPoly P(n1, m_conv_2);
        // NOTE: s0 = sr_mp = s_regev. s0 * g_z_conv.
        MatPoly scaled_gv = mul_by_const(to_ntt(s0), gv);
        MatPoly together(1, m_conv_2);
        place(together, scaled_gv, 0, 0);
        place(together, gv, 0, m_conv);
        // NOTE: -s_gsw * (s0 * g_z_conv)
        MatPoly result = multiply(Sp_Setup_NTT, together);
        MatPoly result_padded(n1, m_conv_2);
        place(result_padded, result, 1, 0);
        // NOTE: s_gsw_public + [s_gsw * (s0 * g_z_conv)]
        add(V, P, result_padded);
    }
    GlobalTimer::stop("Creating conversion keys (W, V)");
    Log::cout  << "Note the permutation matrix (Π) is hard coded in the server." << std::endl;
    /** Attach networking here.
     * @section Setup
     * @step 1.2
     *
     * @note Send the conversion keys (W, V) to the server. W and V are both
     *       MatPoly objects.
     */
    sendToPipe({W, V}, Process::workspace("conversion_keys"));
}

void query_main(
    const MatPoly& S_Query, const MatPoly& Sp_Query, const MatPoly& sr_Query
) {
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
        GlobalTimer::set("Encoding the first and subsequent dimensions of the query");
        sigma.data[2*idx_dim0] = (scal_const * (__uint128_t)init_val) % Q_i;
        for (size_t i = 0; i < further_dims; i++) {
            uint64_t bit = (idx_further & (1 << i)) >> i;
            for (size_t j = 0; j < ell; j++) {
                size_t idx = i * ell + j;
                uint64_t val = (1UL << (bits_per * j)) * bit;
                sigma.data[2*idx+1] = (scal_const * (__uint128_t)val) % Q_i;
            }
        }
        GlobalTimer::stop("Encoding the first and subsequent dimensions of the query");
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
    // Pack the query.
    GlobalTimer::set("Performing query packing to generate a packed polynomial");
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
    GlobalTimer::stop("Performing query packing to generate a packed polynomial");
    // Query encryption.
    GlobalTimer::set("c <- Encrypting the query");
    MatPoly cv = query_encryptSimpleRegev(sr_Query, sigma);
    std::vector<MatPoly> round_cv_v;
    round_cv_v.push_back(cv);
    for (size_t i = 0; i < (1<<g) - 1; i++) {
        round_cv_v.emplace_back(n0, 1);
    }
    GlobalTimer::stop("c <- Encrypting the query");
    /** Attach networking here.
     * @section Query
     * @step 2.1
     *
     * @note Send the encrypted query to the server. The query is a std::vector
     *       of MatPoly objects.
     */
    sendToPipe(round_cv_v, Process::workspace("query"));
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
    GlobalTimer::set("Retrieving the server response (r)");
    size_t dim0 = 1 << num_expansions;
    size_t num_per = total_n / dim0;
    FurtherDimsLocals furtherDimsLocals(num_per);
    /** Attach networking here.
     * @section Extract
     * @step 4.1
     *
     * @note Receive the server response from the server. The server response
     *       is a FurtherDimsLocals object constructed with @param `num_per`.
     *
     *       This is the final step :).
     */
    loadFromPipe(furtherDimsLocals, Process::workspace("response"));
    GlobalTimer::stop("Retrieving the server response (r)");
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
    GlobalTimer::set("Rescaling the final encoding");
    MatPoly first_row = pick(ct_inp, 0, 0, 1, ct_inp.cols);
    MatPoly first_row_sw = getRescaled(first_row, Q_i, arb_qprime);
    MatPoly rest_rows = pick(ct_inp, 1, 0, ct_inp.rows - 1, ct_inp.cols);
    MatPoly rest_rows_sw = getRescaled(rest_rows, Q_i, q_1);
    MatPoly total_resp(n1, n2, false);
    place(total_resp, first_row_sw, 0, 0);
    place(total_resp, rest_rows_sw, 1, 0);
    GlobalTimer::stop("Rescaling the final encoding");
    // Recover takes in rescaled encoding and the secret key and outputs Z.
    GlobalTimer::set("Performing Z <- Recover_q1_q2(S, r) operation");
    MatPoly first_row_decoded = pick(total_resp, 0, 0, 1, total_resp.cols);
    MatPoly rest_rows_decoded = pick(total_resp, 1, 0, total_resp.rows - 1, total_resp.cols);
    to_ntt_qprime(first_row_decoded);
    MatPoly s_prod = mul_over_qprime(Sp_mp_nttd_qprime, first_row_decoded);
    from_ntt_qprime(s_prod);  // Z.
    GlobalTimer::stop("Performing Z <- Recover_q1_q2(S, r) operation");
    // Decode the response to get the encoded message.
    GlobalTimer::set("Performing C <- Decode(Z) operation to get the encoded message");
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
    GlobalTimer::stop("Performing C <- Decode(Z) operation to get the encoded message");
    decodePoly(M_result, queryIndex);
}

void refreshWorkspaceDirectory() {
    if (!std::filesystem::is_empty(Process::processPath)) {
        Log::cout << "Clearing process workspace." << std::endl;
        std::string command = "rm -rf " + Process::processPath.string() + "/*";
        system(command.c_str());
    }
}

size_t retrieveRecordIndex(const size_t queryIndex) {
    const PlaintextConversionConfig config(2);
    const size_t databaseRecordCount = total_n;
    const size_t hashesPerRecord = config.hashesPerPoly;
    const size_t recordIndex = queryIndex / hashesPerRecord;
    assert(recordIndex < databaseRecordCount);
    return recordIndex;
}

void clientExitStrategy(int signal) {
    std::cout << "Received signal " << signal
              << ". Exiting client process." << std::endl;
    exit(signal);
}

void runClient() {
    system("pwd");
    std::signal(SIGINT, clientExitStrategy);
    std::signal(SIGTERM, clientExitStrategy);
    Log::cout << "Using " << DATA_FILENAME << " for verification." << std::endl;
    Process::Data::loadHashes(Process::dataSpace(DATA_FILENAME));
    refreshWorkspaceDirectory();
    MatPoly S_Main, Sp_Main, sr_Query;
    GlobalTimer::set("Fig.2: Setup");
    setup_main(S_Main, Sp_Main, sr_Query);
    GlobalTimer::stop("Fig.2: Setup");
    size_t queryIndex {};
    while (true) {
        std::cout << "[" << UnixColours::CYAN << "Input"
                  << UnixColours::RESET << "] "
                  << "Enter query index: " << std::flush;
        std::cin >> queryIndex;
        const size_t loadedHashCount = Process::Data::hashStore().get<Container::Sequence>().size();
        if (queryIndex < 0 or queryIndex >= loadedHashCount) {
            std::cerr << "Query index is out of bounds." << std::endl;
            continue;
        }
        system("clear");
        Log::cout << "Retrieving hash for index " << UnixColours::MAGENTA
                  << queryIndex << UnixColours::RESET
                  << " from the database." << std::endl;
        IDX_TARGET = retrieveRecordIndex(queryIndex);
        IDX_DIM0 = IDX_TARGET / (1 << further_dims);
        GlobalTimer::set("Fig.2: Query");
        query_main(S_Main, Sp_Main, sr_Query);
        GlobalTimer::stop("Fig.2: Query");
        GlobalTimer::set("Fig.2: Extract");
        extract_main(S_Main, Sp_Main, queryIndex);
        GlobalTimer::stop("Fig.2: Extract");
    }
}
