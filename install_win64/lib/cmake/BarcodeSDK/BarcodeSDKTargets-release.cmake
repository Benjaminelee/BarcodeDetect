#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "BarcodeSDK::BarcodeSDK" for configuration "Release"
set_property(TARGET BarcodeSDK::BarcodeSDK APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(BarcodeSDK::BarcodeSDK PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/BarcodeSDK.lib"
  )

list(APPEND _cmake_import_check_targets BarcodeSDK::BarcodeSDK )
list(APPEND _cmake_import_check_files_for_BarcodeSDK::BarcodeSDK "${_IMPORT_PREFIX}/lib/BarcodeSDK.lib" )

# Import target "BarcodeSDK::BarcodeDemo" for configuration "Release"
set_property(TARGET BarcodeSDK::BarcodeDemo APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(BarcodeSDK::BarcodeDemo PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/BarcodeDemo.exe"
  )

list(APPEND _cmake_import_check_targets BarcodeSDK::BarcodeDemo )
list(APPEND _cmake_import_check_files_for_BarcodeSDK::BarcodeDemo "${_IMPORT_PREFIX}/bin/BarcodeDemo.exe" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
