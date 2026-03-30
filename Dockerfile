FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        protobuf-compiler \
        libprotobuf-dev \
        libssl-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

# Build with trimmed optional modules to keep image and build time small.
RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DKAFKA=n \
      -DMYSQL=n \
      -DREDIS=n \
      -DCONSUL=n \
      -DUPSTREAM=y \
    && cmake --build build -j"$(nproc)"

FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libprotobuf-dev \
        libssl3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --uid 10001 appuser

WORKDIR /app
COPY --from=builder /workspace/build/rpc_simple_server_demo /app/
COPY --from=builder /workspace/build/rpc_simple_client_demo /app/
COPY --from=builder /workspace/build/rpc_upstream_server_demo /app/
COPY --from=builder /workspace/build/rpc_upstream_client_demo /app/
COPY --from=builder /workspace/build/rpc_single_client_server_qps_test /app/

USER appuser

CMD ["./rpc_simple_server_demo"]
