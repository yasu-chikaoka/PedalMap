#include <gtest/gtest.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

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
            "../backend/" + filename,
            "/home/user/workspase/cycling/" + filename,
            "tests/data/spots_test.csv",
            "/app/tests/data/spots_test.csv",
            "backend/tests/data/spots_test.csv",
            "../backend/tests/data/spots_test.csv",
            "/home/runner/work/PedalMap/PedalMap/backend/tests/data/spots_test.csv"};

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
        double a =
            sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return 6371.0 * c;
    }
};

TEST_F(RouteSimulationTest, CompareAlgorithms) {
    auto spots = loadSpots("backend/tests/data/spots_test.csv");
    ASSERT_FALSE(spots.empty());

    std::cout << "\n--- Route Generation Algorithm Comparison ---" << std::endl;
    std::cout << std::left << std::setw(20) << "Start" << std::setw(20) << "End" << std::setw(15)
              << "Target(km)" << std::setw(15) << "Type" << std::setw(15) << "WPs" << std::endl;

    for (size_t i = 0; i < spots.size() && i < 3; ++i) {
        for (size_t j = i + 1; j < spots.size() && j < 4; ++j) {
            Coordinate start{spots[i].lat, spots[i].lon};
            Coordinate end{spots[j].lat, spots[j].lon};
            double straightDist = calculateDistance(start, end);
            double targetDist = straightDist * 1.5;

            if (straightDist < 1.0) continue;

            // Single point
            auto single = RouteService::calculateDetourPoint(start, end, targetDist);
            std::cout << std::left << std::setw(20) << spots[i].name.substr(0, 18) << std::setw(20)
                      << spots[j].name.substr(0, 18) << std::fixed << std::setprecision(2)
                      << std::setw(15) << targetDist << std::setw(15) << "Triangle" << std::setw(15)
                      << (single ? "1" : "0") << std::endl;

            // New Algorithm (MCSS) Simulation
            std::cout << "  -> Simulating MCSS..." << std::endl;
            auto evaluator = [&](const std::vector<Coordinate>& wps) -> std::optional<RouteResult> {
                // Mock evaluator: Calculate simple distance for simulation
                // In real test, this would call OSRM
                double dist = 0.0;
                Coordinate prev = start;
                for (const auto& wp : wps) {
                    dist += calculateDistance(prev, wp);
                    prev = wp;
                }
                dist += calculateDistance(prev, end);

                // Mock result
                RouteResult res;
                res.distance_m = dist * 1000.0;  // convert to m
                res.elevation_gain_m = 0.0;      // Mock
                res.path = wps;
                return res;
            };

            auto bestRoute = RouteService::findBestRoute(start, end, targetDist, 0, evaluator);

            std::string resultStr = "N/A";
            double resultDist = 0.0;
            if (bestRoute) {
                resultDist = bestRoute->distance_m / 1000.0;
                resultStr = std::to_string(resultDist);
            }

            std::cout << std::left << std::setw(20) << "" << std::setw(20) << "" << std::setw(15)
                      << "" << std::setw(15) << "MCSS (Best)" << std::setw(15) << resultStr
                      << std::endl;
        }
    }
}
