#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <string>
#include <vector>

#include "RouteService.h"  // For Coordinate struct
#include "ConfigService.h"

namespace services {

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

// Define point using geographic coordinate system (lat, lon in degrees)
using Point = bg::model::point<double, 2, bg::cs::geographic<bg::degree>>;
using Value = std::pair<Point, size_t>;  // Point and Index in spots_ vector

struct Spot {
    std::string name;
    std::string type;
    double lat;
    double lon;
    double rating;
};

class SpotService {
   public:
    explicit SpotService(const ConfigService& configService);

    // Search spots along a route (polyline)
    // polylineGeometry: Encoded polyline string (not implemented yet, taking path of coordinates)
    // For now, we simulate "along route" by buffering around points or simple path logic.
    // To properly support "along route", we need to convert polyline to LineString and use
    // buffer/distance.
    std::vector<Spot> searchSpotsAlongPath(const std::vector<Coordinate>& path,
                                           double bufferMeters);
    
    // Search spots along route geometry (polyline)
    std::vector<Spot> searchSpotsAlongRoute(const std::string& polylineGeometry, double bufferMeters);

   private:
    std::vector<Spot> spots_;
    bgi::rtree<Value, bgi::quadratic<16>> rtree_;

    void loadSpots(); // Load dummy data (fallback)
    void loadSpotsFromCsv(const std::string& filePath);
};

}  // namespace services