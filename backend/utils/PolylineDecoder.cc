#include "PolylineDecoder.h"

#include <cmath>

namespace utils {

std::vector<services::Coordinate> PolylineDecoder::decode(const std::string& encodedPolyline,
                                                          double precision) {
    std::vector<services::Coordinate> coordinates;
    size_t index = 0;
    size_t len = encodedPolyline.length();
    int lat = 0;
    int lng = 0;

    while (index < len) {
        int b;
        int shift = 0;
        int result = 0;
        do {
            b = encodedPolyline[index++] - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
        lat += dlat;

        shift = 0;
        result = 0;
        do {
            b = encodedPolyline[index++] - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlng = ((result & 1) ? ~(result >> 1) : (result >> 1));
        lng += dlng;

        services::Coordinate coord;
        coord.lat = static_cast<double>(lat) / precision;
        coord.lon = static_cast<double>(lng) / precision;
        coordinates.push_back(coord);
    }

    return coordinates;
}

}  // namespace utils