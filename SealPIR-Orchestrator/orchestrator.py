import os
import sys
import math
import random
import shutil
import subprocess

def generate_random_TX_index(num_values, h, path):
    # Generate random TX indices
    random_TX_indicies = [random.randint(1, int(math.pow(2, h))) for _ in range(num_values)]
    # Save the values in the specified path CTA_PATH
    with open(path, "w") as file:
        for value in random_TX_indicies:
            file.write(str(value) + "\n")
    # Copy the file from source to destination
    shutil.copy(path, SUB_CSA_PATH)
    shutil.copy(path, PBC_PATH)
    print(f"{num_values} random TX indices saved in {path}")


# Run CSA to generate color databases and others based on Google's Xenon2024
def run_CSA(h):
    java_file_path = os.path.join(CSA_PATH, "src/CSA.java")
    gson_jar_path = os.path.join(CSA_PATH, "gson-2.10.1.jar")
    #db_path = os.path.join(CSA_PATH, "colorSubDB")
    classpath = f'{gson_jar_path}:{CSA_PATH + "/src/"}'
    main_class_name = 'CSA'

    # Define the javac command as a list of arguments
    os.chdir(CSA_PATH)
    subprocess.run(['javac', '-cp', classpath, java_file_path], check=True)  # Compile Java source code
    subprocess.run(['java', '-cp', classpath, main_class_name, str(h), CSA_PATH],
                   check=True)  # Run compiled Java class (for h < 30)
    # subprocess.run(['java', '-Xmx64g','-cp', classpath, main_class_name, str(h), db_path], check=True)  # Run compiled Java class (for h = 30)

    # Transfer log files from source to Logs path
    src_path = os.path.join(CSA_PATH, f"CSA_{h}_{2}_log.txt")
    dest_path = os.path.join(LOG_PATH, f"CSA_{h}_{2}_log.txt")
    copy_and_remove_file(src_path, dest_path)


# Run TreePIR-Indexing Algorithm to generate indices based on each Transaction index
def run_TreePIRIndexing(h):
    java_file_path = os.path.join(SUB_CSA_PATH, "src/SubCSA.java")
    main_class_name = 'SubCSA'

    # Define the javac command as a list of arguments
    os.chdir(SUB_CSA_PATH)
    subprocess.run(['javac', '-cp', '.', java_file_path], check=True)  # Compile Java source code
    subprocess.run(['java', '-cp', SUB_CSA_PATH + "/src/", main_class_name, str(h), SUB_CSA_PATH],
                   check=True)  # Run compiled Java class

    # Transfer log files from source to Logs path
    src_path = os.path.join(SUB_CSA_PATH, f"CSAindexing_{h}_{2}_log.txt")
    dest_path = os.path.join(LOG_PATH, f"CSAindexing_{h}_{2}_log.txt")
    copy_and_remove_file(src_path, dest_path)


def SealPIR_TreePIR(h):
    q = 2  # Binary tree
    color = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
             'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9']

    # Read Servers' IP
    server_ips = read_servers_ip()
    # Send color sub-databases to server
    for i in range(h):
        local_file_path = os.path.join(CSA_PATH, f"colorSubDB/color{color[i]}_{h}_{q}.json")
        copy_and_remove_file(local_file_path, SERVER_PATH)

    # Read indices
    color_indices_path = os.path.join(SUB_CSA_PATH, f"color_indices_{h}_{q}.txt")
    output_file_path = os.path.join(SEALPIR_PATH, "servers_list.txt")
    entries = read_indices_from_file(color_indices_path)
    # Generate servers_list.txt
    generate_servers_list(server_ips, entries, output_file_path)

    # Build SealPIR for client and server
    buildSealPIR()

    # Send pirmessage_server to server
    local_file_path = os.path.join(SEALPIR_PATH, 'pirmessage_server')
    copy_and_remove_file(local_file_path, SERVER_PATH)

    # Run SealPIR Server
    run_SealPIR_server()

    # Run SealPIR Client
    run_SealPIR_client()

    # Transfer logs from servers to local machine
    local_file_path = os.path.join(SERVER_PATH, 'server_log.txt')
    copy_and_remove_file(local_file_path, os.path.join(LOG_PATH, f"color_server_{h}_{q}_log.txt"))

    # Transfer log files from source to Logs path
    src_path = os.path.join(SEALPIR_PATH, "client_log.txt")
    dest_path = os.path.join(LOG_PATH, f"color_client_{h}_{q}_log.txt")
    copy_and_remove_file(src_path, dest_path)
    src_path = os.path.join(SUB_CSA_PATH, f"color_indices_{h}_{q}.txt")
    dest_path = os.path.join(LOG_PATH, f"color_indices_{h}_{q}.txt")
    copy_and_remove_file(src_path, dest_path)


