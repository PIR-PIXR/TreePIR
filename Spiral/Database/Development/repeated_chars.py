import json
import os
from prettytable import PrettyTable

def load_hashes_from_file(file_path):
    with open(file_path, 'r') as file:
        return json.load(file)

def find_repeated_chars(hash_value, threshold=16):
    char_counts = {char: hash_value.count(char) for char in set(hash_value)}
    for char, count in char_counts.items():
        if count > threshold:
            return char, count
    return None, None

def process_hashes(file_path):
    hashes = load_hashes_from_file(file_path)
    results, repeated_char_hash_count = [], 0
    total_hashes = len(hashes[0].items())

    for hash_dict in hashes:
        for key, hash_value in hash_dict.items():
            char, count = find_repeated_chars(hash_value)
            if char is not None:
                results.append((key, hash_value, char, count))
                repeated_char_hash_count += 1

    return results, total_hashes, repeated_char_hash_count

def display_results(files, total_hashes, total_repeated_char_hashes):
    table = PrettyTable(field_names=["File", "Hash ID", "Hash Value", "Character", "Count"])
    miss_rate_percent = ((total_hashes - total_repeated_char_hashes) / total_hashes) * 100 if total_hashes > 0 else 0

    for file, (results, file_hash_count) in files.items():
        for result in results:
            table.add_row([file, *result])

    print(table)
    print(f"Total Hashes Processed: {total_hashes}")
    print(f"Hashes with >16 Repeated Characters: {total_repeated_char_hashes}")
    print(f"Miss Rate (%): {miss_rate_percent:.2f}")

def main():
    total_hashes, total_repeated_char_hashes = 0, 0
    file_results = {}

    for file in os.listdir('.'):
        if file.endswith('.json'):
            results, file_hash_count, repeated_char_hash_count = process_hashes(file)
            total_hashes += file_hash_count
            total_repeated_char_hashes += repeated_char_hash_count
            file_results[file] = (results, file_hash_count)

    display_results(file_results, total_hashes, total_repeated_char_hashes)

if __name__ == "__main__":
    main()

