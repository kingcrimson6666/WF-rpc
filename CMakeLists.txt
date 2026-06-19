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
	rpc/src/rpc_config.cc
	rpc/src/rpc_circuit_breaker.cc
	rpc/src/rpc_rate_limiter.cc
	rpc/src/rpc_metrics.cc
	rpc/src/rpc_logger.cc
	rpc/src/rpc_service_registry.cc
	rpc/src/rpc_service_governance.cc
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

# Simple RPC示例（新）
add_executable(simple_rpc_server example/simple_rpc_server.cc)
add_executable(simple_rpc_client example/simple_rpc_client.cc)
add_executable(simple_rpc_lb_server example/simple_rpc_lb_server.cc)
add_executable(simple_rpc_lb_client example/simple_rpc_lb_client.cc)

target_link_libraries(simple_rpc_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(simple_rpc_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(simple_rpc_lb_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(simple_rpc_lb_client workflow_rpc ${WF_LINK_LIBS})

# TinyPB RPC示例（新）
add_executable(tinypb_rpc_server example/tinypb_rpc_server.cc)
add_executable(tinypb_rpc_client example/tinypb_rpc_client.cc)
add_executable(tinypb_rpc_async_client example/tinypb_rpc_async_client.cc)
add_executable(tinypb_rpc_lb_server example/tinypb_rpc_lb_server.cc)
add_executable(tinypb_rpc_lb_client example/tinypb_rpc_lb_client.cc)
add_executable(tinypb_rpc_tls_server example/tinypb_rpc_tls_server.cc)
add_executable(tinypb_rpc_tls_client example/tinypb_rpc_tls_client.cc)
add_executable(tinypb_rpc_discovery_server example/tinypb_rpc_discovery_server.cc)
add_executable(tinypb_rpc_discovery_client example/tinypb_rpc_discovery_client.cc)
add_executable(tinypb_rpc_config_server example/tinypb_rpc_config_server.cc)
add_executable(tinypb_rpc_config_client example/tinypb_rpc_config_client.cc)

target_link_libraries(tinypb_rpc_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_async_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_lb_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_lb_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_tls_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_tls_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_discovery_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_discovery_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_config_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_config_client workflow_rpc ${WF_LINK_LIBS})

# 整合测试示例（新）
add_executable(tinypb_rpc_full_integration_server example/tinypb_rpc_full_integration_server.cc)
add_executable(tinypb_rpc_full_integration_client example/tinypb_rpc_full_integration_client.cc)
add_executable(tinypb_rpc_microservice_integration example/tinypb_rpc_microservice_integration.cc)
add_executable(rpc_comparison_integration example/rpc_comparison_integration.cc)

target_link_libraries(tinypb_rpc_full_integration_server workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_full_integration_client workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(tinypb_rpc_microservice_integration workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_comparison_integration workflow_rpc ${WF_LINK_LIBS})

# 性能测试（保留）
add_executable(rpc_single_client_server_qps_test test/single_client_server_qps_test.cc)
add_executable(rpc_tinypb_qps_test test/tinypb_qps_test.cc)

target_link_libraries(rpc_single_client_server_qps_test workflow_rpc ${WF_LINK_LIBS})
target_link_libraries(rpc_tinypb_qps_test workflow_rpc ${WF_LINK_LIBS})