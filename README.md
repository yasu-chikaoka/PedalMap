# PedalMap - Cycling Route Generator

![CI](https://github.com/yasu-chikaoka/PedalMap/actions/workflows/ci.yml/badge.svg)
![Security Scan](https://github.com/yasu-chikaoka/PedalMap/actions/workflows/security.yml/badge.svg)

PedalMapは、サイクリスト向けのルート自動生成Webアプリケーションです。
出発地と目的地を指定するだけで、自転車に最適化されたルートを瞬時に計算し、周辺のおすすめスポットと共に提案します。

## 🚀 特徴

*   **高速ルート計算**: C++ (OSRM) によるバックエンドで、瞬時に最適ルートを算出。
*   **自転車特化**: 自動車用ナビとは異なる、自転車に適したルート（OSRM Bicycle Profile）を使用。
*   **直感的なUI**: Google Maps と連携し、地名検索やルート描画をスムーズに実現。
*   **スポット提案**: ルート沿いのカフェやレストランなどの休憩スポットを提案（現在はモックデータ）。

## 🛠️ 技術スタック

| Category | Technology |
| :--- | :--- |
| **Frontend** | Next.js (TypeScript), Tailwind CSS, Google Maps API |
| **Backend** | C++20, Drogon (Web Framework), OSRM (Routing Engine) |
| **Data** | OpenStreetMap (OSM) Data |
| **Environment** | Docker, Docker Compose |

## 🏁 始め方 (Getting Started)

### 前提条件

*   Docker & Docker Compose がインストールされていること。
*   Google Maps API Key (Maps JavaScript API, Places API) が取得済みであること。

### 1. リポジトリのクローン

```bash
git clone git@github.com:yasu-chikaoka/PedalMap.git
cd PedalMap
```

### 2. 環境変数の設定

フロントエンド用の環境変数ファイルを作成し、Google Maps APIキーを設定します。

```bash
# frontend/.env.local.example を参考に作成
touch frontend/.env.local
```

`frontend/.env.local` の内容:

```env
NEXT_PUBLIC_GOOGLE_MAPS_API_KEY=あなたのAPIキーをここに貼り付け
NEXT_PUBLIC_API_ENDPOINT=http://localhost:8080/api/v1
```

### 3. OSRMデータの準備（初回のみ）

※リポジトリには地図データ（数百MB）は含まれていません。初回起動時にダウンロードと前処理が必要です。

```bash
# 地図データのダウンロードと前処理を行うためのディレクトリ作成
mkdir -p osrm-data

# コンテナ起動（ビルド含む）
sudo docker compose up -d

# バックエンドコンテナ内で地図データをダウンロード＆処理
# (注意: 数分〜十数分かかります)
sudo docker compose exec backend bash -c "
  curl -L -o /data/kanto-latest.osm.pbf http://download.geofabrik.de/asia/japan/kanto-latest.osm.pbf && \
  osrm-extract -p /usr/local/share/osrm/profiles/bicycle.lua /data/kanto-latest.osm.pbf && \
  osrm-contract /data/kanto-latest.osrm
"
```

### 4. アプリケーションの起動

データ準備完了後、バックエンドサーバーを起動します。

```bash
# バックエンドサーバーの起動
sudo docker compose exec -d backend ./build/cycling_backend

# フロントエンド開発サーバーの起動（既に起動している場合は不要）
sudo docker compose exec -d frontend npm run dev
```

### 5. アクセス

ブラウザで以下のURLにアクセスしてください。

[http://localhost:3000](http://localhost:3000)

## 📁 ディレクトリ構成

```
.
├── backend/            # C++ Backend Project
│   ├── controllers/    # API Controllers
│   ├── CMakeLists.txt  # Build Config
│   └── main.cc         # Entry Point
├── frontend/           # Next.js Frontend Project
│   ├── app/            # App Router Pages
│   └── components/     # React Components
├── osrm-data/          # Map Data (Git Ignored)
└── docker-compose.yml  # Container Orchestration
```

## 🤝 開発協力 (Acknowledgements)

このプロジェクトは、AIコーディングアシスタント **Roo Code** および GoogleのLLM **Gemini** の支援を受けて設計・実装されました。

*   **Roo Code**: アーキテクチャ設計、コード生成、デバッグ支援
*   **Google Gemini**: 技術的な意思決定支援、ドキュメント作成

## 📜 ライセンス (License)

### 本アプリケーションのライセンス
MIT License

### 使用しているオープンソースライセンス
本プロジェクトでは、以下のオープンソースソフトウェアおよびデータを使用しています。

*   **OpenStreetMap Data**: © OpenStreetMap contributors (ODbL License)
    *   本アプリケーションのルートデータは OpenStreetMap のデータに基づいています。
*   **Project-OSRM**: BSD 2-Clause License
*   **Drogon**: MIT License
*   **Next.js / React**: MIT License
*   **Lucide React**: ISC License

### Google Maps Platform
本アプリケーションは Google Maps Platform の API を使用しています。利用に際しては Google Maps Platform の利用規約に従ってください。

## 🧪 CI/CD & Testing

このプロジェクトでは、品質保証のために GitHub Actions を使用して以下のチェックを自動化しています。

### CI Workflow (`ci.yml`)
Push および Pull Request 時に実行されます。

*   **Frontend**:
    *   Lint (`eslint`)
    *   Format Check (`prettier`)
    *   Unit Test (`jest`)
    *   Build Check (`next build`)
*   **Backend**:
    *   Unit Test (`gtest`) - 外部依存を排除したロジックテスト
    *   Format Check (`clang-format`)

### Security Workflow (`security.yml`)
*   **CodeQL**: コードの脆弱性解析（静的解析）
*   **Trivy**: ファイルシステムの脆弱性スキャン

### ローカルでのテスト実行方法

**Frontend**:
```bash
# フロントエンドコンテナで実行
docker compose run --rm frontend npm test
docker compose run --rm frontend npm run lint
```

**Backend (Unit Test)**:
ローカル開発環境（コンテナ内）で実行する場合：
```bash
# テストのビルドと実行
docker compose run --rm backend bash -c "mkdir -p build && cd build && cmake -DBUILD_TESTS_ONLY=ON .. && make && ctest --output-on-failure"
```