# Cycling Route Backend

## 概要 (Overview)

このプロジェクトは、C++と[Drogon](https://github.com/drogonframework/drogon)フレームワークで構築された、サイクリスト向けのルート検索・生成バックエンドAPIです。
オープンソースのルーティングエンジン [OSRM (Open Source Routing Machine)](http://project-osrm.org/) を活用し、自転車に最適化されたルート計算を行います。
また、Google Places API によるスポット検索や、国土地理院の標高タイルAPIによる標高データの取得も行います。

## 主な機能 (Features)

-   **自転車最適化ルート生成**: OSRMのBicycleプロファイルを使用したルート計算。
-   **経由地の指定**: 複数の経由地（ウェイポイント）を含むルートの生成。
-   **ルート沿いのスポット検索**: Google Places API を利用し、ルート周辺のカフェやコンビニ、休憩スポットを検索。
-   **標高データ取得**: 国土地理院 API からルート上の標高情報を取得し、獲得標高などを算出。
-   **希望距離でのルート探索**: 指定された距離に近づけるためのルート調整（開発中）。

## システムアーキテクチャ (System Architecture)

```mermaid
graph TD
    A[Client / Frontend] -->|HTTP Request [JSON]| B(Backend API <br> Drogon C++);
    B -->|OSRM Query| C(OSRM Engine);
    B -->|Nearby Search| D(Google Places API);
    B -->|Elevation Query| E(GSI Elevation API);
    C -->|Route Data| B;
    B -->|HTTP Response [JSON]| A;
```

## 技術スタック (Technology Stack)

-   **言語**: C++20
-   **Webフレームワーク**: Drogon
-   **ルーティングエンジン**: OSRM
-   **主要ライブラリ**:
    -   Boost Libraries (Filesystem, Thread, etc.)
    -   JsonCpp
-   **テスト**: GoogleTest
-   **コンテナ**: Docker, Docker Compose

## システム要件 (Prerequisites)

-   Docker
-   Docker Compose
-   Google Maps API Key (Places API)

## セットアップと実行方法 (Getting Started)

### 1. OSRM用地図データの準備

バックエンドはOSRMを使ってルート計算を行います。ルートディレクトリの `osrm-data` にデータを準備します。

#### 例: 中部地方のデータでセットアップする場合

```bash
# プロジェクトのルートディレクトリで実行
mkdir -p osrm-data

# 中部地方のデータをダウンロード
curl -L http://download.geofabrik.de/asia/japan/chubu-latest.osm.pbf -o osrm-data/chubu-latest.osm.pbf

# OSRMデータの処理 (OSRMバックエンドのバージョンに合わせる)
export PBF_FILE_NAME="chubu-latest"
export OSRM_VERSION="v5.27.1"

docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-extract -p /opt/bicycle.lua "/data/${PBF_FILE_NAME}.osm.pbf"

docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-partition "/data/${PBF_FILE_NAME}.osrm"

docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-customize "/data/${PBF_FILE_NAME}.osrm"
```

### 2. 環境変数の設定

ルートディレクトリの `.env` ファイルに設定します。

```env
GOOGLE_PLACES_API_KEY=あなたのAPIキー
```

### 3. アプリケーションの起動

```bash
# バックエンドコンテナの起動
docker compose up -d backend

# コンテナ内でのビルドと実行
docker compose exec backend bash -c "mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./cycling_backend"
```

## APIエンドポイント (API Endpoints)

### `POST /api/v1/route/generate`

サイクリングルートを生成し、沿道のスポットと標高情報を返します。

#### リクエスト例

```json
{
  "start_point": {"lat": 35.1814, "lon": 136.9063},
  "end_point": {"lat": 35.1709, "lon": 136.8815},
  "waypoints": []
}
```

## テスト (Testing)

```bash
docker compose exec backend bash -c "cd build && ctest --output-on-failure"
```

## ディレクトリ構成 (Project Structure)

- `controllers/`: APIリクエストのハンドリング
- `services/`: ビジネスロジック
    - `elevation/`: 国土地理院API連携
- `utils/`: ポリラインデコード等の共通処理
- `tests/`: ユニットテスト・統合テスト
