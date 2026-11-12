cmake_minimum_required(VERSION 3.24)
project(StorageIOFlightRecorderSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanStorageIOFlightRecorder STATIC
  "sdk/SDK/StorageRecorder.cpp"
)
add_library(StorageIOFlightRecorderSDK::sdk ALIAS TitanStorageIOFlightRecorder)
set_target_properties(TitanStorageIOFlightRecorder PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanStorageIOFlightRecorder PUBLIC cxx_std_17)
target_compile_definitions(TitanStorageIOFlightRecorder PUBLIC )
target_compile_definitions(TitanStorageIOFlightRecorder PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanStorageIOFlightRecorder PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanStorageIOFlightRecorder PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanStorageIOFlightRecorder PUBLIC advapi32 cfgmgr32 comctl32 comdlg32 crypt32 gdi32 newdev ole32 setupapi shell32 user32 wintrust)
install(TARGETS TitanStorageIOFlightRecorder EXPORT StorageIOFlightRecorderSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/storage-io-flight-recorder)
install(EXPORT StorageIOFlightRecorderSDKTargets NAMESPACE StorageIOFlightRecorderSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StorageIOFlightRecorderSDK)
configure_file("StorageIOFlightRecorderSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/StorageIOFlightRecorderSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/StorageIOFlightRecorderSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/StorageIOFlightRecorderSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/StorageIOFlightRecorderSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StorageIOFlightRecorderSDK)
