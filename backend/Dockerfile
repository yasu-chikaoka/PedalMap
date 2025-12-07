FROM ubuntu:22.04

# 環境変数の設定 (非対話モード)
ENV DEBIAN_FRONTEND=noninteractive

# 必要なパッケージのインストール
# build-essential: コンパイラ等
# cmake: ビルドツール
# git: ソース取得用
# libosrm-dev: OSRMライブラリ
# libjsoncpp-dev, uuid-dev, zlib1g-dev, libssl-dev: Drogon依存
# curl: ユーティリティ
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
    clang-format \
    clang-tidy \
    && rm -rf /var/lib/apt/lists/*

# OSRM Backend のインストール (Source Build)
# v5.27.1 を使用
WORKDIR /tmp
RUN git clone https://github.com/Project-OSRM/osrm-backend.git && \
    cd osrm-backend && \
    git checkout v5.27.1 && \
    mkdir build && \
    cd build && \
    cmake .. -DENABLE_MASON=OFF && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf osrm-backend

# Drogon Framework のインストール
# ソースからビルドして最新版を入れる（推奨）
WORKDIR /tmp
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.9.5 && \
    git submodule update --init && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf drogon

# 作業ディレクトリの設定
WORKDIR /app

# ソースコード等はボリュームマウントされる想定のため、ここでのCOPYは最小限または無し
# 依存関係定義ファイル（CMakeLists.txt等）があれば先にCOPYして事前ビルドするなどの最適化が可能だが、
# 今回は開発環境用としてシンプルに保つ。

CMD ["/bin/bash"]