# ---------------------------------------- PBC ---------------------------------------------
def run_PBC(h, q):
    # Change the working directory to the project directory
    os.chdir(PBC_PATH)
    cmake_configure_cmd = "cmake -S . -B build"
    subprocess.run(cmake_configure_cmd, shell=True, check=True)
    cmake_build_cmd = "cmake --build build"
    subprocess.run(cmake_build_cmd, shell=True, check=True)

    # Initialize PBC databases, map
    server_cmd = f"./build/bin/vectorized_batch_pir server {h} {q} {PBC_PATH}"
    subprocess.run(server_cmd, shell=True, check=True)

    # Initialize queries
    client_cmd = f"./build/bin/vectorized_batch_pir client {h} {q} {PBC_PATH}"
    subprocess.run(client_cmd, shell=True, check=True)


def SealPIR_PBC(h):
    q = 2  # Binary tree
    # Run the PBC
    print("Running PBC_db: Setup PBC databases...")
    run_PBC(h, q)

    # Read Servers' IP
    server_ips = read_servers_ip()
    # Send PBC sub-databases to server
    for i in range(math.ceil(1.5 * h)):
        local_file_path = os.path.join(PBC_PATH, f"PBC_data/PBC{i + 1}_{h}_{q}.json")
        copy_and_remove_file(local_file_path, SERVER_PATH)

    # Read indices
    pbc_indices_path = os.path.join(PBC_PATH, f"requests/pbc_indices_{h}_{q}.txt")
    output_file_path = os.path.join(SEALPIR_PATH, "servers_list.txt")
    entries = read_indices_from_file(pbc_indices_path)
    # Generate servers_list.txt
    generate_servers_list(server_ips, entries, output_file_path)

    # Build SealPIR for client and servers
    buildSealPIR()

    # Send pirmessage_server to server
    local_file_path = os.path.join(SEALPIR_PATH, 'pirmessage_server')
    copy_and_remove_file(local_file_path, SERVER_PATH)

    # Run SealPIR Server
    run_SealPIR_server()

    # Run SealPIR Client
    run_SealPIR_client()

    # Transfer logs from servers to local machine
    local_file_path = os.path.join(SERVER_PATH, 'server_log.txt')
    copy_and_remove_file(local_file_path, os.path.join(LOG_PATH, f"pbc_server_{h}_{q}_log.txt"))

    # Sent log files from the source to Logs path
    src_path = os.path.join(SEALPIR_PATH, "client_log.txt")
    dest_path = os.path.join(LOG_PATH, f"pbc_client_{h}_{q}_log.txt")
    copy_and_remove_file(src_path, dest_path)

    src_path = os.path.join(PBC_PATH, f"client_log/client_{h}_{q}.txt")
    dest_path = os.path.join(LOG_PATH, f"pbc_indexing_{h}_{q}_log.txt")
    copy_and_remove_file(src_path, dest_path)

    src_path = os.path.join(PBC_PATH, f"server_log/server_{h}_{q}.txt")
    dest_path = os.path.join(LOG_PATH, f"PBC_{h}_{q}_log.txt")
    copy_and_remove_file(src_path, dest_path)

    src_path = os.path.join(PBC_PATH, f"requests/pbc_indices_{h}_{q}.txt")
    dest_path = os.path.join(LOG_PATH, f"pbc_indices_{h}_{q}.txt")
    copy_and_remove_file(src_path, dest_path)


# ---------------------------------------- Util ---------------------------------------------
def generate_servers_list(server_ips, entries, output_file_path):
    with open(output_file_path, 'w') as file:
        for entry in entries:
            i = 0
            for file_entry in entry['file_entries']:
                server_ip = server_ips[i] if server_ips else "127.0.0.1"
                i += 1
                port_number = 3000  # You can replace this with your port number
                file_name = file_entry['FileName']
                index = file_entry['Index']
                # Write to the file
                file.write(f"{server_ip}:{port_number};{file_name};{index}\n")
    file.close()
    print(f"Servers_list successfully saved to {output_file_path}")

