cmake_minimum_required(VERSION 3.16)
project(pcge_peru VERSION 0.1.0 LANGUAGES C)
include(GNUInstallDirs)
include(CTest)
add_library(pcge STATIC src/pcge.c)
add_library(pcge::pcge ALIAS pcge)
set_target_properties(pcge PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(pcge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_executable(pcge_cli examples/pcge.c)
set_target_properties(pcge_cli PROPERTIES OUTPUT_NAME pcge C_STANDARD 99 C_STANDARD_REQUIRED YES)
target_link_libraries(pcge_cli PRIVATE pcge)
if(BUILD_TESTING)
    add_executable(test_pcge tests/test_pcge.c)
    target_link_libraries(test_pcge PRIVATE pcge)
    set_target_properties(test_pcge PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
    add_test(NAME pcge_api COMMAND test_pcge)
    add_executable(test_unicode tests/test_unicode.c)
    target_include_directories(test_unicode PRIVATE include)
    set_target_properties(test_unicode PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
    add_test(NAME pcge_unicode COMMAND test_unicode)
endif()
install(TARGETS pcge EXPORT pcgeTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/pcge.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT pcgeTargets NAMESPACE pcge:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pcge)
install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/pcge)
install(FILES licenses/LICENSE-UNICODE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/pcge)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/pcgeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pcgeConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pcge)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/pcgeConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pcgeConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pcgeConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pcge)
