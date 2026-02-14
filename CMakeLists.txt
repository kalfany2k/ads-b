cmake_minimum_required(VERSION 3.20)
project(ads-b)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find rtl-sdr library using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED librtlsdr)

# Gather all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(ads-b ${SOURCES}
        src/byte_manipulation.cpp
        src/byte_manipulation.h
        src/ads_b_helpers.cpp
        src/ads_b_helpers.h)

# Link rtl-sdr library
target_link_libraries(ads-b ${RTLSDR_LIBRARIES})
target_include_directories(ads-b PRIVATE ${RTLSDR_INCLUDE_DIRS})
target_link_directories(ads-b PRIVATE ${RTLSDR_LIBRARY_DIRS})

# Set output directory
set_target_properties(ads-b PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")