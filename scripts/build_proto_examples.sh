#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OBJ_DIR="${BUILD_DIR}/make_examples"
PROTO_DIR="${ROOT_DIR}/proto"
EXAMPLE_DIR="${ROOT_DIR}/example"
RPC_SRC_DIR="${ROOT_DIR}/rpc/src"
WORKFLOW_INCLUDE_DIR="${ROOT_DIR}/workflow/_include"
WORKFLOW_LIB="${ROOT_DIR}/workflow/_lib/libworkflow.a"
RPC_CORE_LIB="${BUILD_DIR}/libworkflow_rpc_core.a"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -g -std=c++11 -Wall -Wextra}"

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "[ERROR] missing command: $1" >&2
		exit 1
	fi
}

need_cmd make
need_cmd protoc
need_cmd "$CXX"

mkdir -p "${BUILD_DIR}" "${OBJ_DIR}"

echo "[INFO] building workflow and rpc core via make"
make -C "${ROOT_DIR}" all

echo "[INFO] generating protobuf sources"
protoc -I "${PROTO_DIR}" --cpp_out="${BUILD_DIR}" "${PROTO_DIR}/echo.proto"

compile_obj() {
	local src="$1"
	local obj="$2"
	"${CXX}" ${CXXFLAGS} \
		-I"${BUILD_DIR}" \
		-I"${RPC_SRC_DIR}" \
		-I"${WORKFLOW_INCLUDE_DIR}" \
		-c "${src}" -o "${obj}"
}

link_bin() {
	local out="$1"
	shift
	"${CXX}" "$@" \
		"${RPC_CORE_LIB}" \
		"${WORKFLOW_LIB}" \
		-lprotobuf -lssl -lcrypto -lpthread -ldl -lrt \
		-o "${out}"
}

compile_obj "${BUILD_DIR}/echo.pb.cc" "${OBJ_DIR}/echo.pb.o"
compile_obj "${EXAMPLE_DIR}/simple_server_demo.cc" "${OBJ_DIR}/simple_server_demo.o"
compile_obj "${EXAMPLE_DIR}/simple_client_demo.cc" "${OBJ_DIR}/simple_client_demo.o"
compile_obj "${EXAMPLE_DIR}/upstream_server_demo.cc" "${OBJ_DIR}/upstream_server_demo.o"
compile_obj "${EXAMPLE_DIR}/upstream_client_demo.cc" "${OBJ_DIR}/upstream_client_demo.o"
compile_obj "${EXAMPLE_DIR}/tinypb_loadbalance_demo.cc" "${OBJ_DIR}/tinypb_loadbalance_demo.o"

echo "[INFO] linking example binaries"
link_bin "${BUILD_DIR}/rpc_simple_server_demo" "${OBJ_DIR}/simple_server_demo.o" "${OBJ_DIR}/echo.pb.o"
link_bin "${BUILD_DIR}/rpc_simple_client_demo" "${OBJ_DIR}/simple_client_demo.o" "${OBJ_DIR}/echo.pb.o"
link_bin "${BUILD_DIR}/rpc_upstream_server_demo" "${OBJ_DIR}/upstream_server_demo.o" "${OBJ_DIR}/echo.pb.o"
link_bin "${BUILD_DIR}/rpc_upstream_client_demo" "${OBJ_DIR}/upstream_client_demo.o" "${OBJ_DIR}/echo.pb.o"
link_bin "${BUILD_DIR}/rpc_tinypb_loadbalance_demo" "${OBJ_DIR}/tinypb_loadbalance_demo.o" "${OBJ_DIR}/echo.pb.o"

echo "[OK] built proto and example binaries in ${BUILD_DIR}"
