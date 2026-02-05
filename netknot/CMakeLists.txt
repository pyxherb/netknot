find_package(peff REQUIRED)

file(GLOB HEADERS *.h)
file(GLOB SRC *.cc)

add_library(netknot SHARED)
target_compile_definitions(netknot PRIVATE IS_NETKNOT_BUILDING=1)
set_target_properties(netknot PROPERTIES CXX_STANDARD 17)
target_sources(netknot PRIVATE ${HEADERS} ${SRC})
target_include_directories(netknot PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/> $<INSTALL_INTERFACE:include/>)
target_link_libraries(netknot PUBLIC peff_base peff_utils peff_containers)
set_target_properties(netknot PROPERTIES
    PUBLIC_HEADER "${HEADERS}"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    ARCHIVE FALSE
)

add_library(netknot_static STATIC)
target_compile_definitions(netknot_static PRIVATE IS_NETKNOT_BUILDING=1)
target_compile_definitions(netknot_static PUBLIC NETKNOT_STATIC_LINK=1)
set_target_properties(netknot_static PROPERTIES CXX_STANDARD 17)
target_sources(netknot_static PRIVATE ${HEADERS} ${SRC})
target_link_libraries(netknot_static PUBLIC peff_base_static peff_utils_static peff_containers_static)
target_include_directories(netknot_static PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/> $<INSTALL_INTERFACE:include/>)
set_target_properties(netknot_static PROPERTIES
    PUBLIC_HEADER "${HEADERS}"
    VERSION ${PROJECT_VERSION}
)

if(WIN32)
    add_subdirectory("win")
elseif(UNIX)
    add_subdirectory("unix")
endif()

install(TARGETS netknot netknot_static
    EXPORT netknotTargets
    LIBRARY DESTINATION lib/
    ARCHIVE DESTINATION lib/
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/netknot/
)
