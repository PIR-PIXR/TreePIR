cmake_minimum_required(VERSION 3.0)

project(VBPIR_PBC)

# Set the build directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories(header)

# Set the output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Get all source files under src
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# Create executable target and add source files
add_executable(vbpir_pbc ${SOURCES})

# Add the executable target
# add_executable(vbpir_pbc main.cpp client.cpp server.cpp pirparams.cpp utils.h batchpirparams.cpp batchpirserver.cpp batchpirclient.cpp)

# Set include directories
target_include_directories(vbpir_pbc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++ standard
set_target_properties(vbpir_pbc PROPERTIES CXX_STANDARD 11)


# Add seal library
find_package(SEAL 4.0 REQUIRED)
target_link_libraries(vbpir_pbc SEAL::seal)

# Add compiler flags for optimization
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vbpir_pbc PRIVATE "-O3")
endif()
