cmake_minimum_required(VERSION 3.10)
project(cycling_backend CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# カバレッジフラグの追加
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Windows用: 親プロジェクトのコンパイラ/リンカー設定の上書きを防止
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# パッケージの検索
option(BUILD_TESTS_ONLY "Build only tests without backend dependencies" OFF)

find_package(Boost REQUIRED COMPONENTS filesystem iostreams thread system)
find_package(Drogon REQUIRED)

# if(NOT BUILD_TESTS_ONLY)
#     find_package(Drogon REQUIRED)
# endif()


# インクルードディレクトリ
include_directories(
    /usr/local/include
    /usr/local/include/osrm
    /usr/include/jsoncpp
    .
    controllers
    services
)

if(NOT BUILD_TESTS_ONLY)
    # 実行可能ファイルの追加
    add_executable(cycling_backend
        main.cc
        controllers/RouteController.cc
        services/ConfigService.cc
        services/OSRMClient.cc
        services/RouteService.cc
        services/SpotService.cc
        services/elevation/GSIElevationProvider.cc
        utils/PolylineDecoder.cc
    )

    # ライブラリのリンク
    # Drogonはその依存関係（jsoncpp, uuidなど）をリンクします
    # osrm, osrm_extract, osrm_contract, osrm_partition, osrm_customize が通常のライブラリですが、
    # クエリ/ルーティングには 'osrm' がメインです。
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
  tests/OSRMIntegrationTest.cc
  tests/SpotServiceTest.cc
  tests/PolylineDecoderTest.cc
  tests/RouteControllerTest.cc
  tests/RouteSimulationTest.cc
  tests/GSIElevationProviderTest.cc
  services/ConfigService.cc
  services/OSRMClient.cc
  services/RouteService.cc
  services/SpotService.cc
  services/elevation/GSIElevationProvider.cc
  utils/PolylineDecoder.cc
  controllers/RouteController.cc
)
target_link_libraries(
  cycling_backend_test
  GTest::gtest_main
  osrm
  Drogon::Drogon
  ${Boost_LIBRARIES}
  Boost::thread
  Boost::filesystem

  jsoncpp
  pthread
  z
)

include(GoogleTest)
gtest_discover_tests(cycling_backend_test)