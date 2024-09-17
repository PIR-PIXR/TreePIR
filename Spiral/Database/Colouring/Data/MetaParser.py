import os
import json
import math


def calculate_x(value: int) -> int:
    return math.ceil(math.log2(value))


def count_objects_in_file(file_path: str) -> int:
    with open(file_path, 'r') as file:
        data = json.load(file)
        if isinstance(data, list) and data:
            return len(data[0])
        raise ValueError(f"{file_path} does not contain a list or is empty")


def get_json_files(directory: str) -> list[str]:
    return [file
            for file in os.listdir(directory)
            if file.endswith('.json')]


def process_files_and_write_meta() -> None:
    meta_data: list[tuple[int, str, int, int]] = list()
    for file_name in get_json_files('.'):
        try:
            file_path = os.path.join('.', file_name)
            file_size = os.path.getsize(file_path) / (1024 * 1024)
            object_count = count_objects_in_file(file_name)
            db_size = calculate_x(object_count)
            meta_data.append((file_size, file_name, object_count, db_size))
        except (
                json.JSONDecodeError,
                FileNotFoundError,
                PermissionError,
                ValueError
        ) as e:
            print(f"Error processing {file_name}: {e}")
    meta_data.sort(key=lambda item: item[2])

    with open('META', 'w') as meta_file:
        for file_size, file_name, count, db_size in meta_data:
            meta_file.write(
                f"[ {file_size:.2f}mb ] {file_name} has {count} hashes and requires a database size of 2^{db_size}.\n"
            )


def print_meta_file() -> None:
    with open('META', 'r') as meta_file:
        print(meta_file.read())


if __name__ == "__main__":
    process_files_and_write_meta()
    print_meta_file()
