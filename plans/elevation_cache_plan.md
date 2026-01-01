# 標高データキャッシュ・パフォーマンス改善計画

## 1. 概要
標高データの取得パフォーマンスを最大化し、かつキャッシュデータの肥大化を防ぐため、**Redis** を導入して多層キャッシュシステムを構築します。さらに、データの鮮度維持とAPI負荷制御を両立させるスマートリフレッシュ機構を実装します。

## 2. システムアーキテクチャ

### 2.1. キャッシュ階層
1.  **L1 キャッシュ (Local Memory)**
    *   **実装**: 自作 `LRUCache` クラス (スレッドセーフ)
    *   **役割**: プロセス内での超高速アクセス。
    *   **容量**: タイル数制限 (例: 1000個) によりメモリ肥大化を防止。
    *   **TTL**: なし (LRUによる追い出しのみ)。

2.  **L2 キャッシュ (Redis)**
    *   **役割**: 永続化、大容量キャッシュ、プロセス間共有。
    *   **TTL**: デフォルト **365日** (環境変数 `ELEVATION_CACHE_TTL_DAYS` で設定可能)。
    *   **管理**: `allkeys-lru` ポリシーによる自動容量管理。

3.  **データソース (国土地理院 API)**
    *   **役割**: データのマスター。
    *   **アクセス制御**: キャッシュミス時およびリフレッシュ時のみアクセス。

## 3. 機能詳細

### 3.1. データ構造設計 (Redis)
*   **標高データ**:
    *   **Type**: **Hash** (メタデータ管理のため String から変更)
    *   **Key**: `cycling:elevation:v1:data:{z}:{x}:{y}`
    *   **Fields**:
        *   `content`: CSV文字列 (実データ)
        *   `updated_at`: Unix Timestamp (最終更新日時、リフレッシュ判定用)
    *   **TTL**: 365日 (更新時リセット)
*   **アクセス統計 (Access Rank)**:
    *   Key: `cycling:elevation:v1:stats:rank` (Sorted Set)
    *   Member: `{z}:{x}:{y}`
    *   Score: アクセス頻度スコア
*   **リフレッシュキュー**:
    *   Key: `cycling:elevation:v1:queue:refresh` (Set)
    *   Value: `{z}:{x}:{y}`

### 3.2. キャッシュロジック (`GSIElevationProvider`)
1.  **Get Flow**:
    *   L1 hit -> Return
    *   L2 hit (Redis `HGETALL`) ->
        *   L1 に `content` を保存 -> Return `content`
        *   **非同期処理**:
            *   アクセス統計更新 (`ZINCRBY`)
            *   Lazy Check: `updated_at` が3ヶ月以上前 & スコア高 -> リフレッシュキューへ `SADD`
    *   Miss ->
        *   API Fetch ->
        *   L1 保存 & L2 保存 (`HSET` content, updated_at) -> Return

2.  **Smart Refresh Strategy (分散型)**:
    *   **Lazy Check**: キャッシュヒット時、メタデータ(最終更新等)を確認。
        *   条件: (最終更新 > 3ヶ月前) AND (Access Rank > 閾値)
        *   Action: 対象タイルを `elevation:refresh_queue` に Push。
    *   **Background Worker**:
        *   常駐スレッドまたは定期タスクが Queue からタイルを取り出す。
        *   API から最新データを取得し、L2 キャッシュを更新 (TTLリセット)。
        *   API リクエスト間隔を制御し、負荷を平準化。

3.  **Decay Algorithm (スコア減衰) の具体化**:
    *   **目的**: 過去に頻繁にアクセスされたが、最近アクセスされなくなったデータのスコアを下げ、リフレッシュ対象から除外する。
    *   **実装方法**: Redis の Lua スクリプトを使用し、アトミックに全メンバのスコアを更新する。
    *   **実行トリガー**: Background Worker が定期的に (例: 1日1回) 実行。
    *   **計算式**: `NewScore = OldScore * DecayFactor`
        *   `DecayFactor`: 例として **0.95** (毎日5%減衰)。
        *   これにより、1ヶ月アクセスがないとスコアは約20% (`0.95^30 ≈ 0.21`) にまで低下する。
    *   **Lua Script**:
        ```lua
        local key = KEYS[1]
        local factor = ARGV[1]
        local members = redis.call('ZRANGE', key, 0, -1, 'WITHSCORES')
        for i = 1, #members, 2 do
            local member = members[i]
            local score = members[i+1]
            redis.call('ZADD', key, score * factor, member)
        end
        ```
        *   ※データ量が多い場合はカーソル (`ZSCAN`) を使ったバッチ処理に分割してRedisブロックを回避する。

