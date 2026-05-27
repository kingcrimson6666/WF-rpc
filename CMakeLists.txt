cmake_minimum_required(VERSION 3.16)
project(workflow_rpc_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(LIBRT rt)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rpc/src)

set(WORKFLOW_DEP_LIB "")
find_package(workflow QUIET CONFIG HINTS workflow)
if (workflow_FOUND)
	find_library(WORKFLOW_LIB NAMES libworkflow.a workflow HINTS ${WORKFLOW_LIB_DIR})
	include_directories(${WORKFLOW_INCLUDE_DIR})
	set(WORKFLOW_DEP_LIB ${WORKFLOW_LIB})
else()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/workflow ${CMAKE_CURRENT_BINARY_DIR}/workflow_build)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/workflow/src/include)
	set(WORKFLOW_DEP_LIB workflow-static)
endif()

protobuf_generate_cpp(RPC_PROTO_SRCS RPC_PROTO_HDRS
	${CMAKE_CURRENT_SOURCE_DIR}/proto/echo.proto)

add_library(workflow_rpc STATIC
	rpc/src/rpc_message.cc
	rpc/src/rpc_framework.cc
	rpc/src/rpc_easy.cc
	rpc/src/tinypb_codec.cc
	rpc/src/tinypb_rpc_dispatcher.cc
	rpc/src/tinypb_rpc_controller.cc
	rpc/src/tinypb_rpc_channel.cc
	rpc/src/tinypb_rpc_async_channel.cc
	rpc/src/tinypb_rpc_server.cc
	rpc/src/tinypb_rpc_upstream.cc
	rpc/src/rpc_config.cc
	${RPC_PROTO_SRCS}
)

target_include_directories(workflow_rpc PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/rpc/src
)

if (APPLE)
	set(WF_LINK_LIBS ${WORKFLOW_DEP_LIB} pthread OpenSSL::SSL OpenSSL::Crypto ${Protobuf_LIBRARIES})
else()
	set(WF_LINK_LIBS ${WORKFLOW_DEP_LIB} pthread OpenSSL::SSL OpenSSL::Crypto ${LIBRT} ${Protobuf_LIBRARIES})
endif()

add_executable(rpc_simple_server_demo example/simple_server_demo.cc)
add_executable(rpc_simple_client_demo example/simple_client_demo.cc)
add_executable(rpc_upstream_server_demo example/upstream_server_demo.cc)
add_executable(rpc_upstream_client_demo example/upstream_client_demo.cc)
add_executable(rpc_single_client_server_qps_test test/single_client_server_qps_test.cc)

add_executable(rpc_tinypb_server_demo example/tinypb_server_demo.cc)
add_executable(rpc_tinypb_client_demo example/tinypb_client_demo.cc)
add_executable(rpc_tinypb_async_client_demo example/tinypb_async_client_demo.cc)
add_executable(rpc_tinypb_qps_test test/tinypb_qps_test.cc)
add_executable(rpc_tinypb_loadbalance_demo example/tinypb_loadbalance_demo.cc)

target_link_libraries(rpc_simple_server_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_simple_client_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_upstream_server_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_upstream_client_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_single_client_server_qps_test workflow_rpc ${WF_LINK_LIBS})

target_link_libraries(rpc_tinypb_server_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_tinypb_client_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_tinypb_async_client_demo workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_tinypb_qps_test workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_tinypb_loadbalance_demo workflow_rpc ${WF_LINK_LIBS})