def read_servers_ip():
    file_path = os.path.join(ORCHESTRATOR_PATH, 'list_servers_IPs.txt')
    try:
        with open(file_path, 'r') as file:
            ip_addresses = file.read().split()
        return ip_addresses
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        return []

# Read indices from the file created by CSA
def read_indices_from_file(file_path):
    entries = []

    with open(file_path, 'r') as file:
        lines = file.readlines()

        entry = None
        for line in lines:
            if line.startswith('TX_index:'):
                if entry is not None:
                    entries.append(entry)

                # Create a new entry
                entry = {'TX_index': int(line.split(':')[1].strip()), 'file_entries': []}
            else:
                # Extract information from lines
                parts = line.split(';')
                if len(parts) == 3:
                    file_info = {
                        'FileName': parts[0].strip(),
                        'NodeID': int(parts[1].split(':')[1].strip()),
                        'Index': int(parts[2].split(':')[1].strip())
                    }
                    entry['file_entries'].append(file_info)

        # Add the last entry if it exists
        if entry is not None:
            entries.append(entry)

    return entries

def copy_and_remove_file(src_path, dest_path):
    try:
        # Copy the file from source to destination
        shutil.copy(src_path, dest_path)
        print(f"File '{src_path}' copied to '{dest_path}'.")
        # Remove the original file
        os.remove(src_path)
        print(f"Original file '{src_path}' removed.")
    except FileNotFoundError:
        print(f"Error: File '{src_path}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")


def buildSealPIR():
    os.chdir(SEALPIR_PATH)
    subprocess.run(["cmake", "."])
    subprocess.run(["make"])

def run_SealPIR_client():
    os.chdir(SEALPIR_PATH)
    # Run pirmessage_client in the current terminal
    subprocess.run(["./pirmessage_client"], check=True)
    print("pirmessage_client successfully executed")

def run_SealPIR_server():
    os.chdir(SERVER_PATH)
    # Run pirmessage_server in a new terminal
    command = ["gnome-terminal", "--", "./pirmessage_server", "-port", "3000"]
    subprocess.Popen(command)
    print("pirmessage_server successfully executed")


# ---------------------------------------- MAIN ---------------------------------------------
if __name__ == "__main__":
    if len(sys.argv) >= 3:
        h = int(sys.argv[1])
        PROJECT_PATH = sys.argv[2]  # "/home/quang/Desktop/TreePIR"
        SERVER_PATH = os.path.join(PROJECT_PATH, "SealPIRServer")
        CSA_PATH = os.path.join(PROJECT_PATH, "CSA")
        SUB_CSA_PATH = os.path.join(PROJECT_PATH, "TreePIR-Indexing")
        PBC_PATH = os.path.join(PROJECT_PATH, "PBC")
        LOG_PATH = os.path.join(PROJECT_PATH, "SealPIR-Logs")
        SEALPIR_PATH = os.path.join(PROJECT_PATH, "SealPIRplus")
        ORCHESTRATOR_PATH = os.path.join(PROJECT_PATH, "SealPIR-Orchestrator")

        # ----------- Step 1: Generate whole tree and sub-databases based on Color-Splitting Algorithm ------------------------
        print("Generating Merkle tree from xenon2024...")
        run_CSA(h)

        # Copy Whole Tree data
        src_path = os.path.join(CSA_PATH, f"colorSubDB/WholeTree_{h}_{2}.json")
        dest_path = os.path.join(PBC_PATH, f"treedata/WholeTree_{h}_{2}.JSON")
        shutil.copy(src_path, dest_path)

        # ----------- Step 2: Generate a random index ------------------------
        path = os.path.join(ORCHESTRATOR_PATH, f"list_TXs_{h}_{2}.txt")
        generate_random_TX_index(1, h, path)

        # ----------- Step 3: TreePIRIndexing ------------------------
        # Generate indices based on TreePIR-Indexing Algorithm
        run_TreePIRIndexing(h)

        # ----------- Step 4: Running SealPIR+TreePIR ------------------------
        SealPIR_TreePIR(h)

        # ----------- Step 5: Running SealPIR+PBC ------------------------
        SealPIR_PBC(h)


        print("Orchestration completed successfully.")
    else:
        print(
            "Insufficient command-line arguments. Usage: python3 orchestrator.py <parameter1: (h)> <parameter2: (path/to/Project)>")
