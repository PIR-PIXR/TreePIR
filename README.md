<p align="center">
  <img width="250" height="230" src="https://github.com/PIR-PIXR/Certificate-Transparency-Logs/assets/102839948/530caacf-868e-464c-995b-04e995bc02bc">
</p>

# TreePIR: Efficient Private Retrieval of Merkle Proofs via Tree Colorings with Fast Indexing and Zero Storage Overhead

## Abstract
A Batch Private Information Retrieval (batch-PIR) scheme allows a client to retrieve multiple data items from a database without revealing them to the storage server(s). Most existing approaches for batch-PIR are based on batch codes, in particular, probabilistic batch codes (PBC) ([Angel et al. IEEE S&P'18](https://doi.ieeecomputersociety.org/10.1109/SP.2018.00062)), which incur large storage overheads. In this work, we show that *zero* storage overhead is achievable for tree-shaped databases. In particular, we develop *TreePIR*, a novel approach tailored made for private retrieval of the set of nodes along an arbitrary *root-to-leaf path* in a Merkle tree with no storage redundancy. This type of trees has been widely implemented in many real-world systems such as Amazon DynamoDB, Google's Certificate Transparency, and blockchains. Tree nodes along a root-to-leaf path forms the well-known *Merkle proof*. TreePIR, which employs a novel tree coloring, outperforms PBC, a fundamental component in state-of-the-art batch-PIR schemes ([Angel et al. IEEE S&P'18](https://doi.ieeecomputersociety.org/10.1109/SP.2018.00062), [Mughees-Ren IEEE S&P'23](https://doi.ieeecomputersociety.org/10.1109/SP46215.2023.10179329), [Liu et al. IEEE S&P'24](https://doi.ieeecomputersociety.org/10.1109/SP54263.2024.00039)), in all metrics, achieving $3\times$ lower total storage and $1.5 - 3\times$ lower computation and communication costs. Most notably, TreePIR has $8 - 60\times$ lower setup time and its *polylog*-complexity indexing algorithm is $19 - 160\times$ faster than PBC for trees of $2^{10} - 2^{24}$ leaves. You can find the paper at [IEEE S&P 2025](https://doi.ieeecomputersociety.org/10.1109/SP61157.2025.00032) or Full version [here](https://github.com/PIR-PIXR/TreePIR/blob/main/TreePIR-Full.pdf).

---
## Experimental setup
We ran our experiments on a laptop (Intel® Core™ i9-13900H and 32GiB of system memory) in the Ubuntu 22.04 LTS environment. For each tree size, we ran the batch-PIR protocols for 10 random Merkle proofs and recorded the averages. 
To build Merkle trees of $2^{10} - 2^{20}$ leaves, we fetched $2^{20}$ entries from Google's [Xenon2024](https://github.com/PIR-PIXR/Certificate-Transparency-Logs), each of which comprises of an entry number, a timestamp, and a certificate, and applied SHA-256 on the certificates to produce tree leaves. Consequently, each tree node has size $32$ bytes. To evaluate TreePIR's performance for larger trees ($n = 2^{22},\ldots,2^{36})$, we use random hashes to avoid excessive hashing overheads. We did not consider PBC beyond $h = 24$ as its index became too large. We used the existing C++ implementations of [PBC](https://github.com/newPIR/TreePIR/tree/main/PBC) by Mughees-Ren (after fixing some minor errors), [Spiral](https://github.com/menonsamir/spiral), [VBPIR](https://github.com/mhmughees/vectorized_batchpir), and [PIRANA](https://github.com/zju-abclab/PIRANA).

---
### Dataset
Download the dataset from [Google Drive](https://drive.google.com/file/d/1VzdnIZK5nXA-kihbjMM_KNTrlwgzWJYs/view?usp=sharing) and overwrite the downloaded file to CSA/xenon2024/xenon2024_log_entries. The new file has a size of around 2GB.

You can also download Certificate Transparency logs from our [Github](https://github.com/PIR-PIXR/Certificate-Transparency-Logs).

---
### Installing Libraries

- #### Javac
      $ sudo apt update
      $ sudo apt upgrade
      $ sudo apt install default-jdk
- #### Python3
      $ sudo apt update
      $ sudo apt upgrade
      $ sudo apt install python3 
- #### SEAL 4.0.0
      $ sudo apt install build-essential cmake clang git g++ libssl-dev libgmp3-dev
      $ sudo apt update
      $ sudo apt upgrade
      $ git clone https://github.com/cnquang/SEAL-4.0.0.git
      $ cd SEAL-4.0.0
      $ cmake -S . -B build
      $ cmake --build build
      $ sudo cmake --install build
- #### JSON
      $ git clone https://github.com/microsoft/vcpkg
      $ ./vcpkg/bootstrap-vcpkg.sh
      $ ./vcpkg install rapidjson
- #### Google gRPC
      $ sudo apt install -y build-essential autoconf libtool pkg-config
      $ git clone --recurse-submodules -b v1.58.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
      $ cd grpc
      $ mkdir -p cmake/build
      $ pushd cmake/build
      $ cmake -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        ../..
      $ make -j 4
      $ sudo make install
      $ popd

---
### Executing SealPIR+PBC and SealPIR+TreePIR
      $ git clone https://github.com/newPIR/TreePIR.git
      $ cd TreePIR/SealPIR-Orchestrator
      $ python3 orchestrator.py <tree height> /Path/To/TreePIR
For example, with tree height $h = 10$, 
      python3 orchestrator.py 10 /Path/To/TreePIR.

---
### Executing Spiral+PBC and Spiral+TreePIR

- ##### Docker
      $ cd TreePIR/Spiral
      $ sudo docker build -t spiral_toolchain .
      $ sudo apt-get install libstdc++-11-dev
      $ sudo docker run -it \
        -u root \
        -v /Path/To/TreePIR/Spiral:/tmp/Spiral \
        -v /Path/To/TreePIR/Spiral/Process_Workspace:/home/ubuntu/Process_Workspace \
        --rm spiral_toolchain:latest \
        /bin/bash -c "cd /tmp/Spiral; exec bash"
      $ cd Evaluation
      $ python3 evaluate.py

To execute Spiral+PBC, open the evaluation.py and active line 23: "mode: typing.Final[Mode] = Mode.PBC". In the file TreePIR/Spiral/Seperate/src/spiral.cpp, active line 1140.

To execute Spiral+TreePIR, open the evaluation.py and active line 22: "mode: typing.Final[Mode] = Mode.COLOURING". In the file TreePIR/Spiral/Seperate/src/spiral.cpp, active line 1139.

---
### Executing VBPIR+PBC
      $ cd TreePIR/VBPIR_PBC
      $ cmake -S . -B build
      $ cmake --build build
      $ ./build/bin/vbpir_pbc
      
---
### Executing VBPIR+TreePIR
      $ cd TreePIR/VBPIR_TreePIR
      $ cmake -S . -B build
      $ cmake --build build
      $ ./build/bin/vbpir_treepir

---
## Performance
The performance of a batch-code-based batch-PIR depends on the number of sub-databases and their sizes, and on the performance of the underlying PIR scheme. Moreover, it also depends on the dimension $d$ that PIR sub-databases are represented, e.g., $d=1$ for PIRANA, $d=2$ for SealPIR, $d=2, 3$ for VBPIR, and $d=4$ for Spiral. Theoretically, as TreePIR uses $1.5\times$ fewer sub-databases of size $2\times$ smaller compared to PBC, theoretically, TreePIR uses $3\times$ less storage, with $\sqrt[d]{2}\times$ faster max server computation, $1.5\sqrt[d]{2}\times$ faster total server computation, and $1.5 \times$ faster client query-generation time (except for VBPIR and PIRANA, the speedup for client query-generation time is $\sqrt[d]{2}\times$ and $\sqrt[k]{3}\times$, respectively). The client answer-extraction times are similar for both as dummy responses are ignored in PBC. These theoretical gains were also reflected in the experiments.

TreePIR has significantly faster setup and indexing thanks to its efficient coloring and indexing algorithms on trees. In particular, for trees with $2^{10} - 2^{24}$ leaves, TreePIR's setup and indexing are $8$-$60\times$ and $19 - 160\times$ faster than PBC's, respectively. TreePIR still works well beyond that range, requiring $180$ seconds to setup a tree of $2^{30}$ leaves, and only $0.7$ milliseconds to index in a tree of $2^{36}$ leaves.

**Table 1:** A comparison of the *setup* time between TreePIR and PBC for various tree heights. TreePIR's setup is $8 - 60\times$ faster for trees of $2^{10} - 2^{24}$ leaves.

| $h$ | 10 | 12 | 14 | 16 | 18 | 20 | 22 | 24 |
|-----|----|----|----|----|----|----|----|----|
| PBC (ms) | 3.4 | 8.9 | 53.1 | 406 | 2132 | 9259 | 37591 | 159814 |
| **TreePIR** (ms) | 0.4 | 2.4 | 7.6 | 39.1 | 56.1 | 179 | 600 | 2700 |
| $h$ | 26 |    |    | 28 |    | 29 |    | 30 |
| **TreePIR** (sec) | 9.6 |    |    | 37.3 |    | 77.1 |    | 179.4 |

**Table 2:** A comparison of the *indexing* times of TreePIR and PBC. Despite ignoring the download time of its (large) index, PBC's indexing is still 19 - $160\times$ slower than TreePIR's indexing.

| $h$ | 10 | 12 | 14 | 16 | 18 | 20 | 22 | 24 |
|-----|----|----|----|----|----|----|----|----|
| Indexing PBC (ms) | 4 | 4 | 4 | 4 | 5 | 11 | 22 | 74 |
| **TreePIR** (ms) | 0.21 | 0.24 | 0.25 | 0.32 | 0.34 | 0.38 | 0.41 | 0.46 |
| $h$ | 26 | 28 |  | 30 | 32 | | 34 | 36 |
| **TreePIR** (ms) | 0.47 | 0.48 |  | 0.51 | 0.52 | | 0.61 | 0.69 |

**Table 3:** The client *query-generation* times of TreePIR and PBC when combined with SealPIR, Spiral, VBPIR, and PIRANA.

| $h$ | 10 | 12 | 14 | 16 | 18 | 20 |
|-----|----|----|----|----|----|----|
| SealPIR+PBC (ms) | 21 | 24 | 28 | 31 | 37 | 40 |
| **SealPIR+TreePIR** (ms) | 13 | 16 | 18 | 21 | 24 | 27 |
| Spiral+PBC (ms) | 33 | 39 | 46 | 54 | 62 | 66 |
| **Spiral+TreePIR** (ms) | 20 | 24 | 29 | 33 | 37 | 41 |
| VBPIR+PBC (ms) | 5.1 | 5.1 | 7.0 | 7.0 | 7.3 | 7.5 |
| **VBPIR+TreePIR** (ms) | 4.1 | 4.1 | 4.3 | 6.2 | 6.4 | 6.6 |
| PIRANA+PBC (ms) | 4.1 | 5.6 | 8.6 | 14 | 24 | 45 |
| **PIRANA+TreePIR** (ms) | 3.3 | 3.9 | 5.6 | 8 | 15 | 28 |

**Table 4:** The client *answer-extraction* times of TreePIR and PBC when combined with SealPIR, Spiral, VBPIR, and PIRANA are similar.

| $h$ | 10  | 12  | 14  | 16  | 18  | 20  |
|-----|-----|-----|-----|-----|-----|-----|
| SealPIR+PBC (ms) | 12.4 | 14.9 | 17.3 | 19.4 | 22.4 | 24.6 |
| **SealPIR+TreePIR** (ms) | 12.6 | 15.0 | 17.2 | 19.4 | 22.2 | 24.3 |
| Spiral+PBC (ms) | 6.7 | 7.9 | 9.4 | 10.4 | 10.9 | 12.2 |
| **Spiral+TreePIR** (ms) | 5.8 | 7.2 | 8.4 | 9.6 | 10.5 | 12.2 |
| VBPIR+PBC (ms) | 1.3 | 1.3 | 0.7 | 0.7 | 0.7 | 0.7 |
| **VBPIR+TreePIR** (ms) | 1.3 | 1.3 | 1.3 | 0.7 | 0.7 | 0.7 |
| PIRANA+PBC (ms) | 6.1 | 6.1 | 6.1 | 6.1 | 6.1 | 6.1 |
| **PIRANA+TreePIR** (ms) | 6.1 | 6.1 | 6.1 | 6.1 | 6.1 | 6.1 |

**Table 5:** Theoretically, TreePIR's max server computation time is $\sqrt[d]{2} \times$ faster than PBC. This is reflected correctly in the table with $d = 2$ for SealPIR and $d=4$ for Spiral.

| $h$ | 10 | 12 | 14 | 16 | 18 | 20 |
|-----|----|----|----|----|----|----|
| SealPIR+PBC (ms) | 6.0 | 6.2 | 12.4 | 21.3 | 60 | 107 |
| **SealPIR+TreePIR** (ms) | 5.8 | 5.8 | 9.1 | 18.9 | 36 | 76 |
| Spriral+PBC (ms) | 33 | 34 | 34 | 34 | 35 | 39 |
| **Spiral+TreePIR** (ms) | 30 | 31 | 31 | 31 | 32 | 33 |

**Table 6:** TreePIR's total server computation time is $1.5$ - $3\times$ faster than PBC for larger trees. Theoretically, it is $1.5 \sqrt[d]{2} \times$ faster.

| $h$ | 10 | 12 | 14 | 16 | 18 | 20 |
|-----|----|----|----|----|----|----|
| SealPIR+PBC (ms) | 69 | 99 | 235 | 486 | 1185 | 2916 |
| **SealPIR+TreePIR** (ms) | 47 | 55 | 104 | 233 | 531 | 1250 |
| Spiral+PBC (ms) | 501 | 613 | 700 | 805 | 916 | 1151 |
| **Spiral+TreePIR** (ms) | 309 | 373 | 429 | 507 | 561 | 663 |
| VBPIR+PBC (ms) | 399 | 405 | 586 | 969 | 2357 | 7481 |
| **VBPIR+TreePIR** (ms) | 396 | 397 | 415 | 588 | 1372 | 3871 |
| PIRANA+PBC (ms) | 46 | 76 | 189 | 570 | 1851 | 6796 |
| **PIRANA+TreePIR** (ms) | 26 | 42 | 91 | 218 | 696 | 2377 |


<p align="center">
  <img width="600" height="350" src="https://github.com/user-attachments/assets/9dec3668-cba6-4cb2-a22b-ed057541f34f">
</p>

**Figure 1:** TreePIR's communication cost is about $1.5\times$ lower than PBC's as expected for most combinations (except VBPIR).

---
## ACKNOWLEDGMENTS 
This work was supported by the Australian Research Council through the Discovery Project under Grant DP200100731.

---
## REFERENCES

[[SealPIR+PBC](https://doi.ieeecomputersociety.org/10.1109/SP.2018.00062)] Angel, S., Chen, H., Laine, K., & Setty, S. (2018, May). PIR with compressed queries and amortized query processing. In 2018 IEEE symposium on security and privacy (SP) (pp. 962-979). [GitHub](https://github.com/microsoft/SealPIR).

[[Spiral](https://doi.ieeecomputersociety.org/10.1109/SP46214.2022.9833700)] Menon, S. J., & Wu, D. J. (2022, May). Spiral: Fast, high-rate single-server PIR via FHE composition. In 2022 IEEE Symposium on Security and Privacy (SP) (pp. 930-947). [GitHub](https://github.com/menonsamir/spiral).

[[VBPIR](https://doi.ieeecomputersociety.org/10.1109/SP46215.2023.10179329)] Mughees, M. H., & Ren, L. (2023, May). Vectorized batch private information retrieval. In 2023 IEEE Symposium on Security and Privacy (SP) (pp. 437-452). [GitHub](https://github.com/mhmughees/vectorized_batchpir).

[[PIRANA](https://doi.ieeecomputersociety.org/10.1109/SP54263.2024.00039)] J. Liu, J. Li, D. Wu and K. Ren, (2024), PIRANA: Faster Multi-query PIR via Constant-weight Codes, in 2024 IEEE Symposium on Security and Privacy (SP), (pp. 43-43). [GitHub](https://github.com/zju-abclab/PIRANA).
