cmake_minimum_required(VERSION 3.16)

set(SDKCONFIG_DEFAULTS "config/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(m5sticks3_bluetooth_mic)
