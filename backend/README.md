# Cycling Route Backend

## 概要 (Overview)

このプロジェクトは、C++と[Drogon](https://github.com/drogonframework/drogon)フレームワークで構築された、サイクリスト向けのルート検索・生成バックエンドAPIです。
オープンソースのルーティングエンジン [OSRM (Open Source Routing Machine)](http://project-osrm.org/) を活用し、指定された条件に基づいて最適なサイクリングルートを計算します。

## 主な機能 (Features)

-   **基本的なルート生成**: 開始地点と終了地点を指定して、最適なルートを計算します。
-   **経由地の指定**: 複数の経由地（ウェイポイント）を追加して、ルートをカスタマイズできます。
-   **希望距離でのルート探索**: 全体の走行距離を指定すると、目的地までの間に適切な迂回ルートを自動で計算します。
-   **ルート沿いのスポット検索**: 生成されたルートの周辺にある休憩所やカフェなどのスポット情報を検索し、提供します。

## システムアーキテクチャ (System Architecture)

システムは、クライアントからのリクエストを受け付けるバックエンドAPIと、実際のルート計算を行うOSRMエンジンから構成されています。これらはDockerコンテナとして独立して動作します。

```mermaid
graph TD
    A[Client / Frontend] -->|HTTP Request [JSON]| B(Backend API <br> Drogon C++);
    B -->|OSRM Query| C(OSRM Engine);
    C -->|Route Data| B;
    B -->|HTTP Response [JSON]| A;
```

## 技術スタック (Technology Stack)

-   **言語**: C++20
-   **Webフレームワーク**: Drogon
-   **ルーティングエンジン**: OSRM
-   **主要ライブラリ**:
    -   Boost Libraries (Filesystem, Geometry, etc.)
    -   JsonCpp
-   **テスト**: GoogleTest
-   **コンテナ**: Docker, Docker Compose

## システム要件 (Prerequisites)

-   Docker
-   Docker Compose

## セットアップと実行方法 (Getting Started)

このプロジェクトをローカル環境で実行するための手順です。

### 1. OSRM用地図データの準備

バックエンドはOSRM (Open Source Routing Machine) を使ってルート計算を行います。そのため、まずOSRMが利用する形式の地図データを作成する必要があります。

#### 例: 関東地方のデータでセットアップする場合

ここでは例として、日本の関東地方の地図データを使ってセットアップを進めます。

##### ステップ1: ディレクトリの作成と地図データのダウンロード

```bash
# プロジェクトのルートディレクトリにいることを確認してください
mkdir -p osrm-data

# 関東地方のデータをダウンロード（ファイルサイズが大きいので時間がかかる場合があります）
curl -L http://download.geofabrik.de/asia/japan/kanto-latest.osm.pbf -o osrm-data/kanto-latest.osm.pbf
```

##### ステップ2: Dockerを使ったOSRMデータの処理

ダウンロードした `.pbf` ファイルをOSRMが読み込める形式に変換します。以下のコマンドをプロジェクトのルートディレクトリで順番に実行してください。

```bash
# データファイル名をシェル変数に格納
export PBF_FILE_NAME="kanto-latest"
export OSRM_VERSION="v5.27.1" # Dockerfileとバージョンを合わせています

# 1. グラフデータを抽出 (osrm-extract)
docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-extract -p /opt/bicycle.lua "/data/${PBF_FILE_NAME}.osm.pbf"

# 2. データを分割して階層を準備 (osrm-partition)
docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-partition "/data/${PBF_FILE_NAME}.osrm"

# 3. 検索グラフをカスタマイズ (osrm-customize)
docker run --rm -t -v "${PWD}/osrm-data:/data" "osrm/osrm-backend:${OSRM_VERSION}" \
    osrm-customize "/data/${PBF_FILE_NAME}.osrm"
```

これで `osrm-data` ディレクトリ内に関東地方の地図データが準備できました。

#### 他の地域のデータを使用する場合

もちろん、関東地方以外の地図データも利用できます。

1.  **地図データの選択**: [geofabrik.de](http://download.geofabrik.de/) のサイトから、利用したい地域の `.osm.pbf` ファイルのURLをコピーします。
2.  **ダウンロード**: `curl` コマンドのURLと出力ファイル名を任意のものに書き換えて実行します。
    ```bash
    # 例: カリフォルニア州の場合
    curl -L http://download.geofabrik.de/north-america/us/california-latest.osm.pbf -o osrm-data/california-latest.osm.pbf
    ```
3.  **データ処理**: 上記の「ステップ2」のコマンドを実行する際に、`PBF_FILE_NAME` の値をダウンロードしたファイル名（拡張子なし）に変更してください。
    ```bash
    # 例: カリフォルニア州の場合
    export PBF_FILE_NAME="california-latest"
    # この後、docker run ... のコマンドを3つ実行する
    ```

### 2. アプリケーションの起動

プロジェクトのルートディレクトリで以下のコマンドを実行すると、バックエンドAPIと準備したデータを使うOSRMサーバーが起動します。

```bash
docker-compose up --build
```

初回起動時はDockerイメージのビルドに時間がかかります。
起動後、バックエンドAPIは `http://localhost:8080` でリクエストを受け付けます。

## APIエンドポイント (API Endpoints)

### `POST /api/v1/route/generate`

新しいサイクリングルートを生成します。

#### リクエストボディ

```json
{
  "start_point": {
    "lat": 35.681236,
    "lon": 139.767125
  },
  "end_point": {
    "lat": 35.658581,
    "lon": 139.745433
  },
  "waypoints": [
    {
      "lat": 35.66983,
      "lon": 139.77889
    }
  ],
  "preferences": {
    "target_distance_km": 50
  }
}
```

**パラメータ詳細:**

| キー                 | 型       | 必須 | 説明                                                                                       |
| -------------------- | -------- | :--: | ------------------------------------------------------------------------------------------ |
| `start_point`        | Object   |  ✅  | 開始地点の座標オブジェクト (`lat`, `lon`)                                                  |
| `end_point`          | Object   |  ✅  | 終了地点の座標オブジェクト (`lat`, `lon`)                                                  |
| `waypoints`          | Array    |  -   | 経由地の座標オブジェクトの配列。                                                           |
| `preferences`        | Object   |  -   | ルート設定に関するオブジェクト。                                                           |
| `target_distance_km` | Number   |  -   | `preferences` 内のキー。希望するおおよその走行距離（km）。指定がない場合は最短ルートを探索。 |

#### レスポンスボディ (成功時)

```json
{
  "summary": {
    "total_distance_m": 52104.5,
    "estimated_moving_time_s": 9378.8
  },
  "geometry": "_p~ncF~o|u... (Polyline encoded string)",
  "stops": [
    {
      "name": "Cycle Cafe A",
      "type": "cafe",
      "location": {
        "lat": 35.6701,
        "lon": 139.7792
      },
      "rating": 4.5
    }
  ]
}
```

## テスト (Testing)

以下のコマンドでDockerコンテナに入り、`ctest`を実行することで単体・結合テストを実行できます。

```bash
# 1. バックエンドコンテナにアクセス
docker compose exec backend bash

# 2. ビルドディレクトリに移動してテストを実行
cd build
ctest
```

## コードスタイル (Coding Standards)

このプロジェクトでは、以下のツールを用いてコードスタイルと品質を維持しています。

-   **フォーマット**: `clang-format` (Google Styleベース)
-   **静的解析**: `clang-tidy`

設定ファイルはリポジトリのルートにある `.clang-format` と `.clang-tidy` を参照してください。

## 今後の開発計画 (Future Plans)

現在、`RefactoringPlan.md` に基づき、機能の拡張とリファクタリングを計画しています。
主な計画は以下の通りです。

-   **スポット検索機能の高度化**: `Boost.Geometry` のR-treeを導入し、ルート沿いのスポットをより高速かつ高精度に検索する機能。

詳細は [`backend/RefactoringPlan.md`](./backend/RefactoringPlan.md) をご覧ください。