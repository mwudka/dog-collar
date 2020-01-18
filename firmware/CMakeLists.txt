cmake_minimum_required(VERSION 3.8.2)

# This sample runs as a non-secure application on nRF91. Therefore, it
# requires the secure_partition_manager that prepares the required
# peripherals to be available for the application.
#
# Configure the SPM image to enable the peripherals that this sample
# needs.
set(spm_CONF_FILE
  prj.conf
  ${CMAKE_CURRENT_LIST_DIR}/child_secure_partition_manager.conf
  )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/nanopb/extra")
find_package(Nanopb REQUIRED)

include_directories(${NANOPB_INCLUDE_DIRS})

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH ../proto
  ../proto/dogcollar.proto )
  
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS}
    PROPERTIES GENERATED TRUE)

include($ENV{ZEPHYR_BASE}/../nrf/cmake/boilerplate.cmake NO_POLICY_SCOPE)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(dog-collar)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
# NORDIC SDK APP END
zephyr_include_directories(src)

target_sources(app PRIVATE 
  src/ble.c
  src/gps.c
  src/battery_monitor.c
  src/activity_tracker.c
  src/server_upload.c
  src/vendor/croaring/roaring.c
  ${PROTO_SRCS} ${PROTO_HDRS}
)