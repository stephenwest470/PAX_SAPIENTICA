cmake_minimum_required(VERSION 3.16)
project(BatchSimulator)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/GraphicsLibraryHelpers.cmake)

# Headless replicate / batch runner for settlement simulation models (no graphics library)
add_executable(PAXS_BatchSimulator Main.cpp)
paxs_setup_common_options(PAXS_BatchSimulator)
paxs_copy_config_tsv(PAXS_BatchSimulator "${CMAKE_CURRENT_SOURCE_DIR}/../MapViewer/Windows/Config.tsv")
