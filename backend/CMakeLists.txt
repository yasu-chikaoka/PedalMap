cmake_minimum_required(VERSION 3.10)
project(cycling_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Find packages
option(BUILD_TESTS_ONLY "Build only tests without backend dependencies" OFF)

if(NOT BUILD_TESTS_ONLY)
    find_package(Drogon REQUIRED)
    find_package(Boost REQUIRED COMPONENTS filesystem iostreams thread system)
endif()

# Include directories
include_directories(
    /usr/local/include
    /usr/local/include/osrm
    controllers
    services
)

if(NOT BUILD_TESTS_ONLY)
    # Add executable
    add_executable(cycling_backend
        main.cc
        controllers/RouteController.cc
        services/RouteService.cc
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
endif()

enable_testing()

add_executable(
  cycling_backend_test
  tests/RouteServiceTest.cc
  services/RouteService.cc
)
target_link_libraries(
  cycling_backend_test
  GTest::gtest_main
)