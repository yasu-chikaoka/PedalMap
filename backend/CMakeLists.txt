cmake_minimum_required(VERSION 3.10)
project(cycling_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Drogon REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem iostreams thread system)

# Include directories
include_directories(
    /usr/local/include
    /usr/local/include/osrm
    controllers
)

# Add executable
add_executable(cycling_backend
    main.cc
    controllers/RouteController.cc
)

# Link libraries
# Drogon links its dependencies (jsoncpp, uuid, etc)
# osrm, osrm_extract, osrm_contract, osrm_partition, osrm_customize are the usual libs, 
# but for query/routing 'osrm' is the main one.
target_link_libraries(cycling_backend
    PRIVATE
    Drogon::Drogon
    osrm
    ${Boost_LIBRARIES}
    pthread
    z
)