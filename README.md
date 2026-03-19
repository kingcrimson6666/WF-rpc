# Workflow RPC Demo（基于 Protobuf）

这是一个基于 Workflow 的轻量级 RPC 示例工程，包含：

1. 自定义二进制 RPC 协议（头部 + 路由元信息 + Protobuf 负载）
2. 服务端/客户端基础框架封装
3. 上游服务发现与治理能力（基于 Workflow Upstream）
4. 更易用的一层业务封装（EasyRpcServer / EasyRpcClient / ServiceRegistry）
5. 可直接运行的示例程序

该项目适合用于学习以下能力：

1. Workflow 网络任务模型与 ProtocolMessage 扩展
2. Protobuf 消息序列化/反序列化在自定义 RPC 协议中的接入
3. 服务治理（权重路由、故障切换、动态节点管理）的工程化落地

---

## 目录结构

```text
.
├── CMakeLists.txt                  # 项目根构建入口
├── README.md
├── proto/
│   └── echo.proto                  # Protobuf 定义
├── rpc/
│   └── src/
│       ├── rpc_message.h/.cc       # RPC 协议报文编解码
│       ├── rpc_framework.h/.cc     # RpcServer/RpcClient 框架层
│       └── rpc_easy.h/.cc          # 易用封装层
├── example/
│   ├── server_main.cc              # 基础服务端示例（原始框架）
│   ├── client_main.cc              # 基础客户端示例（原始框架）
│   ├── easy_server_main.cc         # 易用服务端示例
│   ├── easy_client_main.cc         # 易用客户端示例
│   └── upstream_governance_client.cc # 治理调用示例
└── workflow/                       # Workflow 源码（本地依赖）
```

说明：`rpc/` 目录仅保留源码，构建入口、proto 与示例都在根目录，便于统一管理。

---

## 核心特性

1. 一次 RPC 调用对应一个 `WFNetworkTask<RpcRequest, RpcResponse>`
2. Protobuf 作为消息体，支持通用二进制传输
3. 自定义 RPC 协议头，支持序列号、状态码、路由元信息
4. 内置 Workflow Upstream 治理能力：
  - 加权随机
  - 一致性哈希
  - 手动路由
  - VNSWRR
  - 节点动态增删
5. 提供 URL 形式的任务创建接口，支持 path/query/fragment 参与路由策略

---

## 依赖要求

1. CMake >= 3.16
2. C++11 编译器（g++/clang++）
3. Protobuf（libprotobuf + protoc）
4. OpenSSL
5. Linux 下通常还需要 `librt`

---

## 构建方法

项目现在提供两段式构建：

1. 根目录 `Makefile`：负责编译 `workflow` 与 `rpc/src` 核心源码
2. 独立脚本：负责 `proto` 代码生成，以及 `example` 程序编译和链接

### 1. 编译 workflow + rpc 核心

在项目根目录执行：

```sh
make
```

该步骤会生成：

1. `workflow/_lib/libworkflow.a`
2. `build/libworkflow_rpc_core.a`

### 2. 编译 proto + example 并链接

执行：

```sh
./scripts/build_proto_examples.sh
```

该步骤会自动：

1. 由 `proto/echo.proto` 生成 `build/echo.pb.h` 和 `build/echo.pb.cc`
2. 编译 `example/` 下示例源码
3. 链接 `build/rpc_server`、`build/rpc_client`、`build/rpc_easy_server`、`build/rpc_easy_client`、`build/rpc_upstream_client`

也可以一条命令完成：

```sh
make proto_examples
```

清理构建产物：

```sh
make clean
```

---

## 快速开始

### 1. 基础调用（原始框架 API）

终端 1：

```sh
./build/rpc_server
```

终端 2：

```sh
./build/rpc_client
```

预期输出：

```text
response: echo: hello
```

默认值如下：

1. 服务端默认端口：`9000`
2. 客户端默认目标：`127.0.0.1:9000`
3. 客户端默认消息：`hello`

也支持显式参数覆盖：

```sh
./build/rpc_server 9000
./build/rpc_client 127.0.0.1 9000 hello
```

### 2. 基础调用（易用封装 API）

终端 1：

```sh
./build/rpc_easy_server
```

终端 2：

```sh
./build/rpc_easy_client
```

默认值如下：

1. 服务端默认端口：`9000`
2. 客户端默认目标：`127.0.0.1:9000`
3. 客户端默认消息：`hello_easy`

也支持显式参数覆盖：

```sh
./build/rpc_easy_server 9000
./build/rpc_easy_client 127.0.0.1 9000 hello_easy
```

### 3. 治理调用（upstream 名 + 节点列表）

先启动两台服务：

```sh
./build/rpc_easy_server 9000
./build/rpc_easy_server 9001
```

再发起治理调用：

```sh
./build/rpc_upstream_client echo.service 9000 hello_gov 127.0.0.1:9000,127.0.0.1:9001
```

