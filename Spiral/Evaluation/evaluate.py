from __future__ import annotations

import os
import abc
import sys
import math
import enum
import json
import typing
import statistics
import dataclasses
import matplotlib.font_manager
import pandas as pd
import matplotlib.pyplot as plt


class Mode(str, enum.Enum):
    PBC = "PBC"
    COLOURING = "Colouring"


mode: typing.Final[Mode] = Mode.COLOURING
#mode: typing.Final[Mode] = Mode.PBC


@dataclasses.dataclass
class CMakeConfiguration:
    build_configuration: str
    build_type: typing.Literal["Debug", "Release"]
    toolchain_file: typing.Final[os.PathLike] \
        = "/home/ubuntu/vcpkg/scripts/buildsystems/vcpkg.cmake"
    build_directory_base_name: typing.Final[str] = "build"
    source_directory: typing.Final[os.PathLike] = "/tmp/Spiral"
    use_log: typing.Literal["ON", "OFF"] = "OFF"
    use_timer_log: typing.Literal["ON", "OFF"] = "OFF"
    use_native_log: typing.Literal["ON", "OFF"] = "OFF"

    @property
    def build_path(self) -> str:
        return os.fspath(
            f"{self.source_directory}/{self.build_directory_base_name}_{self.build_configuration}"
        )

    def __str__(self) -> str:
        return (
            f"cmake "
            f"-DCMAKE_BUILD_TYPE={self.build_type} "
            f"-DCMAKE_TOOLCHAIN_FILE={self.toolchain_file} "
            f"-DUSE_TIMERLOG={self.use_timer_log} "
            f"-DUSE_NATIVELOG={self.use_native_log} "
            f"-DUSE_LOG={self.use_log} "
            f"-S {self.source_directory} "
            f"-B {self.build_path}"
        )


class SpiralTarget(str, enum.Enum):
    SEPERATED = "Seperated"
    SERVER = "Server"
    CLIENT = "Client"


@dataclasses.dataclass
class CMakeBuild:
    parameter_set: str
    build_path: str
    target: SpiralTarget
    core_count: int = 6
    verbose: bool = True

    def __str__(self) -> str:
        return (
                f"cmake "
                f"--build {self.build_path} "
                f"--target {self.target} "
                + (f"-v " if self.verbose else "") +
                f"-j{self.core_count} "
                f"-- PARAMSET=PARAMS_DYNAMIC {self.parameter_set}"
        )


@dataclasses.dataclass
class CMakeRun:
    cmake_configuration: CMakeConfiguration
    cmake_build: CMakeBuild
    executable: SpiralTarget
    database_file: str
    further_dimensions: int
    folding_factor: int
    perform_build: bool = True

    @property
    def build_path(self) -> str:
        return self.cmake_build.build_path

    def executable_path(self) -> str:
        target_project_map = {
            "Seperated": "Seperated",
            "Server": "PIR_Server",
            "Client": "Client"
        }
        return os.fspath(
            f"{self.build_path}/{target_project_map[self.executable]}/{self.executable}"
        )

    def generate_command(self) -> str:
        executable_command: str = (
            f"{self.executable_path()} "
            f"{self.further_dimensions} {self.folding_factor} "
            f"{self.database_file}"
        )
        return (
            f"{self.cmake_configuration} && "
            f"{self.cmake_build} && "
            f"{executable_command}"
        ) if self.perform_build else executable_command

    def run(self) -> int:
        return os.system(self.generate_command())


