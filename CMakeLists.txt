cmake_minimum_required(VERSION 3.18)
project(BarcodeSDK LANGUAGES C CXX)

# C++标准全局配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 开关：控制静态/动态库、是否编译Demo
option(BUILD_SHARED_LIBS "ON=动态库dll/so  OFF=静态库lib/a" OFF)
option(BUILD_DEMO "编译main.cpp演示程序" ON)

# ===================== 平台差异化编译配置 =====================
if(MSVC)
    set(BUILD_DIR_NAME "build_win64")
    set(INSTALL_SUFFIX "install_win64")
else()
    set(BUILD_DIR_NAME "build_arm64")
    set(INSTALL_SUFFIX "install_arm64")
endif()
# 统一设置安装目录：CMakeLists.txt同目录下 install_xxx
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/${INSTALL_SUFFIX}")

if(MSVC)
    # Windows x64 MSVC
    add_compile_options(/utf-8)
    add_definitions(-DNOMINMAX)
    add_definitions(-DPROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    # 统一MD/MDd运行库，避免依赖冲突
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
else()
    # Linux ARM64
    add_compile_options(-Wall -O3 -march=armv8-a+simd -Wno-unused-function)
    add_definitions(-DLINUX)
    add_definitions(-DPROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    option(DOCKER_BUILD "Docker无头环境编译" ON)
    if(DOCKER_BUILD)
        add_definitions(-DRUN_IN_DOCKER)
    endif()
    # Linux 必须开启PIC，否则静态库无法链接动态库
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# ===================== 加载第三方依赖 =====================
set(THIRD_PARTY_SUFFIX ${INSTALL_SUFFIX})
# VTK
set(VTK_ROOT "${PROJECT_SOURCE_DIR}/third_party/VTK-9.2.6/${THIRD_PARTY_SUFFIX}")
set(VTK_DIR "${VTK_ROOT}/lib/cmake/vtk-9.2")
find_package(VTK REQUIRED)

# OpenCV
set(OPENCV_ROOT "${PROJECT_SOURCE_DIR}/third_party/opencv-4.12.0/${THIRD_PARTY_SUFFIX}")
if(MSVC)
    set(OpenCV_DIR "${OPENCV_ROOT}/")
else()
    set(OpenCV_DIR "${OPENCV_ROOT}/lib/cmake/opencv4")
endif()
find_package(OpenCV REQUIRED)

# AprilTag
set(APRILTAG_ROOT "${PROJECT_SOURCE_DIR}/third_party/apriltag/${THIRD_PARTY_SUFFIX}")
set(apriltag_DIR "${APRILTAG_ROOT}/lib/apriltag/cmake")
find_package(apriltag REQUIRED)

# Aruco
set(ARUCO_ROOT "${PROJECT_SOURCE_DIR}/third_party/aruco-3.1.12/${THIRD_PARTY_SUFFIX}")
set(aruco_DIR "${ARUCO_ROOT}/share/aruco")
find_package(aruco REQUIRED)

if(MSVC)
    set(MVS_DEV_ROOT "C:/Program Files (x86)/MVS")
    set(MVS_INCLUDE ${MVS_DEV_ROOT}/Development/Includes)
    set(MVS_LIB_DIR ${MVS_DEV_ROOT}/Development/Libraries/win64)
    set(MVS_RUNTIME_ROOT "C:/Program Files (x86)/Common Files/MVS")
    set(MVS_RUNTIME ${MVS_RUNTIME_ROOT}/Runtime/Win64_x64)
    include_directories(${MVS_INCLUDE})
endif()

# 全局头文件检索
include_directories(${PROJECT_SOURCE_DIR}/include)

# ===================== 1. 编译核心库 BarcodeSDK =====================
# 收集所有cpp，剔除main.cpp（main作为Demo单独编译）
file(GLOB ALL_SOURCE_FILES "${PROJECT_SOURCE_DIR}/source/*.cpp")
list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX "main.cpp")

# 生成静态/动态库，由BUILD_SHARED_LIBS控制
add_library(BarcodeSDK ${ALL_SOURCE_FILES})

# 对外头文件路径：构建时、安装后两套路径
target_include_directories(BarcodeSDK PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# PUBLIC：第三方使用者链接库时自动继承OpenCV/VTK等依赖
target_link_libraries(BarcodeSDK PUBLIC
    ${VTK_LIBRARIES}
    ${OpenCV_LIBS}
    ${aruco_LIBS}
    apriltag::apriltag
    $<$<NOT:$<BOOL:${MSVC}>>:pthread>
)

# Windows 追加MVS库链接
if(MSVC)
    target_link_directories(BarcodeSDK PUBLIC ${MVS_LIB_DIR})
    target_link_libraries(BarcodeSDK PUBLIC MvCameraControl.lib)
endif()

# Windows动态库导出符号（避免dllexport报错）
if(MSVC AND BUILD_SHARED_LIBS)
    target_compile_definitions(BarcodeSDK PRIVATE BARCODESDK_EXPORTS)
endif()

# ===================== 2. 编译Demo演示程序 =====================
if(BUILD_DEMO)
    add_executable(BarcodeDemo source/main.cpp)
    target_link_libraries(BarcodeDemo PRIVATE BarcodeSDK)

    # VS调试环境自动填充第三方dll路径
    if(MSVC)
        set(VTK_BIN "${VTK_ROOT}/bin")
        set(OPENCV_BIN "${OPENCV_ROOT}/bin")
        set(ARUCO_BIN "${ARUCO_ROOT}/bin")
        set(APRILTAG_BIN "${APRILTAG_ROOT}/bin")
		# 把MVS Runtime追加到PATH，调试时不用手动复制dll
        set(MVS_RUNTIME_PATH ${MVS_RUNTIME})
        set_target_properties(BarcodeDemo PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "PATH=${VTK_BIN};${OPENCV_BIN};${ARUCO_BIN};${APRILTAG_BIN};${MVS_RUNTIME_PATH};%PATH%"
        )
		# 编译完成自动复制MVS所有dll/cti到exe输出目录
        file(GLOB MVS_RUNTIME_FILES
            ${MVS_RUNTIME}/*.dll
            ${MVS_RUNTIME}/*.cti
        )
        add_custom_command(TARGET BarcodeDemo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MVS_RUNTIME_FILES}
            $<TARGET_FILE_DIR:BarcodeDemo>
            COMMENT "复制海康MVS运行时库到Demo程序目录"
        )
    endif()
endif()

# ===================== 3. Install 安装打包（核心导出SDK） =====================
# 安装库、Demo、导出CMake Target信息
install(TARGETS BarcodeSDK BarcodeDemo
    EXPORT BarcodeSDKTargets
    LIBRARY DESTINATION lib          # Linux so
    ARCHIVE DESTINATION lib          # Windows lib / Linux a
    RUNTIME DESTINATION bin          # Windows dll / exe
)

# 拷贝对外头文件到SDK/include
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

# 拷贝全套第三方预编译库到SDK/third_party（开箱即用，用户不用自己装OpenCV/VTK）
install(DIRECTORY
    ${VTK_ROOT}/
    ${OPENCV_ROOT}/
    ${ARUCO_ROOT}/
    ${APRILTAG_ROOT}/
    DESTINATION third_party
)

# Windows安装包同步打包MVS Runtime库
if(MSVC)
    install(DIRECTORY ${MVS_RUNTIME}/
        DESTINATION bin/mvs_runtime
        FILES_MATCHING PATTERN "*.dll" PATTERN "*.cti"
    )
endif()

# 导出库的编译目标信息 BarcodeSDKTargets.cmake
install(EXPORT BarcodeSDKTargets
    FILE BarcodeSDKTargets.cmake
    NAMESPACE BarcodeSDK::
    DESTINATION lib/cmake/BarcodeSDK
)

# 打包JSON配置模板到SDK
if(EXISTS ${PROJECT_SOURCE_DIR}/config/camera_calc_config.json)
    install(FILES ${PROJECT_SOURCE_DIR}/config/camera_calc_config.json
        DESTINATION config
    )
endif()

# 生成SDK配置文件，供外部find_package加载
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/BarcodeSDKConfig.cmake.in
    ${CMAKE_BINARY_DIR}/BarcodeSDKConfig.cmake
    INSTALL_DESTINATION lib/cmake/BarcodeSDK
)
install(FILES ${CMAKE_BINARY_DIR}/BarcodeSDKConfig.cmake
    DESTINATION lib/cmake/BarcodeSDK
)