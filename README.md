# workflow-rpc-demo

一个基于 Workflow 与 Protobuf 的轻量 RPC 示例工程，包含：

- 最小可用 RPC 框架封装（server/client/upstream）
- 单机直连调用示例（simple demo）
- upstream 加权路由示例（upstream demo）
- 单 client + 单 server 的多线程 QPS 测试程序

项目目标是帮助你快速完成从“定义 proto -> 启动服务 -> 发起调用 -> 做吞吐测试”的完整闭环。

## 1. 项目结构

```text
project/
├── CMakeLists.txt                 # 根 CMake 构建入口（推荐）
├── Makefile                       # 兼容 make 构建入口
├── proto/
│   └── echo.proto                 # 示例 RPC 协议定义
├── rpc/
│   └── src/
│       ├── rpc_message.*          # 自定义 RPC 消息编解码
│       ├── rpc_framework.*        # 底层 RPC server/client/upstream 封装
│       └── rpc_easy.*             # 便捷 API（SimpleRpcServer/Client）
├── example/
│   ├── simple_server_demo.cc      # 单服务端示例
│   ├── simple_client_demo.cc      # 单客户端示例
│   ├── upstream_server_demo.cc    # 两后端服务端示例
│   └── upstream_client_demo.cc    # upstream 客户端示例
├── test/
│   └── single_client_server_qps_test.cc  # 多线程 QPS 压测程序
├── scripts/
│   └── build_proto_examples.sh    # make 方式下构建 proto + example 的脚本
└── workflow/                      # Workflow 子模块/源码
```

## 2. 核心能力

### 2.1 RPC 状态语义

`rpc/src/rpc_framework.h` 定义了统一状态码：

- `RPC_OK`：成功
- `RPC_BAD_REQUEST`：非法请求
- `RPC_NOT_FOUND`：找不到 service/method
- `RPC_PROTO_PARSE_ERROR`：请求/响应 protobuf 解析失败
- `RPC_PROTO_SERIALIZE_ERROR`：响应序列化失败
- `RPC_INTERNAL_ERROR`：服务端内部错误
- `RPC_NETWORK_ERROR`：网络层错误

客户端返回还包含 Workflow 运行态：

- `state == WFT_STATE_SUCCESS` 表示传输层成功
- `status == RPC_OK` 表示 RPC 业务成功

通常需要两者都成功才算一次完整成功调用。

### 2.2 便捷 API

`rpc/src/rpc_easy.h` 提供了三组高层封装：

- `SimpleRpcServer`：快速注册 protobuf 方法并启动服务
- `SimpleRpcClient`：同步阻塞调用，返回 `SimpleRpcResult`
- `ServiceRegistry`：配置/清理 upstream 路由（加权后端等）

## 3. 环境依赖

建议在 Linux 下准备：

- C++ 编译器：`g++`（支持 C++11）
- CMake：`>= 3.16`
- GNU Make
- Protobuf：`protoc` + C++ 库
- OpenSSL 开发库
- pthread / dl / rt（Linux 常见系统库）

如果你使用 Ubuntu，可参考：

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake make \
  protobuf-compiler libprotobuf-dev \
  libssl-dev
