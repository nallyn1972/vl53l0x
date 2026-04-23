cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vl53l0x_idf_test_app)