class Evaluate:
    def __init__(self) -> None:
        metric_file_to_table_map: dict[str, typing.Type[EvaluationTable]] = {
            "Query_Generation.client": QueryGenerationTable,
            "Extract_Response.client": ResponseExtractionTable,
            "Rate.client": RateTable,
            "Database_Generation.server": DatabaseGenerationTable,
            "Answer.server": AnswerTable,
            "Query_Cost.communication": QueryCostTable,
            "Response_Cost.communication": ResponseCostTable,
            "Total_Cost.communication": TotalCostTable,
        }
        _evaluation_table_map: dict[str, EvaluationTable] = dict()
        for metric_file, table_type in metric_file_to_table_map.items():
            assert metric_file in metric_file_to_table_map, \
                f"{metric_file} not in table map."
            _evaluation_table_map[metric_file] = table_type()
        self.evaluation_table_map = _evaluation_table_map
        self.data_path: str = os.fspath("./Data")
        self.average_metrics: dict[
            typing.Type[EvaluationTable], list[float]] = {
            table: list() for table in metric_file_to_table_map.values()
        }
        self.min_metrics: dict[
            typing.Type[EvaluationTable], list[float]] = {
            table: list() for table in metric_file_to_table_map.values()
        }
        self.max_metrics: dict[
            typing.Type[EvaluationTable], list[float]] = {
            table: list() for table in metric_file_to_table_map.values()
        }

    def flip_record_average_switch(self) -> None:
        self.refresh_average_table()

    def read_metric_files(self) -> dict[str, list[list[float]]]:
        """
        :return: {
            metric_filename: [
                [floats...], -> All values (for average)
                [floats...], -> Min
                [floats...], -> Max
            ],
        """
        filename_to_content_map = dict()
        for filename in os.listdir(self.data_path):
            if filename == "Meta":
                continue
            filepath = os.path.join(self.data_path, filename)
            if os.path.isfile(filepath):
                with open(filepath, "r") as file:
                    filename_to_content_map[filename] = file.readlines()
        return {
            filename: [
                [float(value) for value in line.split()]
                for line in content
            ]
            for filename, content in filename_to_content_map.items()
        }

    def run(self, n_power: int, q: int, exit_code: int) -> None:
        assert n_power in range(10, 25), f"{n_power=} not in range of n."
        n_column_index = range(10, 25).index(n_power)
        q_row_index = [2, 16, 128, 256].index(q)
        print(f"==> Evaluating {n_power=} and {q=}.")
        for metric_name, metric_packet in self.read_metric_files().items():
            print(f"===> Evaluating {metric_name} {metric_packet}.")
            table = self.evaluation_table_map[metric_name]
            metric_data_sources = zip([
                table.average_data,
                table.min_data,
                table.max_data
            ], metric_packet)
            assert len(metric_packet) == 3, \
                f"Value must be Average, Min, Max. Received {len(metric_packet)=}."
            for data_source, data_source_values in metric_data_sources:
                table_value = self.determine_metric_value(data_source, table, data_source_values)
                if exit_code != 0:
                    data_source[n_column_index][q_row_index] = f"Code: {exit_code}"
                else:
                    data_source[n_column_index][q_row_index] = f"{table_value:.3f}"
            table.render(show=True)
            table.write_latex_table()

    def determine_metric_value(
            self, data_source: list[list[str]],
            table: EvaluationTable,
            value: list[float]) -> float:
        if data_source == table.average_data:
            self.average_metrics[type(table)].extend(value)
            return statistics.mean(self.average_metrics[type(table)])
        elif data_source == table.min_data:
            self.min_metrics[type(table)].extend(value)
            return min(self.min_metrics[type(table)])
        elif data_source == table.max_data:
            self.max_metrics[type(table)].extend(value)
            return max(self.max_metrics[type(table)])

    def refresh_metric_cache(self) -> None:
        for cache in [
            self.average_metrics,
            self.min_metrics,
            self.max_metrics
        ]:
            for table in cache:
                cache[table].clear()


class TableSection(str, enum.Enum):
    SERVER = "I: Server"
    CLIENT = "II: Client"
    COMMUNICATION_COST = "III: Communication Cost"


