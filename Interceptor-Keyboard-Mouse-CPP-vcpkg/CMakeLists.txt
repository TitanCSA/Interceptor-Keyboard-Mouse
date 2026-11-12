cmake_minimum_required(VERSION 3.24)
project(InterceptorKeyboardMouseSDK VERSION 1.0.5.0 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanInterceptorKeyboardMouse STATIC
  "sdk/Interceptor/Interceptor.cpp"
)
add_library(InterceptorKeyboardMouseSDK::sdk ALIAS TitanInterceptorKeyboardMouse)
set_target_properties(TitanInterceptorKeyboardMouse PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanInterceptorKeyboardMouse PUBLIC cxx_std_17)
target_compile_definitions(TitanInterceptorKeyboardMouse PUBLIC )
target_compile_definitions(TitanInterceptorKeyboardMouse PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanInterceptorKeyboardMouse PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanInterceptorKeyboardMouse PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/Interceptor>"
  "$<INSTALL_INTERFACE:include/Interceptor>"
)
target_link_libraries(TitanInterceptorKeyboardMouse PUBLIC advapi32 bcrypt cfgmgr32 comctl32 hid newdev ole32 setupapi user32)
install(TARGETS TitanInterceptorKeyboardMouse EXPORT InterceptorKeyboardMouseSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/interceptor-keyboard-mouse)
install(EXPORT InterceptorKeyboardMouseSDKTargets NAMESPACE InterceptorKeyboardMouseSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InterceptorKeyboardMouseSDK)
configure_file("InterceptorKeyboardMouseSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/InterceptorKeyboardMouseSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/InterceptorKeyboardMouseSDKConfigVersion.cmake" VERSION 1.0.5.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/InterceptorKeyboardMouseSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/InterceptorKeyboardMouseSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InterceptorKeyboardMouseSDK)
