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
            if (index >= len) break;  // Prevent buffer overrun
            b = encodedPolyline[index++] - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
        lat += dlat;

        shift = 0;
        result = 0;
        do {
            if (index >= len) break;  // Prevent buffer overrun
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

std::string PolylineDecoder::encode(const std::vector<services::Coordinate>& points,
                                    double precision) {
    std::string encodedString;
    int lastLat = 0;
    int lastLng = 0;

    for (const auto& point : points) {
        int lat = static_cast<int>(std::round(point.lat * precision));
        int lng = static_cast<int>(std::round(point.lon * precision));

        int dLat = lat - lastLat;
        int dLng = lng - lastLng;

        // Encode latitude
        int num = dLat << 1;
        if (dLat < 0) {
            num = ~(num);
        }
        while (num >= 0x20) {
            encodedString += static_cast<char>((0x20 | (num & 0x1f)) + 63);
            num >>= 5;
        }
        encodedString += static_cast<char>(num + 63);

        // Encode longitude
        num = dLng << 1;
        if (dLng < 0) {
            num = ~(num);
        }
        while (num >= 0x20) {
            encodedString += static_cast<char>((0x20 | (num & 0x1f)) + 63);
            num >>= 5;
        }
        encodedString += static_cast<char>(num + 63);

        lastLat = lat;
        lastLng = lng;
    }

    return encodedString;
}

}  // namespace utils
