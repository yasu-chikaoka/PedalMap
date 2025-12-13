# バックエンド リファクタリング & 機能拡張計画 (v2)

## 1. 概要
`RouteController` のリファクタリングに加え、スポット検索機能を抜本的に強化します。Boost.Geometry R-tree を用いた空間インデックスを導入し、ルート（LineString）沿いのスポットを高速かつ高精度に検索できる仕組みを構築します。

## 2. RouteController の責務変更
（v1から変更なし、スポット検索呼び出し時にルートのジオメトリを渡す点が追加）

## 3. RouteService の拡張
（v1から変更なし）

## 4. SpotService の高度化（R-tree導入）

### 技術選定
- **Boost.Geometry Index (R-tree)**: C++の標準的な幾何計算ライブラリの一部。空間検索（近傍検索、矩形検索）を高速に行える。
- データ構造: `std::pair<Point, SpotData>` をR-treeに格納。

### SpotService の責務
1.  **データロード**: 起動時にCSVファイル等からスポットデータを読み込み、R-treeを構築する。
2.  **ルート沿い検索**: 
    - 入力: ルートのジオメトリ（Polylineまたは座標点列）、検索半径（バッファ距離）。
    - 処理:
        1. ルートのバウンディングボックス（MBR）を取得し、R-treeで粗いフィルタリングを行う。
        2. フィルタリングされた候補について、ルート（線分集合）との正確な距離を計算する。
        3. 指定距離以内のスポットを抽出する。

### インターフェース案
```cpp
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

// 緯度経度座標系を使用
using Point = bg::model::point<double, 2, bg::cs::geographic<bg::degree>>;
using Value = std::pair<Point, size_t>; // size_t はスポット配列のインデックス

struct Spot {
    size_t id;
    std::string name;
    std::string type;
    double lat;
    double lon;
    double rating;
};

class SpotService {
public:
    SpotService(); // コンストラクタでデータをロードしR-treeを構築

    // 指定されたルート（Polyline文字列）周辺のスポットを検索
    // bufferMeters: ルートからの最大距離(メートル)
    std::vector<Spot> searchSpotsAlongRoute(const std::string& polylineGeometry, double bufferMeters = 500.0);
    
    // (オプション) 座標点列からの検索
    std::vector<Spot> searchSpotsAlongPath(const std::vector<Coordinate>& path, double bufferMeters);

private:
    std::vector<Spot> spots_;
    bgi::rtree<Value, bgi::quadratic<16>> rtree_;
    
    // 内部ヘルパー: Polylineデコードなど
};
```

### 必要な依存ライブラリ
- Boost (Geometry, Range, etc.) - プロジェクトで既にBoostを使用しているため追加導入は不要（CMakeでcomponentsを確認する）。

## 5. テスト戦略
- **SpotServiceTest**:
    - **R-tree構築テスト**: データが正しくインデックスされているか。
    - **距離検索テスト**: 特定の点から半径Xメートル以内のスポットが検索できるか。
    - **ルート沿い検索テスト**: 
        - 線分（A地点→B地点）の近くにあるスポットがヒットするか。
        - 線分から遠いスポットが除外されるか。
        - 複雑な形状（L字型など）のルートでも正しく検索できるか。
- **カバレッジ**:
    - エッジケース（スポットが0件、ルートが空、不正なPolylineなど）を網羅し、90%以上のカバレッジを目指す。

## 6. データソース
- テスト用に `backend/data/spots.csv` を作成し、主要なサイクリングスポットのデータ（約100件程度）を用意する。
