cmake_minimum_required(VERSION 3.13)
project(netknot VERSION 0.1.0)

add_subdirectory("netknot")
add_subdirectory("example")

# Generate the version file for the config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/netknotConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMinorVersion
)

# Create config file
configure_package_config_file(
    cmake/netknotConfig.cmake.in netknotConfig.cmake
    INSTALL_DESTINATION lib/cmake/netknot
)

# Install config files
install(
    FILES
       ${CMAKE_CURRENT_BINARY_DIR}/netknotConfig.cmake
       ${CMAKE_CURRENT_BINARY_DIR}/netknotConfigVersion.cmake
    DESTINATION
        lib/cmake/${PROJECT_NAME}
)

# Exporting Targets from the Build Tree
install(EXPORT netknotTargets
    FILE netknotTargets.cmake
    DESTINATION lib/cmake/${PROJECT_NAME}
)
