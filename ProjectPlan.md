# 🚴‍♂️ ProjectPlan: サイクリングコース自動生成Webアプリ

## 🎯 プロジェクト概要
ユーザーが希望条件を入力すると、最適なサイクリングコースを自動生成し、Google Mapsに反映できるWebアプリを開発する。独自アルゴリズムにより、指定された**獲得標高**や**距離**を満たす周回ルートを高速に生成し、適切なタイミングでの食事・休憩スポットも提案する。

---

## 🛠️ 技術スタック

### フロントエンド
- **React / Next.js**  
  - SPA + SSR対応で高速表示  
  - ルーティング管理とSEO最適化
- **Tailwind CSS**  
  - コンポーネントベースのスタイリング  
  - レスポンシブ対応

### バックエンド (High Performance)
- **C++ (C++20)**
  - ルート計算エンジンのコアロジック実装
  - **Drogon**: 高速非同期Webフレームワーク（HTTP APIサーバー）
- **OSRM (Open Source Routing Machine) / libosrm**
  - C++ライブラリとして組み込み利用
  - 独自の重み付け（Profile）によるルート探索
  - SRTMデータ統合による標高コスト計算

### 外部サービス・データ
- **Google Maps API** (Maps JavaScript API)
  - クライアント側での地図描画・ルート表示
- **Google Places API**
  - ルート沿いの飲食店・観光地検索
- **OpenStreetMap (OSM) Data**
  - OSRMの基盤地図データ

---

## 🏗️ システムアーキテクチャ

```mermaid
graph TD
    User[ユーザー] -->|条件入力| Client[🖥️ Frontend (Next.js)]
    
    subgraph "Docker Compose Environment"
        subgraph "Backend Container (Ubuntu 22.04)"
            API[API Handler (Drogon)]
            Logic[🚴 Route Logic]
            OSRM[🗺️ libosrm]
        end
        
        subgraph "Frontend Container (Node.js)"
            Next[Next.js Dev Server]
        end
        
        Data[(Vol: OSRM Data)] -.-> OSRM
    end

    Client -->|HTTP Request| Next
    Next -->|API Call| API
    
    API --> Logic
    Logic --> OSRM
    
    %% External
    ExtHandler[External API Client]
    API --> ExtHandler
    ExtHandler --> Places[☁️ Google Places API]
```

---

## 🐳 開発環境 (Docker) 構成

### ディレクトリ構造案
```
/cycling
  ├── /backend        # C++ Project (CMake, Drogon, OSRM logic)
  ├── /frontend       # Next.js Project
  ├── /osrm-data      # 地図データ (.pbf, .osrm) ※Git除外
  ├── docker-compose.yml
  └── ProjectPlan.md
```

### Docker Compose 定義概要
- **backend**:
  - Image: `ubuntu:22.04` + `cmake`, `g++`, `libosrm-dev`, `drogon deps`
  - Volumes: `./backend:/app`, `./osrm-data:/data`
  - Ports: `8080:8080`
- **frontend**:
  - Image: `node:20-alpine`
  - Volumes: `./frontend:/app`
  - Ports: `3000:3000`

---

## 📋 機能要件 & API仕様

### 1. ルート生成 API (`POST /api/v1/route/generate`)

**リクエスト (Input)**
- 出発地点・目的地（周回の場合は同一）
- **Preferences**:
  - 目標距離 (km)
  - 目標獲得標高 (m) ※最重要パラメータ（上限/下限設定も許容誤差として処理）
  - バイクタイプ (Road, Gravel)
- **Timing**:
  - スタート時間（ランチタイム計算に使用）
- **Waypoints**:
  - 寄り道カテゴリ（カフェ, ラーメン, 温泉など）

**レスポンス (Output)**
- 生成されたルートの形状データ (Encoded Polyline)
- ルート概要
  - 総距離、総獲得標高、推定移動時間
- 立ち寄りスポット (Stops)
  - 店舗名、位置、到着予定時刻
  - Google Maps リンク

### 2. データ処理フロー
1. **OSRM Pre-processing**: OSMデータとSRTM（標高データ）を事前に結合し、`.osrm` 形式のグラフデータを生成。
2. **Route Calculation**: `libosrm` の `Table` や `Route` 機能を使い、指定距離・標高を満たすウェイポイントを探索的に配置してルートを構築。
3. **Place Matching**: 確定したルートの通過時刻に基づき、適切なエリアでGoogle Places APIを叩いて休憩場所を提案。

---