set(CMAKE_INCLUDE_CURRENT_DIR on)
ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${ODINDATA_INCLUDE_DIRS}
        ${ODINDATA_INCLUDE_DIR}/dpdk
        ${Boost_INCLUDE_DIRS}
        ${LOG4CXX_INCLUDE_DIRS}/..
        ${ZEROMQ_INCLUDE_DIRS}
        ${BLOSC_INCLUDE_DIR}
)

# Add library for XIDynDetectorPlugin
add_library(XIDynDetectorPlugin SHARED
        XIDynPlugin.cpp
)

target_compile_options(XIDynDetectorPlugin PRIVATE ${LIBDPDK_CFLAGS})
target_link_directories(XIDynDetectorPlugin PRIVATE ${ODINDATA_LIBRARY_DIR} ${LIBDPDK_LIBRARY_DIRS})
target_link_libraries(XIDynDetectorPlugin
        PRIVATE OdinDataDpdk rte_eal rte_kvargs rte_telemetry
        ${LIBDPDK_LDFLAGS} ${BLOSC_LIBRARIES}
)

get_target_property(PLUGIN_INSTALL_RPATH XIDynDetectorPlugin INSTALL_RPATH)

set_target_properties(XIDynDetectorPlugin PROPERTIES
        INSTALL_RPATH "${PLUGIN_INSTALL_RPATH}:${LIBDPDK_LIBRARY_DIRS}"
)

install(TARGETS XIDynDetectorPlugin LIBRARY DESTINATION lib)
