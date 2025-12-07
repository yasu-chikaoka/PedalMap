#pragma once

#include <utility>
#include <optional>

namespace services {

struct Coordinate {
    double lat;
    double lon;
};

class RouteService {
public:
    static std::optional<Coordinate> calculateDetourPoint(
        const Coordinate& start, 
        const Coordinate& end, 
        double targetDistanceKm
    );
};

} // namespace services