@dataclasses.dataclass
class EvaluationTable(abc.ABC):
    average_data: list[list[str]] \
        = dataclasses.field(default_factory=list)
    min_data: list[list[str]] \
        = dataclasses.field(default_factory=list)
    max_data: list[list[str]] \
        = dataclasses.field(default_factory=list)
    column_labels: typing.Final[list[str]] \
        = dataclasses.field(
        default_factory=lambda: [
            f"2^{{{database_size}}}"
            for database_size in range(10, 25)
        ]
    )
    row_labels: typing.Final[list[str]] \
        = dataclasses.field(
        default_factory=lambda: [
            str(q)
            for q in [2, 16, 128, 256]
        ]
    )

    def __post_init__(self):
        self.average_data = [
            ["NA" for _ in range(len(self.row_labels))]
            for _ in range(len(self.column_labels))
        ]
        self.min_data = [
            ["NA" for _ in range(len(self.row_labels))]
            for _ in range(len(self.column_labels))
        ]
        self.max_data = [
            ["NA" for _ in range(len(self.row_labels))]
            for _ in range(len(self.column_labels))
        ]

    @property
    def metric_types(self) -> list[str]:
        return ["Average", "Min", "Max"]

    @property
    def metric_packet(self) -> list[list[list[str]]]:
        return [
            self.average_data, self.min_data, self.max_data
        ]

    @property
    def label(self) -> str:
        return f"tab:{self.title.lower().replace(' ', '_')}"

    @property
    def latex_tables(self) -> list[str]:
        dataframes = self.retrieve_padded_dataframe()
        return [
            df.to_latex(caption=f"[{name}] {self.title} ({self.unit})", label=self.label)
            for name, df in zip(self.metric_types, dataframes)
        ]

    @property
    @abc.abstractmethod
    def section(self) -> TableSection:
        pass

    @property
    @abc.abstractmethod
    def title(self) -> str:
        pass

    @property
    @abc.abstractmethod
    def unit(self) -> str:
        pass

    def __hash__(self) -> int:
        return hash(self.__class__.__name__)

    def __eq__(self, other: typing.Self) -> bool:
        return self.__class__.__name__ == other.__class__.__name__

    def _retrieve_data_column_map(self) -> list[dict[str, list[str]]]:
        packet: list[dict[str, list[str]]] = list()
        for data in self.metric_packet:
            column_map = dict()
            for column_index, column_label in enumerate(self.column_labels):
                column_map[column_label] = data[column_index].copy() \
                    if column_index < len(data) else []
            packet.append(column_map)
        return packet

    def _pad_rows(self, out_data: dict[str, list[str]]) -> dict[str, list[str]]:
        for column_name, column in out_data.items():
            # Lengthen the columns to be of row size.
            out_data[column_name].extend(["NA"] * (len(self.row_labels) - len(column)))
        return out_data

    def retrieve_padded_dataframe(self) -> list[pd.DataFrame]:
        dataframes: list[pd.DataFrame] = list()
        for data in self._retrieve_data_column_map():
            self._pad_rows(data)
            dataframes.append(pd.DataFrame(data, index=self.row_labels))
        return dataframes

    def write_latex_table(self) -> None:
        with open(f"./Latex_Tables/{self.title.replace(' ', '_')}.tex", "w") as file:
            file.write("\n\n".join(self.latex_tables))

    def render(self, show: bool = False, save_to_file: bool = True) -> None:
        dataframes = self.retrieve_padded_dataframe()
        num_tables = len(dataframes)
        total_fig_height = num_tables * 3.5
        fig, axs = plt.subplots(num_tables, 1, figsize=(35, total_fig_height))
        if num_tables == 1:
            axs = [axs]
        fig.patch.set_facecolor("#FAF5FF")
        for ax, metric_type, df in zip(axs, self.metric_types, dataframes):
            ax.axis('off')
            ax.axis('tight')
            ax.set_facecolor("#FAF5FF")
            font_prop = matplotlib.font_manager.FontProperties(family="Inter", weight="bold")
            ax.set_title(
                f"{self.section} - {self.title} ({self.unit}) - {metric_type}", fontproperties=font_prop)
            ax.title.set_fontsize(20)
            table = ax.table(cellText=df.values, colLabels=df.columns, rowLabels=df.index, loc='center')
            for (i, j), cell in table.get_celld().items():
                text: str = cell.get_text().get_text()
                column_or_row_header: bool = i == 0 or j == -1
                if column_or_row_header:
                    cell.set_facecolor("#E4CCFF")
                    cell.get_text().set_fontproperties(font_prop)
                elif text == "NA":
                    cell.set_facecolor("#E7DCF1")
                elif "Code" in text:
                    cell.set_facecolor("#E7C0DC")
                else:
                    cell.set_facecolor("#F4EAFF")
                cell.set_edgecolor('#B3A3C9')
            table.set_fontsize(16)
            table.scale(1, 2.5)
        fig.subplots_adjust(hspace=20.0)
        fig.tight_layout()
        if save_to_file:
            plt.savefig(f"./Figures/{self.title.replace(' ', '_')}.png", dpi=450)
        if show:
            plt.show()
        plt.close()


