# Cycling Route Frontend

## 概要 (Overview)

このプロジェクトは、Next.js で構築されたサイクリスト向けのルート生成・可視化Webアプリケーションのフロントエンドです。
Google Maps API を活用し、バックエンドから提供されるルート情報の描画、標高グラフの表示、スポット情報の提示を行います。

## 技術スタック (Technology Stack)

- **フレームワーク**: Next.js 15 (App Router)
- **言語**: TypeScript
- **スタイリング**: Tailwind CSS 4
- **地図**: Google Maps JavaScript API (via @vis.gl/react-google-maps)
- **アイコン**: Lucide React
- **ドラッグ&ドロップ**: @dnd-kit (経由地の並び替え)
- **状態管理・Hooks**: React Hooks, SWR/Fetch

## セットアップと実行方法 (Getting Started)

### 1. 環境変数の設定

プロジェクトルートの `.env` ファイル、または `frontend/.env.local` に設定します。

```env
NEXT_PUBLIC_GOOGLE_MAPS_API_KEY=あなたのGoogleMapsAPIキー
NEXT_PUBLIC_API_ENDPOINT=http://localhost:8080/api/v1
```

※ `docker-compose` 経由で起動する場合、ルートディレクトリの `.env` にある `GOOGLE_PLACES_API_KEY` が自動的に `NEXT_PUBLIC_GOOGLE_MAPS_API_KEY` として引き継がれます。

### 2. 開発サーバーの起動

#### Dockerを使用する場合 (推奨)

プロジェクトのルートディレクトリで以下を実行します。

```bash
docker compose up frontend
```

#### ローカルのNode.jsを使用する場合

```bash
cd frontend
npm install
npm run dev
```

ブラウザで [http://localhost:3000](http://localhost:3000) にアクセスしてください。

## 主なコンポーネント

- `ControlPanel`: 目的地入力、ルート設定
- `PlaceAutocomplete`: Google Places を利用した地点検索
- `RoutePolyline`: 地図上へのルート描画
- `RouteStats`: 距離・時間・獲得標高の表示
- `WaypointsList`: 経由地のドラッグ&ドロップ編集

## テスト (Testing)

Jest と React Testing Library を使用してテストを実行します。

```bash
cd frontend
npm test
```

## コードスタイル

Prettier と ESLint を使用しています。

```bash
npm run format
npm run lint
```
