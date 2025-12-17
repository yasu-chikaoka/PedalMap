#include <gtest/gtest.h>

#include "../services/ConfigService.h"
#include "../services/SpotService.h"

using namespace services;

class SpotServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Set environment variable for test CSV
        // Assume the test is run from the build directory, so we need to point to the source dir
        // Or better, use an absolute path or relative to where we expect to be.
        // In CMake/CTest environment, usually CWD is build dir.
        // Let's assume the data file is copied or we use relative path from source root.
        
        // Use absolute path in the container
        setenv("SPOTS_CSV_PATH", "/app/tests/data/spots_test.csv", 1);
        
        // Re-initialize services after setting env var
        configService = std::make_unique<ConfigService>();
        spotService = std::make_unique<SpotService>(*configService);
    }

    void TearDown() override {
        unsetenv("SPOTS_CSV_PATH");
    }

    std::unique_ptr<ConfigService> configService;
    std::unique_ptr<SpotService> spotService;
};

TEST_F(SpotServiceTest, SearchSpotsAlongPath_Simple) {
    // A path from Tokyo Station to Akihabara
    std::vector<Coordinate> path = {
        {35.681236, 139.767125},  // Tokyo Station
        {35.698383, 139.773072}   // Akihabara
    };

    // Search within 1km radius
    auto spots = spotService->searchSpotsAlongPath(path, 1000.0);

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

    auto spots = spotService->searchSpotsAlongPath(path, 1000.0);

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
    auto spots = spotService->searchSpotsAlongPath(path, 1500.0);

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

TEST_F(SpotServiceTest, SearchSpotsAlongRoute_EncodedPolyline) {
    // A path passing through Tokyo Station
    // Encoded polyline for a segment near Tokyo Station
    // Points: (35.6800, 139.7600) -> (35.6850, 139.7700)
    // "Cycling Cafe Base" is at 35.681236, 139.767125, which should be close to this line.
    
    // Encoding (35.6800, 139.7600), (35.6850, 139.7700)
    // 35.6800, 139.7600 -> _ibE_seK
    // Delta: +0.0050, +0.0100 -> +500, +1000
    // 500 << 1 = 1000 = 1111101000_2
    // 11101000 -> 101000(40) -> 101000 + 63 = 103('g')
    // 111 -> 000111(7) -> 7+63=70('F')
    // -> _ibE_seK_gF_qJ (Approximate manual check, let's use a generated string for reliability)
    
    // Let's use a simpler known string or just constructed coordinates to be safe.
    // Or we can rely on the fact that we've tested PolylineDecoder separately,
    // and here we just test that SpotService correctly calls it and filters.
    
    // Let's mock the string with one that we know decodes to the path in Simple test:
    // (35.681236, 139.767125) -> (35.698383, 139.773072)
    // This is hard to encode manually perfectly.
    
    // Alternative: Use a simple integer-coordinate path for testing logic?
    // No, SpotService uses real lat/lon.
    
    // Let's use the decoder to generate the string first? No, decoder is decode-only.
    // I will use a pre-calculated string for: (35.68, 139.76) -> (35.70, 139.78)
    // This passes near Tokyo Station (35.681236, 139.767125) and Akihabara (35.698383, 139.773072)
    // Polyline: "_ibE_seK_}hI_}hR" (approximate)
    
    // Valid string for (35.68, 139.76) to (35.70, 139.78):
    // 35.68000, 139.76000
    // 35.70000, 139.78000
    // String: "_ibE_seK_qiF_qoJ"
    // 35.68000 -> 3568000
    // 139.76000 -> 13976000
    // 35.70000 - 35.68000 = 2000
    // 139.78000 - 139.76000 = 2000
    
    // Let's try to verify this string manually or trust it.
    // If decoding fails/is off, the test might fail gracefully (no spots found).
    // Let's use the exact coordinates of the spots to ensure match if we can't easily encode.
    // Actually, I can add an Encode function to utility for testing?
    // Or just manually encode:
    // Point 1: 35.68124, 139.76713 (Tokyo Station approx)
    // Point 2: 35.69838, 139.77307 (Akihabara approx)
    
    // 35.68124 -> 3568124. 139.76713 -> 13976713
    // _kbE_~eK  <-- likely wrong manually.
    
    // Let's use the string from the PolylineDecoderTest which we know decodes to (38.5, -120.2)
    // and add a dummy spot there for this test case?
    // But SpotService loads hardcoded spots around Tokyo.
    
    // OK, let's use a very simple vertical line passing through Tokyo Station.
    // Spot: 35.681236, 139.767125
    // Line: (35.68000, 139.76713) -> (35.69000, 139.76713)
    // Lat change: +0.01000 (+1000). Lon change: 0.
    // Start: 35.68000, 139.76713
    // 35.68000 -> 3568000 -> 1101100110111000000000 (binary) ... hard.
    
    // Easier approach: Use an online tool to get a polyline for "Tokyo Station to Akihabara"
    // and use that string.
    // Tokyo Station: 35.681236, 139.767125
    // Akihabara: 35.698383, 139.773072
    // OSRM/Google encoded polyline (approx):
    // "{kbEua}K_@s@u@e@k@g@i@e@c@a@_@??" (Just a guess? No.)
    
    // Let's try a string that is KNOWN to be valid and verify if it finds anything.
    // If not, we can rely on `searchSpotsAlongPath` test and unit test for decoder.
    // But integration test is better.
    
    // I will use this simple string: "_ibE_seK_qiF_qoJ"
    // Use a known polyline string from PolylineDecoderTest that decodes to (38.5, -120.2)
    // and we have added "Polyline Test Spot" at this location in spots_test.csv.
    std::string polyline = "_p~iF~ps|U";
    // Decodes to (38.5, -120.2)
    
    auto spots = spotService->searchSpotsAlongRoute(polyline, 100.0);
    
    bool foundTestSpot = false;
    for (const auto& spot : spots) {
        if (spot.name == "Polyline Test Spot") foundTestSpot = true;
    }
    
    EXPECT_TRUE(foundTestSpot) << "Should find Polyline Test Spot";
}