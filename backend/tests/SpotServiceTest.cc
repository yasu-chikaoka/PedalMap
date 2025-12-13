#include <gtest/gtest.h>

#include "../services/SpotService.h"

using namespace services;

class SpotServiceTest : public ::testing::Test {
   protected:
    SpotService spotService;
};

TEST_F(SpotServiceTest, SearchSpotsAlongPath_Simple) {
    // A path from Tokyo Station to Akihabara
    std::vector<Coordinate> path = {
        {35.681236, 139.767125},  // Tokyo Station
        {35.698383, 139.773072}   // Akihabara
    };

    // Search within 1km radius
    auto spots = spotService.searchSpotsAlongPath(path, 1000.0);

    // Expect to find "Cycling Cafe Base" (Tokyo Sta) and "Ramen Energy" (Akihabara)
    ASSERT_GE(spots.size(), 2);

    bool foundCafe = false;
    bool foundRamen = false;
    for (const auto& spot : spots) {
        if (spot.name == "Cycling Cafe Base") foundCafe = true;
        if (spot.name == "Ramen Energy") foundRamen = true;
    }

    EXPECT_TRUE(foundCafe);
    EXPECT_TRUE(foundRamen);
}

TEST_F(SpotServiceTest, SearchSpotsAlongPath_NoSpotsNearby) {
    // A path far from any registered spots
    std::vector<Coordinate> path = {{40.0, 135.0}, {40.1, 135.1}};

    auto spots = spotService.searchSpotsAlongPath(path, 1000.0);

    EXPECT_TRUE(spots.empty());
}

TEST_F(SpotServiceTest, SearchSpotsAlongPath_ComplexPath) {
    // Tokyo -> Akihabara -> Skytree -> Ueno
    std::vector<Coordinate> path = {
        {35.681236, 139.767125},  // Tokyo Station
        {35.698383, 139.773072},  // Akihabara
        {35.710063, 139.810700},  // Skytree
        {35.714074, 139.774109}   // Ueno
    };

    // Search within 1.5km
    auto spots = spotService.searchSpotsAlongPath(path, 1500.0);

    // Expect to find Tokyo, Akihabara, Skytree, and Ueno spots
    ASSERT_GE(spots.size(), 4);

    std::set<std::string> foundNames;
    for (const auto& spot : spots) {
        foundNames.insert(spot.name);
    }

    EXPECT_TRUE(foundNames.count("Cycling Cafe Base"));
    EXPECT_TRUE(foundNames.count("Ramen Energy"));
    EXPECT_TRUE(foundNames.count("Skytree View"));
    EXPECT_TRUE(foundNames.count("Ueno Park Cafe"));
}