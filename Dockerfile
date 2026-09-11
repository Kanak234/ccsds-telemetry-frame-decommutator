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

RUN rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    && cmake --build build --config Release \
    && ctest --test-dir build --output-on-failure

# Runtime stage
FROM ubuntu:24.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 10001 -U appuser

WORKDIR /app
COPY --from=builder /app/build/ccsds_decommutator /usr/local/bin/ccsds_decommutator
COPY --from=builder /app/build/benchmark_throughput /usr/local/bin/benchmark_throughput

RUN chown -R appuser:appuser /app
USER appuser

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD ["ccsds_decommutator", "health"] || exit 1

ENTRYPOINT ["ccsds_decommutator"]
CMD ["--help"]