@dataclasses.dataclass
class QueryGenerationTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.CLIENT

    @property
    def title(self) -> str:
        return "Query generation time per query"

    @property
    def unit(self) -> str:
        return "μs"


@dataclasses.dataclass
class ResponseExtractionTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.CLIENT

    @property
    def title(self) -> str:
        return "Query extraction time per query"

    @property
    def unit(self) -> str:
        return "μs"


@dataclasses.dataclass
class RateTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.CLIENT

    @property
    def title(self) -> str:
        return "Rate"

    @property
    def unit(self) -> str:
        return "number of hashes per record"


@dataclasses.dataclass
class AnswerTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.SERVER

    @property
    def title(self) -> str:
        return "Answer time per query"

    @property
    def unit(self) -> str:
        return "μs"


@dataclasses.dataclass
class DatabaseGenerationTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.SERVER

    @property
    def title(self) -> str:
        return "Encoding time per query"

    @property
    def unit(self) -> str:
        return "μs"


@dataclasses.dataclass
class QueryCostTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.COMMUNICATION_COST

    @property
    def title(self) -> str:
        return "Query cost per query"

    @property
    def unit(self) -> str:
        return "Bytes"


@dataclasses.dataclass
class ResponseCostTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.COMMUNICATION_COST

    @property
    def title(self) -> str:
        return "Response cost per query"

    @property
    def unit(self) -> str:
        return "Bytes"


@dataclasses.dataclass
class TotalCostTable(EvaluationTable):

    @property
    def section(self) -> TableSection:
        return TableSection.COMMUNICATION_COST

    @property
    def title(self) -> str:
        return "Total cost per query"

    @property
    def unit(self) -> str:
        return "Bytes"


def calculate_height(q: int, n: int) -> float:
    """
    Calculate the height of a tree given
    based on log_q(n).
    :param q: Q-value of a q-ary tree.
    :param n: Number of leaves in a tree.
    :return: The height of the tree.
    """
    return math.log(1 << n, q)


def derive_total_database_size(q: int, h: float) -> int:
    """
    Derive the total database size (N) of a tree
    based on q(q^{h} - 1) / q - 1.
    :param q: Q-value of a q-ary tree.
    :param h: Height of the tree.
    :return: The total number of hashes in the
             database.
    """
    return math.ceil(q * ((q ** h) - 1) / (q - 1))


def calculate_n(q: int, h: float) -> int:
    """
    Calculate the number of leaves in a tree based on
    q and h.
    :param q: Q-value of a q-ary tree.
    :param h: Height of the tree.
    :return: The number of leaves in the tree.
    """
    return q ** h


def parse_spiral_configuration(configuration: str) -> tuple[str, list[int, int]]:
    configuration_file = f"./../Documents/Configuration/{configuration}.config"
    with open(configuration_file, "r") as file:
        lines = file.readlines()
    build_configuration: str = lines[1].strip()
    FurtherDims = FoldDims = int
    program_arguments: list[FurtherDims, FoldDims] = \
        [int(argument)
         for argument in lines[4].split()
         if argument.isdigit()][:2]
    assert len(program_arguments) == 2, \
        f"Expected 2 program arguments, read {len(program_arguments)}."
    return build_configuration, program_arguments


def parse_index_file(file_path) -> dict[str, list[int]]:
    with open(file_path, 'r') as file:
        lines = file.readlines()
    trial_configuration = dict()
    for line in lines:
        parts = line.split(';')
        if len(parts) != 3:
            continue
        data_file = parts[0].strip()
        index_arg = parts[2].strip()
        index = int(index_arg.split(':')[1].strip())
        #if mode == Mode.COLOURING:
            #index -= 1
        if data_file in trial_configuration:
            trial_configuration[data_file].append(index)
        else:
            trial_configuration[data_file] = [index]
    return trial_configuration


def find_color_indices_files(directory) -> list[str]:
    matching_files: list[str] = list()
    prefix: typing.Final = "color_indices_" \
        if mode == Mode.COLOURING else "indices_"
    for filename in os.listdir(directory):
        if filename.startswith(prefix) and filename.endswith(".txt"):
            matching_files.append(filename)
    return matching_files


