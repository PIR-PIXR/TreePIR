
cmake_minimum_required(VERSION 3.12)
project(pirmessage C CXX)
include(common.cmake)

find_package(SEAL 4.0 REQUIRED)


add_library(sealpir memplumber.h memplumber-internals.h memplumber.cpp pir.hpp pir.cpp pir_client.hpp pir_client.cpp pir_server.hpp pir_server.cpp helper.hpp helper.cpp)

#find_package(Boost REQUIRED COMPONENTS system thread)
target_link_libraries(sealpir SEAL::seal)

get_filename_component(hw_proto "proto/pirmessage.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/pirmessage.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/pirmessage.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/proto/pirmessage.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/proto/pirmessage.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}/proto/"
       --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/proto/"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")


# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")


add_library(hw_grpc_proto
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
  ${hw_proto_srcs}
  ${hw_proto_hdrs})
target_link_libraries(hw_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  )
 

foreach(_target
  pirmessage_server
  pirmessage_client 
  )

  add_executable(${_target} "${_target}.cpp")
  target_link_libraries(${_target}  sealpir)

  target_link_libraries(${_target}
    hw_grpc_proto    
    absl::flags
    absl::flags_parse
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()


