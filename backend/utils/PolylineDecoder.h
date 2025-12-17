#pragma once

#include <string>
#include <vector>

#include "../services/RouteService.h" // For Coordinate struct

namespace utils {

class PolylineDecoder {
   public:
    // Decodes an encoded polyline string into a vector of coordinates
    // Precision defaults to 1e5 (5 decimal places), consistent with OSRM and Google Maps
    static std::vector<services::Coordinate> decode(const std::string& encodedPolyline, 
                                                    double precision = 1e5);
};

}  // namespace utils