def run_spiral_instance(
        N_power: int, element_size: int, database_file: str,
        clean_build: bool) -> int:
    spiral_configuration_label: str = f"{N_power}_{element_size}"
    parameter_set, program_arguments \
        = parse_spiral_configuration(spiral_configuration_label)
    cmake_configuration = CMakeConfiguration(
        spiral_configuration_label, "Release")
    target = SpiralTarget.SEPERATED
    execution_instance = CMakeRun(
        cmake_configuration,
        CMakeBuild(
            parameter_set, cmake_configuration.build_path,
            target),
        executable=target,
        database_file=database_file,
        further_dimensions=program_arguments[0],
        folding_factor=program_arguments[1],
        perform_build=clean_build
    )
    print(f"\nExecuting command: {execution_instance.generate_command()}")
    exit_code: int = execution_instance.run()
    return exit_code


def inject_query_indices(query_indices: list[int]) -> None:
    query_storage_file = "./../Query_Storage/Default.query"
    with open(query_storage_file, "w") as file:
        for index in query_indices:
            file.write(f"{index}\n")


def dummy_run() -> int:
    import random
    evaluation_data_path = "./Data"
    for filename in os.listdir(evaluation_data_path):
        if filename == "Meta":
            continue
        filepath = os.path.join(evaluation_data_path, filename)
        if os.path.isfile(filepath):
            with open(filepath, "w") as file:
                for line_number in range(3):
                    if line_number == 0:
                        dummy_averages = [random.randint(0, 100) for _ in range(4)]
                        file.write(" ".join([str(value) for value in dummy_averages]) + "\n")
                    else:
                        file.write(f"{random.randint(0, 100)}\n")
    return 0


def retrieve_file_hash_count(database_file: str) -> int:
    database_directory = "./../Database/Colouring/Data" \
        if mode == Mode.COLOURING else "./../Database/PBC/Data"
    with open(os.path.join(database_directory, database_file), "r") as file:
        content = file.readline()
    return len(json.loads(content)[0])


def run_all_samples() -> None:
    evaluator: typing.Final = Evaluate()
    element_size: typing.Final = 32
    index_directory: typing.Final = "../Database/Colouring/Indices" \
        if mode == Mode.COLOURING else "../Database/PBC/Indices"
    all_index_files: typing.Final[list[str]] \
        = find_color_indices_files(index_directory)
    height_range_over_q: typing.Final = {
        2: range(10, 21, 2),
        # 16: range(3, 7),
        # 128: range(2, 4),
        # 256: range(2, 4)
    }
    for q, height_range in height_range_over_q.items():
        for h in height_range:
            evaluator.refresh_metric_cache()
            os.system("clear")
            index_file = f"color_indices_{h}_{q}.txt" \
                if mode == Mode.COLOURING else f"indices_{h}_{q}.txt"
            if index_file not in all_index_files:
                print(f"The requested index file for {h=}, {q=} does not exist.",
                      file=sys.stderr)
                continue
            trial_configuration = parse_index_file(os.path.join(index_directory, index_file))

            clean_build: bool = True
            for database_file, query_indices in trial_configuration.items():
                if f"{h}_{q}" not in database_file:
                    print(f"There is a mismatch between the requested database "
                          f"file ({h=}, {q=}) and the index database {database_file}.",
                          file=sys.stderr)
                    continue

                n = calculate_n(q, h)
                n_power = math.ceil(math.log2(n))
                N = retrieve_file_hash_count(database_file) #Number of each element in each sub-database
                N_power = math.ceil(math.log2(N))
                trial_log_information = f"2^{N_power} corresponding to {q=}, {h=}, {n=}, {n_power=}"
                if N_power > 30 or N_power < 1:
                    print(f"Skipping unsupported database size {trial_log_information}.",
                          file=sys.stderr)
                    continue

                inject_query_indices(query_indices)
                print("\n\n" + "#" * 100 + f"\n==> Injecting {len(query_indices)} query indices.")
                #print(f"==> Running database size: {trial_log_information}.")
                print(f"==> Running database file: {database_file}.")
                exit_code: int = run_spiral_instance(N_power, element_size, database_file, clean_build)
                # exit_code: int = dummy_run()
                evaluator.run(n_power, q, exit_code)
                if exit_code == 0:
                    clean_build = False


if __name__ == "__main__":
    assert mode in Mode, f"{mode=} not in {Mode}."
    run_all_samples()
