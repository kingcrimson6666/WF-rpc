ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR := $(ROOT_DIR)/build
CORE_BUILD_DIR := $(BUILD_DIR)/make_core
WORKFLOW_DIR := $(ROOT_DIR)/workflow
RPC_SRC_DIR := $(ROOT_DIR)/rpc/src

CXX ?= g++
AR ?= ar
CXXFLAGS ?= -O2 -g -std=c++11 -Wall -Wextra
CPPFLAGS := -I$(RPC_SRC_DIR) -I$(WORKFLOW_DIR)/_include

RPC_SRCS := \
	$(RPC_SRC_DIR)/rpc_message.cc \
	$(RPC_SRC_DIR)/rpc_framework.cc \
	$(RPC_SRC_DIR)/rpc_easy.cc
RPC_OBJS := $(patsubst $(RPC_SRC_DIR)/%.cc,$(CORE_BUILD_DIR)/%.o,$(RPC_SRCS))
RPC_CORE_LIB := $(BUILD_DIR)/libworkflow_rpc_core.a

.PHONY: all workflow rpc proto_examples clean

all: workflow rpc

workflow:
	$(MAKE) -C $(WORKFLOW_DIR)

rpc: $(RPC_CORE_LIB)

$(CORE_BUILD_DIR):
	mkdir -p $@

$(CORE_BUILD_DIR)/%.o: $(RPC_SRC_DIR)/%.cc | $(CORE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(RPC_CORE_LIB): $(RPC_OBJS)
	mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $^

proto_examples: all
	./scripts/build_proto_examples.sh

clean:
	rm -rf $(CORE_BUILD_DIR) $(RPC_CORE_LIB)
	rm -f $(BUILD_DIR)/echo.pb.h $(BUILD_DIR)/echo.pb.cc $(BUILD_DIR)/echo.pb.o
	rm -f $(BUILD_DIR)/rpc_simple_server_demo $(BUILD_DIR)/rpc_simple_client_demo
	rm -f $(BUILD_DIR)/rpc_upstream_server_demo $(BUILD_DIR)/rpc_upstream_client_demo
	-$(MAKE) -C $(WORKFLOW_DIR) clean
