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

add_executable(rpc_server example/server_main.cc)
add_executable(rpc_client example/client_main.cc)
add_executable(rpc_easy_server example/easy_server_main.cc)
add_executable(rpc_easy_client example/easy_client_main.cc)
add_executable(rpc_upstream_client example/upstream_governance_client.cc)

target_link_libraries(rpc_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_easy_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_easy_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_upstream_client workflow_rpc ${WF_LINK_LIBS})
