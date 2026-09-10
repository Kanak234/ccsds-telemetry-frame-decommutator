# Build stage
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    clang \
    clang-format \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    && cmake --build build --config Release \
    && ctest --test-dir build --output-on-failure

# Runtime stage
FROM ubuntu:24.04 AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/ccsds_decommutator /usr/local/bin/ccsds_decommutator
COPY --from=builder /app/build/benchmark_throughput /usr/local/bin/benchmark_throughput

ENTRYPOINT ["ccsds_decommutator"]
CMD ["--help"]