```

## 4. 构建方式

## 4.1 方式 A：CMake（推荐）

```bash
cd project
cmake -S . -B build
cmake --build build -j
```

构建完成后，主要可执行文件在 `build/` 下：

- `rpc_simple_server_demo`
- `rpc_simple_client_demo`
- `rpc_upstream_server_demo`
- `rpc_upstream_client_demo`
- `rpc_single_client_server_qps_test`

## 4.2 方式 B：Make + 脚本

```bash
cd project
make                # 构建 workflow 与 rpc core
make proto_examples # 调用 scripts/build_proto_examples.sh 构建 proto + example
```

说明：

- `make` 主要产出核心静态库（如 `build/libworkflow_rpc_core.a`）
- `make proto_examples` 会生成并编译 `echo.pb.cc/.h` 及示例二进制

## 5. 快速运行

## 5.1 simple 直连示例

终端 1（服务端）：

```bash
cd project/build
./rpc_simple_server_demo
```

终端 2（客户端）：

```bash
cd project/build
./rpc_simple_client_demo
```

预期：客户端输出类似 `simple rpc response: echo_simple: hello_simple_rpc`。

## 5.2 upstream 加权路由示例

终端 1（启动两个后端）：

```bash
cd project/build
./rpc_upstream_server_demo
```

终端 2（通过 upstream 调用）：

```bash
cd project/build
./rpc_upstream_client_demo
```

说明：

- upstream 客户端会在进程内配置加权后端（9100 权重大于 9101）
- 返回消息前缀可用于观察命中后端（`from_9100` / `from_9101`）

## 5.3 单机多线程 QPS 测试

该测试程序会在同一进程内：

1. 启动本地单服务端（127.0.0.1:19000）
2. 以线程档位 `[1, 2, 4, 8, 16, 32]` 启动客户端并发调用
3. 统计每个档位的成功数、失败数、耗时、QPS、平均时延

运行：

```bash
cd project/build
./rpc_single_client_server_qps_test 1000
```

其中 `1000` 表示“每线程请求数”，可按机器性能调大，例如：

```bash
./rpc_single_client_server_qps_test 5000
```

输出字段含义：

- `threads`：并发线程数
- `total`：总请求数（threads * requests_per_thread）
- `success` / `fail`：成功/失败请求数
- `elapsed`：本档位总耗时（秒）
- `qps`：每秒成功请求数
- `avg_latency`：平均时延（微秒，按总请求均摊）

### 5.3.1 实测结果（2026-03-20）

本次测试命令：

```bash
cd project/build
./rpc_single_client_server_qps_test 1000
```

测试说明：

- 本地单机回环地址（127.0.0.1）
- 单 client 进程 + 单 server 实例
- 每线程请求数 `requests_per_thread = 1000`

实测数据如下：

| threads | total | success | fail | elapsed(s) | qps | avg_latency(us) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1000 | 1000 | 0 | 0.17 | 5969.15 | 167.53 |
| 2 | 2000 | 2000 | 0 | 0.15 | 13018.10 | 76.82 |
| 4 | 4000 | 4000 | 0 | 0.15 | 26076.17 | 38.35 |
| 8 | 8000 | 8000 | 0 | 0.14 | 55697.90 | 17.95 |
| 16 | 16000 | 16000 | 0 | 0.15 | 107716.52 | 9.28 |
| 32 | 32000 | 32000 | 0 | 0.19 | 172362.60 | 5.80 |

观察：

- 各线程档位 `fail=0`，本轮无失败请求。
- QPS 随并发线程增加持续上升，说明在该负载下尚未明显触顶。
- `avg_latency` 随线程增加而下降，主要因为总体吞吐提升带来的均摊效应。

## 6. 如何扩展一个新 RPC 方法

以新增 `SayHello` 方法为例：

1. 在 `proto/echo.proto` 中增加消息体和 service rpc 定义
2. 重新生成 protobuf 代码并重编译
3. 在服务端使用 `register_method<Req, Resp>(...)` 注册处理器
4. 在客户端通过 `SimpleRpcClient::call(...)` 发起调用
5. 检查返回：`state == WFT_STATE_SUCCESS && status == RPC_OK`

## 7. 常见问题排查

### 7.1 服务端启动失败

常见原因：

- 端口已被占用
- 没有权限绑定对应端口
- 地址配置错误

建议：

```bash
ss -lntp | grep 9000
ss -lntp | grep 9100
ss -lntp | grep 9101
ss -lntp | grep 19000
```

### 7.2 客户端调用失败（state/status 非成功）

检查顺序建议：

1. 服务是否已启动且监听正确端口
2. `service` / `method` 字符串是否与服务端注册完全一致
3. Protobuf 请求/响应结构是否匹配
4. 是否误把 upstream 名称配置成可被 DNS 解析的真实域名（可能绕过预期路由）

### 7.3 CMake 新目标找不到

当你新增了可执行目标但 `cmake --build build --target ...` 提示无该 target，通常是因为未重新配置。执行：

```bash
cd project
cmake -S . -B build
```

然后再次构建目标。

## 8. 开发建议

- 推荐先跑通 `simple`，再切到 `upstream`
- 压测时先用小请求量验证，再逐步增大
- 若要做稳定对比，建议增加预热阶段和多轮统计
- 若要观察尾延迟，建议额外记录 p95/p99

## 9. 许可证与致谢

本仓库包含 Workflow 目录，其许可证与说明见 `workflow/` 内相关文件（如 `LICENSE`、`README.md`、`README_cn.md`）。

---