## 4. UX向上 (Frontend)
*   **メッセージ**: 「日本国内のサイクリングに特化した地形データを学習中！たくさん検索するとデータが蓄積され、次回からよりスムーズになります。」
*   **表示**: ルート生成画面のヘルプ/ヒントとして表示。

## 5. 開発プロセス
*   **ブランチ戦略**: 作業開始前に `feature/elevation-cache-redis` ブランチを作成し、このブランチで作業を行います。
*   **コミット粒度**: 実装の論理的な区切り（例: インフラ設定完了、L1キャッシュ実装完了、など）ごとに git commit を行います。
*   **CI確認**: 実装完了後、`act` ツールを使用してローカルで `.github/workflows/ci.yml` および `.github/workflows/security.yml` を実行し、全てのテストとセキュリティスキャンが通過することを確認します。

## 6. 詳細設計・検討事項 (Refinement Points)
実装フェーズでの手戻りを防ぐため、以下のポイントについて具体的に決定・考慮します。

### 6.1. Redis データ構造とメモリ管理
*   **圧縮**: 標高CSVデータはテキスト形式であり、gzip圧縮等を行えばサイズを大幅に削減（1/3〜1/5程度）できる可能性があります。Redisのメモリコスト削減のため、**圧縮して保存**するかどうかを検討します（今回はパース負荷とのトレードオフで非圧縮スタートとし、将来的な改善候補とします）。
*   **Key Prefix**: 他の機能でRedisを使う可能性を考慮し、ネームスペース設計を確定します。
    *   `cycling:elevation:v1:data:{z}:{x}:{y}`
    *   `cycling:elevation:v1:stats:rank`
    *   `cycling:elevation:v1:queue:refresh`

### 6.2. エラーハンドリングと障害対策
*   **Redis ダウン時**:
    *   `GSIElevationProvider` 内で Redis 操作（Get/Set）が例外またはエラーを返した場合、直ちにログ (`LOG_ERROR`) を出力し、処理を継続します。
    *   Get 失敗時: キャッシュミス扱いとして API 取得フローへ進む (No Cache モード)。
    *   Set 失敗時: 無視してメモリキャッシュのみ更新する。
    *   これにより、Redis が単一障害点 (SPOF) になることを防ぎます。
*   **API エラー時 (リフレッシュワーカー)**:
    *   国土地理院APIが `429 Too Many Requests` や `5xx Server Error` を返した場合、**Exponential Backoff** を適用します。
        *   リトライ待機時間: `base_delay * 2^retry_count` (例: 1s, 2s, 4s...)
        *   最大リトライ回数: 3回。
    *   それでも失敗した場合、そのタスクをキューの末尾に戻す（Dead Letter Queue としての再利用）か、ログを出力して破棄します。今回はシンプルに「ログ出力して破棄（次回アクセス時に再判定）」とします。

### 6.3. 同時実行制御 (Race Conditions)
*   **Cache Stampede (Thundering Herd) 対策**:
    *   同一タイルに対する複数の同時リクエストが発生した場合、全員が同時にAPIを叩きに行くと非効率です。
    *   **対策**: `GSIElevationProvider` 内に `std::mutex` で保護された `std::unordered_map<string, std::shared_future<...>>` (In-flight Requests Map) を導入します。
    *   同じタイルIDへのリクエストが処理中の場合、後続のリクエストはその完了 (`future`) を待ち合わせるようにし、APIリクエストを1回に集約します。
*   **リフレッシュ重複**:
    *   リフレッシュキューへの追加時に、Redis の `SADD` (Set Add) を利用します。`SADD` はメンバが既に存在すれば 0 を返すため、これをチェックすることで重複追加をアトミックに防げます。

### 6.4. パラメータ調整
*   **Access Score Threshold**:
    *   リフレッシュ対象とする「高頻度」の閾値。
    *   初期値: **10** (過去1週間に10回以上アクセスがあった等、減衰後のスコア)。
    *   運用後は、全体のアクセスの **上位 10%** のスコアを動的に計算して閾値とするロジックへの移行を検討しますが、初期フェーズでは固定値で運用します。
*   **Worker Rate Limit**:
    *   国土地理院サーバーへの負荷配慮。
    *   設定: **1秒あたり 1 リクエスト (1 QPS)**。
    *   実装: `std::this_thread::sleep_for` を用いて、各APIリクエスト後に最低 1000ms のウェイトを挿入します。

