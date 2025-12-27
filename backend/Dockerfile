# --- Build Stage ---
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies in a single layer to reduce size
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libtbb-dev \
    liblua5.3-dev \
    libluabind-dev \
    libstxxl-dev \
    libxml2-dev \
    libosmpbf-dev \
    libbz2-dev \
    libzip-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    curl \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install OSRM (pinned version)
WORKDIR /tmp
RUN git clone https://github.com/Project-OSRM/osrm-backend.git && \
    cd osrm-backend && \
    git checkout v5.27.1 && \
    mkdir build && \
    cd build && \
    cmake .. -DENABLE_MASON=OFF -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf /tmp/osrm-backend

# Install Drogon (pinned version)
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.9.5 && \
    git submodule update --init && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf /tmp/drogon

# --- Development Stage ---
FROM builder AS dev

RUN apt-get update && apt-get install -y \
    clang-format \
    clang-tidy \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
# We don't COPY . /app here to allow for volume mounting in docker-compose
# COPY . /app

CMD ["/bin/bash"]

# --- Production Runner Stage ---
FROM ubuntu:22.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libboost-filesystem1.74.0 \
    libboost-iostreams1.74.0 \
    libboost-program-options1.74.0 \
    libboost-thread1.74.0 \
    libboost-system1.74.0 \
    libboost-regex1.74.0 \
    libboost-chrono1.74.0 \
    libboost-date-time1.74.0 \
    libboost-atomic1.74.0 \
    libtbb12 \
    liblua5.3-0 \
    libxml2 \
    libosmpbf-dev \
    libprotobuf23 \
    libjsoncpp25 \
    uuid-runtime \
    zlib1g \
    libssl3 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built libraries and binaries from builder
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/bin /usr/local/bin
COPY --from=builder /usr/local/share/osrm /usr/local/share/osrm

# Update library cache
RUN ldconfig

EXPOSE 8080
