vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/TitanCSA/Interceptor-Keyboard-Mouse/releases/download/sdk-v1.0.5/interceptor-keyboard-mouse-sdk-1.0.5.tar.gz"
    FILENAME "interceptor-keyboard-mouse-sdk-1.0.5.tar.gz"
    SHA512 051a76f6061768da6b19f939592ec8bf4a04054ffae2935c9989341b06c8a16e2803542c36f8d6e98f693df8910f4083806241fae6ab547057d6ebab7bd716f8
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME InterceptorKeyboardMouseSDK CONFIG_PATH lib/cmake/InterceptorKeyboardMouseSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