### 6.5. クラス設計と責務の分離 (疎結合設計)
各機能が密結合にならないよう、インターフェースを通じて依存関係を管理し、単体テストと交換可能性を担保します。

1.  **`ElevationCacheManager` クラス (新規作成)**
    *   **責務**: キャッシュ戦略の統括 (L1 -> L2 -> Fallback のフロー制御)。
    *   **依存**: `ILruCache`, `IRedisClient` (Drogonラッパー), `IElevationProvider` (API呼び出し用)
    *   `GSIElevationProvider` はデータフェッチ（API通信）のみに専念させ、キャッシュロジックをこのマネージャに委譲します。
    *   **メソッド**: `getElevation(coord)`, `refreshTile(tileKey)`

2.  **`RedisElevationAdapter` クラス (新規作成)**
    *   **責務**: Redis との具体的な通信詳細 (Key生成、シリアライズ/デシリアライズ、TTL設定、エラーハンドリング) の隠蔽。
    *   **インターフェース**: `IElevationCacheRepository`
    *   これにより、将来 Redis から他の KVS に変更する場合も Adapter の差し替えのみで対応可能にします。

3.  **`SmartRefreshService` クラス (新規作成)**
    *   **責務**: アクセス統計の更新 (Lazy Check)、リフレッシュキューの管理、Background Worker の実行。
    *   **依存**: `IElevationCacheRepository`, `IElevationProvider`
    *   `GSIElevationProvider` とは独立して動作し、メインのルート生成ロジックに影響を与えません。

### 6.6. バックグラウンドワーカーのライフサイクルとCI
*   **Graceful Shutdown**:
    *   `SmartRefreshService` 内のワーカー（スレッド）は、アプリケーション終了時（シグナル受信時など）に処理中のタスクを完了してから安全に停止するように実装します (`std::atomic<bool> running_` フラグなどを使用)。
*   **CI環境 (`.github/workflows/ci.yml`)**:
    *   統合テストで Redis を使用するため、Github Actions の `services` セクションに `redis` コンテナの定義を追加する必要があります。これを実装時のタスクに含めます。

## 7. 実装手順 (Todo)

### Phase 1: インフラ & ベース実装 (Model: Flash)
- [ ] `docker-compose.yml` に Redis 追加 & `backend/Dockerfile` に依存ライブラリ追加
- [ ] `ConfigService` に Redis 設定 & TTL設定追加
- [ ] `LruCache` クラス実装 & 単体テスト
- [ ] `backend/main.cc` で Redis クライアント初期化
- [ ] **新規**: `IElevationCacheRepository` インターフェースと `RedisElevationAdapter` の実装

### Phase 2: キャッシュロジック実装 (Model: Pro)
**注意: このフェーズの実装は複雑かつ重要であるため、モデルを `gemini-2.0-pro-exp` に切り替えて実施してください。**
- [ ] **変更**: `ElevationCacheManager` を実装し、キャッシュ階層ロジックを集約
- [ ] `GSIElevationProvider` を `ElevationCacheManager` を使用するようにリファクタリング
- [ ] `SmartRefreshService` の実装 (アクセス統計更新 & Lazy Check)
- [ ] Background Refresh Worker 実装 (APIレートリミット、エラーハンドリング含む)

### Phase 3: テスト & UX (Model: Flash)
**注意: 実装完了後、モデルを `gemini-2.0-flash-thinking-exp` に戻して実施してください。**
- [ ] 単体テスト (`ElevationCacheManager`, `RedisElevationAdapter`, `SmartRefreshService`)
- [ ] 統合テスト (Redis連携、リカバリ試験)
- [ ] フロントエンドへのメッセージ追加
- [ ] 動作確認 & CI通過確認 (act)

## 9. 必須テストコードリスト (Required Tests)
TDD (Test-Driven Development) または実装と並行して作成すべき、必須のテストコード一覧です。

### 9.1. 単体テスト (Unit Tests)
*   **`tests/LruCacheTest.cc`**
    *   **正常系**:
        *   `insert_and_get`: データを挿入し、正しいキーで取得できること。
        *   `update_existing`: 既存キーの値を更新し、新しい値が取得できること。
    *   **境界値・異常系**:
        *   `capacity_limit_eviction`: 容量限界(N個)まで挿入後、N+1個目を挿入した際、**最も古くアクセスされた(Least Recently Used)** 要素が削除されること。
        *   `access_order_update`: 既存要素にアクセス(Get)した後、それが「最新」とみなされ、Eviction対象から外れること。
        *   `empty_get`: 空のキャッシュ、または存在しないキーへのアクセスで `nullopt` (または空ポインタ) が返ること。
    *   **並行性**:
        *   `thread_safety`: 複数のスレッドから同時に Insert/Get を繰り返してもデータ破損やクラッシュが発生しないこと。

