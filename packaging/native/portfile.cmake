vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Storage-IO-Flight-Recorder/releases/download/sdk-v0.1.1/storage-io-flight-recorder-sdk-0.1.1.tar.gz"
    FILENAME "storage-io-flight-recorder-sdk-0.1.1.tar.gz"
    SHA512 10d7d456da728abbafee7dbcd8ff3214677b39c5b411ece8c6118092791719cb56126cd2febe1d5a2d67114d550e40a4418a653dbe2c49b9119356b0d0a3100b
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME StorageIOFlightRecorderSDK CONFIG_PATH lib/cmake/StorageIOFlightRecorderSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
