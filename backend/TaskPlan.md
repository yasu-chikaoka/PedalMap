# バックエンド リファクタリング タスク計画

## Phase 1: 準備
- [x] 新しい feature/refactor-backend ブランチを作成する

## Phase 2: 設定と依存関係の分離
- [x] 設定値（OSRMパス等）を管理するConfigクラスを作成する
- [x] OSRMエンジンをカプセル化するOSRMClientクラスを作成する
- [x] DrogonのDI機能を利用してControllerにServiceとClientを注入する

## Phase 3: 機能実装と改善
- [x] SpotServiceにCSVからスポットデータを読み込む機能を実装する
- [-] searchSpotsAlongRoute（ポリライン沿いのスポット検索）を実装する (保留: ポリラインデコード機能が必要なため、現在はPathベースの検索を使用)

## Phase 4: テストの強化
- [x] SpotServiceのテストをCSVデータを使うように更新・拡充する (ConfigService経由でテスト)
- [-] RouteControllerのエンドツーエンドテストを作成する (保留: 統合テスト環境の整備が必要)

## Phase 5: 仕上げ
- [x] 新しく追加したソースファイルをCMakeLists.txtに追加する
- [x] 全体のリファクタリング内容を確認し、不要なコードを削除する