*   **`tests/ElevationCacheManagerTest.cc`** (Mock使用)
    *   **キャッシュフロー網羅**:
        *   `hit_l1`: L1キャッシュにデータがある場合、L2/API呼び出しが発生せず、即座に値が返ること。
        *   `miss_l1_hit_l2`: L1ミス・L2ヒット時、L1への書き戻し(Put)が発生し、値が返ること。
        *   `miss_all_api_success`: 全てミス時、APIが呼ばれ、その結果がL1とL2両方に保存されること。
        *   `miss_all_api_failure`: APIも失敗した場合、適切なエラーまたは空値が返り、キャッシュへの保存が行われないこと。
    *   **障害対応**:
        *   `redis_down_fallback`: Redis操作で例外が発生してもクラッシュせず、API取得フローへフォールバックすること。
        *   `api_timeout`: API呼び出しがタイムアウトした場合の挙動確認。

*   **`tests/SmartRefreshServiceTest.cc`**
    *   **判定ロジック**:
        *   `should_refresh_true`: 「最終更新から3ヶ月以上経過」かつ「アクセススコア > 閾値」の場合に True となること。
        *   `should_refresh_false_recent`: 「最終更新がついさっき」の場合は False。
        *   `should_refresh_false_low_score`: 「スコアが低い」場合は False。
    *   **スコア計算**:
        *   `increment_score`: アクセスごとにスコアが正しく加算されること。
        *   `decay_algorithm`: 減衰処理実行後、スコアが指定の係数(例: 0.95)で減少していること。小数点以下の扱いや0への収束確認。

### 9.2. 統合テスト (Integration Tests)
*   **`tests/integration/RedisIntegrationTest.cc`**
    *   **接続・基本操作**:
        *   `connection_retry`: 起動直後のRedis接続確立待ちが機能するか。
        *   `binary_safety`: 任意のバイト列（バイナリデータ）を含む値を保存・復元できるか（圧縮データ対応の布石）。
    *   **TTL動作**:
        *   `ttl_expiration`: 短いTTL(例: 1秒)を設定し、期限切れ後にキーが消滅していること。
    *   **Luaスクリプト**:
        *   `script_atomicity`: スコア減衰スクリプト実行中に、他のクライアントからの干渉を受けないか（概念実証）。
        *   `large_dataset_scan`: 大量のキーがある状態でスクリプトを実行してもエラーにならないか（ZSCANの動作確認）。

## 10. 動作確認手順 (Deployment & Verification)
実装後の動作確認は **Debugモード** に切り替えて実施します。以下の手順に従ってください。

1.  **モード切り替え**:
    *   `switch_mode` ツールを使用し、モードを `debug` に変更します。

2.  **コンテナ再ビルドと起動 (Debugモード)**:
    *   `docker-compose down`
    *   `docker-compose build --no-cache backend`
    *   `docker-compose up -d`
    *   必要に応じてバックエンドコンテナ内でビルドを実行 (`cmake` & `make`) します。

3.  **Redis 接続確認**:
    *   `docker-compose logs -f backend` で接続成功ログを確認します。
    *   必要に応じて `redis-cli` で直接接続確認を行います。

4.  **動作確認シナリオ (Debugモード)**:
    *   **シナリオ1: 初回アクセス (Cache Miss)**
        *   ブラウザまたは `curl` でルート生成APIを叩く。
        *   ログで「APIからの取得」が行われたことを確認。
        *   Redis にキーが作成されたことを確認 (`KEYS *`)。
    *   **シナリオ2: 2回目アクセス (Cache Hit)**
        *   同じリクエストを送信。
        *   ログで「L1/L2キャッシュヒット」を確認。
        *   レスポンスタイムが短縮されていることを確認。
    *   **シナリオ3: 障害試験 (Redis Down)**
        *   `docker-compose stop redis` を実行。
        *   リクエストを送信し、エラーにならずAPIからデータを取得できるか確認 (フォールバック)。
        *   `docker-compose start redis` で復旧し、再接続されるか確認。
