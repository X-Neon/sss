cmake_minimum_required(VERSION 3.0)
project(SSS)

find_package(Catch2 CONFIG REQUIRED)
find_package(Boost REQUIRED)

add_executable(
    sss_test 
    test/round_trip.cpp
)

target_include_directories(sss_test PRIVATE include)
target_link_libraries(sss_test PRIVATE Catch2::Catch2WithMain Boost::boost)
target_compile_features(sss_test PRIVATE cxx_std_20)

enable_testing()
add_test(NAME sss_test COMMAND sss_test)