---

## 协议与调用流程

### 协议层（rpc_message）

协议头固定 32 字节，核心字段包括：

1. `magic`（`WRPC`）
2. `version`
3. `sequence`
4. `meta_len`
5. `payload_len`
6. `status`

其中：

1. `meta` 存放 `service + method` 路由信息
2. `payload` 存放 Protobuf 二进制

### 框架层（rpc_framework）

1. 客户端：
  - 将请求对象 `SerializeToString`
  - 填充 `service/method` + `payload`
  - 发送网络任务
2. 服务端：
  - 按 `service/method` 查找处理函数
  - 请求 `ParseFromString`
  - 执行业务逻辑
  - 响应 `SerializeToString`

---

## 状态码定义（RpcStatus）

1. `RPC_OK = 0`
2. `RPC_BAD_REQUEST = 400`
3. `RPC_NOT_FOUND = 404`
4. `RPC_PROTO_PARSE_ERROR = 422`
5. `RPC_PROTO_SERIALIZE_ERROR = 423`
6. `RPC_INTERNAL_ERROR = 500`
7. `RPC_NETWORK_ERROR = 503`

---

## API 说明

### 一、基础框架 API

#### 服务端

1. `RpcServer::register_pb_method<Req, Resp>(service, method, handler)`
2. `RpcServer::start(...)`
3. `RpcServer::stop()`

#### 客户端

1. `RpcClient::create_pb_task<Req, Resp>(host, port, service, method, request, retry_max, callback)`
2. `RpcClient::create_pb_task_by_url<Req, Resp>(url, service, method, request, retry_max, callback)`

### 二、治理相关 API

1. `RpcClient::create_weighted_upstream(...)`
2. `RpcClient::create_consistent_hash_upstream(...)`
3. `RpcClient::create_manual_upstream(...)`
4. `RpcClient::create_vnswrr_upstream(...)`
5. `RpcClient::add_upstream_server(...)`
6. `RpcClient::remove_upstream_server(...)`
7. `RpcClient::delete_upstream(...)`
8. `RpcClient::configure_weighted_upstream(...)`

### 三、易用封装 API（rpc_easy）

1. `EasyRpcServer`
  - 构造时绑定 `service_name`
  - `register_method` 只需填 `method`
2. `EasyRpcClient`
  - 构造时绑定 `host/port/service_name`
  - `create_task` 只需填 `method` 和请求对象
3. `ServiceRegistry`
  - 对 `RpcClient` 的治理接口再封装，简化服务注册/摘除操作

---

## 代码片段示例

### 1. 使用易用封装注册服务

```cpp
wf_rpc::EasyRpcServer server("wf.rpc.example.EchoService", &params);
server.register_method<EchoRequest, EchoResponse>(
   "Echo",
   [](const EchoRequest& req, EchoResponse& resp) {
      resp.set_message("echo: " + req.message());
   });
```

### 2. 使用易用封装发起调用

```cpp
wf_rpc::EasyRpcClient client("127.0.0.1", 9000, "wf.rpc.example.EchoService");
auto *task = client.create_task<EchoRequest, EchoResponse>(
   "Echo", req, 1, callback);
```

### 3. 使用 ServiceRegistry 配置治理

```cpp
std::vector<wf_rpc::UpstreamServer> servers = {
   {"127.0.0.1:9000", 5},
   {"127.0.0.1:9001", 1}
};
wf_rpc::ServiceRegistry::configure_weighted("echo.service", servers, true);
```

---

## 示例程序说明

1. `example/server_main.cc`
  - 原始框架服务端写法
2. `example/client_main.cc`
  - 原始框架客户端写法
  - 支持一次性 upstream bootstrap 参数：
    - `addr1:port1@w1,addr2:port2@w2,...`
3. `example/easy_server_main.cc`
  - 简化服务端写法
4. `example/easy_client_main.cc`
  - 简化客户端写法
5. `example/upstream_governance_client.cc`
  - 展示 ServiceRegistry + EasyRpcClient 的治理调用路径

---

## 常见问题

### 1. 启动后调用报连接错误

排查顺序：

1. 目标服务端是否已启动并监听正确端口
2. 客户端 host/port 是否匹配
3. 本地防火墙或端口占用情况

### 2. 返回 `RPC_NOT_FOUND`

通常是 `service` 或 `method` 字符串不一致。

### 3. 返回 `RPC_PROTO_PARSE_ERROR`

通常是请求/响应类型与实际消息体不匹配，或 protobuf 版本/定义不一致。

---

## 后续可扩展方向

1. 对接外部注册中心（Consul/Nacos/etcd 等）实现动态服务发现
2. 增加拦截器机制（日志、鉴权、指标）
3. 增加统一配置中心与热更新
4. 增加更多 RPC 方法与多服务示例

