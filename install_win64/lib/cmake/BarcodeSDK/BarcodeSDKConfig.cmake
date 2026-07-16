
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was BarcodeSDKConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# SDK根目录下第三方依赖路径
set(BARCODE_SDK_THIRD_PARTY "/third_party")

# 自动设置第三方cmake查找路径
set(VTK_DIR "${BARCODE_SDK_THIRD_PARTY}/VTK-9.2.6/${INSTALL_SUFFIX}/lib/cmake/vtk-9.2")
set(OpenCV_DIR "${BARCODE_SDK_THIRD_PARTY}/opencv-4.12.0/${INSTALL_SUFFIX}")
set(aruco_DIR "${BARCODE_SDK_THIRD_PARTY}/aruco-3.1.12/${INSTALL_SUFFIX}/share/aruco")
set(apriltag_DIR "${BARCODE_SDK_THIRD_PARTY}/apriltag/${INSTALL_SUFFIX}/lib/apriltag/cmake")

# 加载导出的库目标
include("${CMAKE_CURRENT_LIST_DIR}/BarcodeSDKTargets.cmake")
check_required_components(BarcodeSDK)
