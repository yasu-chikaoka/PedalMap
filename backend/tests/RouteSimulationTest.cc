#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>
#include "../services/RouteService.h"

using namespace services;

class RouteSimulationTest : public ::testing::Test {
protected:
    struct Spot {
        std::string name;
        std::string type;
        double lat;
        double lon;
    };

    std::vector<Spot> loadSpots(const std::string& filename) {
        std::vector<Spot> spots;
        // Search in possible locations
        std::vector<std::string> search_paths = {
            filename,
            "../" + filename,
            "backend/" + filename,
            "/home/user/workspase/cycling/" + filename,
            "tests/data/spots_test.csv"
        };
        
        std::ifstream file;
        for (const auto& path : search_paths) {
            file.open(path);
            if (file.is_open()) {
                std::cout << "[DEBUG] Found spots file at: " << path << std::endl;
                break;
            }
        }

        if (!file.is_open()) {
            std::cerr << "[ERROR] Could not find spots file." << std::endl;
            return spots;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            Spot spot;
            if (!std::getline(ss, spot.name, ',')) continue;
            if (!std::getline(ss, spot.type, ',')) continue;
            if (!std::getline(ss, item, ',')) continue;
            spot.lat = std::stod(item);
            if (!std::getline(ss, item, ',')) continue;
            spot.lon = std::stod(item);
            spots.push_back(spot);
        }
        return spots;
    }

    double calculateDistance(const Coordinate& p1, const Coordinate& p2) {
        double lat1 = p1.lat * M_PI / 180.0;
        double lon1 = p1.lon * M_PI / 180.0;
        double lat2 = p2.lat * M_PI / 180.0;
        double lon2 = p2.lon * M_PI / 180.0;
        double dlat = lat2 - lat1;
        double dlon = lon2 - lon1;
        double a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return 6371.0 * c;
    }
};

TEST_F(RouteSimulationTest, CompareAlgorithms) {
    auto spots = loadSpots("backend/tests/data/spots_test.csv");
    ASSERT_FALSE(spots.empty());

    std::cout << "\n--- Route Generation Algorithm Comparison ---" << std::endl;
    std::cout << std::left << std::setw(20) << "Start" << std::setw(20) << "End" 
              << std::setw(15) << "Target(km)" << std::setw(15) << "Type" 
              << std::setw(15) << "WPs" << std::endl;

    for (size_t i = 0; i < spots.size() && i < 3; ++i) {
        for (size_t j = i + 1; j < spots.size() && j < 4; ++j) {
            Coordinate start{spots[i].lat, spots[i].lon};
            Coordinate end{spots[j].lat, spots[j].lon};
            double straightDist = calculateDistance(start, end);
            double targetDist = straightDist * 1.5;

            if (straightDist < 1.0) continue;

            // Single point
            auto single = RouteService::calculateDetourPoint(start, end, targetDist);
            std::cout << std::left << std::setw(20) << spots[i].name.substr(0, 18) 
                      << std::setw(20) << spots[j].name.substr(0, 18)
                      << std::fixed << std::setprecision(2) 
                      << std::setw(15) << targetDist 
                      << std::setw(15) << "Triangle"
                      << std::setw(15) << (single ? "1" : "0") << std::endl;

            // Polygon (2 points)
            auto poly = RouteService::calculatePolygonDetourPoints(start, end, targetDist);
            std::cout << std::left << std::setw(20) << "" << std::setw(20) << ""
                      << std::setw(15) << "" 
                      << std::setw(15) << "Polygon"
                      << std::setw(15) << poly.size() << std::endl;
        }
    }
}
