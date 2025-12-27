#pragma once

#include <functional>
#include <optional>
#include <vector>

namespace services {

// RouteService.h に定義されている Coordinate を使用
struct Coordinate;

namespace elevation {

class IElevationProvider {
   public:
    virtual ~IElevationProvider() = default;

    using ElevationCallback = std::function<void(std::optional<double>)>;
    using ElevationsCallback = std::function<void(const std::vector<double>&)>;

    /**
     * @brief 単一地点の標高を取得（非同期）
     *
     * @param coord 座標
     * @param callback 取得完了時に呼ばれるコールバック（失敗時は nullopt）
     */
    virtual void getElevation(const Coordinate& coord, ElevationCallback&& callback) = 0;

    /**
     * @brief 複数地点の標高を一括取得（非同期）
     *
     * @param coords 座標リスト
     * @param callback 取得完了時に呼ばれるコールバック（結果のベクトルを返す。取得失敗箇所は 0.0）
     */
    virtual void getElevations(const std::vector<Coordinate>& coords,
                               ElevationsCallback&& callback) = 0;

    /**
     * @brief 同期的に標高を取得（テストや特定のユースケース用）
     */
    virtual std::optional<double> getElevationSync(const Coordinate& coord) = 0;
};

}  // namespace elevation
}  // namespace services
