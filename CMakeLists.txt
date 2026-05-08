cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/config/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(m5sticks3_bluetooth_mic)
