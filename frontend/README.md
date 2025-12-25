# Cycling Route Frontend

## 概要 (Overview)

このプロジェクトは、Next.js で構築されたサイクリスト向けのルート生成・可視化Webアプリケーションのフロントエンドです。
Google Maps API を活用し、バックエンドから提供されるルート情報の描画やスポット情報の表示を行います。

## 技術スタック (Technology Stack)

-   **フレームワーク**: Next.js (App Router)
-   **言語**: TypeScript
-   **スタイリング**: Tailwind CSS
-   **地図**: Google Maps JavaScript API (via @vis.gl/react-google-maps)
-   **アイコン**: Lucide React
-   **ドラッグ&ドロップ**: @dnd-kit

## セットアップと実行方法 (Getting Started)

### 1. 環境変数の設定

`.env.local` ファイルを作成し、必要なAPIキーを設定します。

```bash
NEXT_PUBLIC_GOOGLE_MAPS_API_KEY=あなたのGoogleMapsAPIキー
NEXT_PUBLIC_API_ENDPOINT=http://localhost:8080/api/v1
```

### 2. 開発サーバーの起動

#### Dockerを使用する場合 (推奨)

プロジェクトのルートディレクトリで以下を実行します。

```bash
docker compose up frontend
```

#### ローカルのNode.jsを使用する場合

```bash
npm install
npm run dev
```

ブラウザで [http://localhost:3000](http://localhost:3000) にアクセスして確認できます。

## テスト (Testing)

Jest と React Testing Library を使用してテストを実行します。

```bash
npm test
```

## 本番用イメージのビルド (Production Build)

マルチステージビルドを使用して、本番用の軽量なイメージをビルドできます。

```bash
docker build --target runner -t cycling-frontend:latest .
```
