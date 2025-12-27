# ルート生成アルゴリズム改善設計書

## 1. 背景と目的
現在のルート生成アルゴリズムは、単純な幾何学計算（二点間の垂直二等分線上の点を計算）に基づいて迂回ポイントを決定しています。しかし、実際の道路網は直線ではないため、計算上の距離と実際の走行距離に大きな乖離が生じることがあります。また、獲得標高を考慮する仕組みが実装されていません。
本設計では、これらの課題を解決し、**目標距離および獲得標高に対する精度を最大化**するための新しいアルゴリズムを定義します。

## 2. 新アルゴリズム: Multi-Candidate Sampling & Selection (MCSS)

幾何学的な一点予測ではなく、複数の迂回候補点を生成して実際にルーティングエンジン（OSRM）に問い合わせ、その結果から最適なものを選択する「探索的アプローチ」を採用します。

### 2.1 処理フロー

1.  **入力パラメータ**:
    *   Start (始点), End (終点)
    *   TargetDistance (目標距離 km)
    *   TargetElevation (目標獲得標高 m) [Optional]

2.  **候補点生成 (Candidate Generation)**:
    *   始点と終点の中点を中心とし、直線距離を基準とした垂直方向へのオフセットを持つ候補点を複数生成します。
    *   **展開パラメータ**:
        *   `Side`: Left (-1), Right (1)
        *   `ExpansionFactor`: 目標距離を達成するために必要な理論上の「膨らみ」に対し、係数を掛けてバリエーションを持たせます（例: 0.8, 1.0, 1.2, 1.5倍）。
        *   これにより、8〜12パターン程度の候補点（Waypoint）を生成します。

3.  **実ルート評価 (OSRM Probing)**:
    *   生成された各Waypoint経由のルートについて、OSRM `Route` APIを実行します。
    *   レスポンスから以下を取得します。
        *   `ActualDistance`: 実走行距離
        *   `ElevationGain`: 累積獲得標高（※後述の実装詳細あり）

4.  **最適解の選択 (Selection)**:
    *   以下のコスト関数を最小化するルートを選択します。
    *   $$ Cost = W_d \times |D_{target} - D_{actual}| + W_e \times |E_{target} - E_{actual}| $$
    *   ここで、$W_d$（距離重み）、$W_e$（標高重み）は設定により調整可能とします。

## 3. 実装詳細変更点

### 3.1 `RouteResult` 構造体の拡張 (`RouteService.h`)
獲得標高を扱うため、フィールドを追加します。

```cpp
struct RouteResult {
    double distance_m;
    double duration_s;
    double elevation_gain_m; // 追加
    std::string geometry;
    std::vector<Coordinate> path;
};
```

### 3.2 標高データの取得方法 (`RouteService.cc`)
OSRMの標準レスポンスには「獲得標高」フィールドが含まれないため、以下のいずれかの方法で算出します。今回は **Method A** を採用します。

*   **Method A: `annotations=true` を使用 (推奨)**
    *   OSRMリクエスト時に `annotations=true` または `annotations=nodes` を付与。
    *   レスポンスに含まれる各ノードのメタデータから標高を参照できる場合、それを使用。
    *   *注意*: OSRMデータのビルド時にSRTMデータが含まれている必要があります。もし含まれていない場合、Method Bへのフォールバックが必要です。
*   **Method B: 外部標高APIまたは数値地図との照合**
    *   ルートのLat/Lon列に対して、別途標高を取得して積算。

### 3.3 クラス設計の変更

*   `RouteService` に新しいメソッドを追加します。
    *   `findBestRoute(start, end, targetDist, targetElev)`
    *   内部で `generateCandidates()` と `evaluateCandidates()` を実行。

## 4. 評価計画

### 4.1 シミュレーションデータ (`simulation_scenarios.csv`)
様々な距離・地形条件を含むテストケースを作成します。

| ID | Start | End | TargetDist (km) | Note |
|---|---|---|---|---|
| 1 | 東京駅 | 東京駅 | 30 | 都市部・短距離周回 |
| 2 | 高尾山口 | 高尾山口 | 60 | 山間部・中距離周回 |
| 3 | 渋谷 | 横浜 | 50 | 都市間・片道迂回 |
| 4 | 小田原 | 箱根湯本 | 40 | 山岳・短距離 |

### 4.2 評価指標
*   **距離誤差率**: $|(Actual - Target) / Target| \times 100 (\%)$
*   **処理時間**: 全候補の計算にかかる時間（実用的なレスポンスタイムに収まるか）

## 5. 今後の拡張性
*   このアプローチは、将来的に「カフェ経由」「景観優先」などの評価軸をコスト関数に追加することで容易に拡張可能です。
