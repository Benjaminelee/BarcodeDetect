#!/bin/bash
set -euo pipefail

# ====================== 路径配置区（无需修改，匹配你现有目录） ======================
# aruco源码根目录
ARUCO_ROOT="/workspace/third_party/aruco-3.1.12"
# OpenCV ARM64安装目录
OPENCV_INSTALL="/workspace/third_party/opencv-4.12.0/install_arm64"
# OpenCV CMake配置目录（从截图确认真实路径）
OPENCV_CMAKE_DIR="${OPENCV_INSTALL}/lib/cmake/opencv4"
# aruco输出安装目录
ARUCO_INSTALL="${ARUCO_ROOT}/install_arm64"
# 构建目录
BUILD_DIR="${ARUCO_ROOT}/build_arm64"
# ==============================================================================

echo "==================== Aruco-3.1.12 ARM64 一键编译脚本 ===================="
echo "Aruco源码路径: ${ARUCO_ROOT}"
echo "OpenCV CMake路径: ${OPENCV_CMAKE_DIR}"
echo "Aruco输出安装目录: ${ARUCO_INSTALL}"
echo "========================================================================"

# 1. 校验OpenCV配置文件是否存在
if [ ! -f "${OPENCV_CMAKE_DIR}/OpenCVConfig.cmake" ]; then
    echo "❌ 错误：未找到OpenCVConfig.cmake，请检查OpenCV编译安装是否完整！"
    echo "查找路径：${OPENCV_CMAKE_DIR}/OpenCVConfig.cmake"
    exit 1
fi

# 2. 清理旧构建目录
echo "🔧 清理旧构建缓存..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || exit 1

# 3. CMake配置
echo "🔧 执行CMake配置..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${ARUCO_INSTALL}" \
      -DOpenCV_DIR="${OPENCV_CMAKE_DIR}" \
      ..

# 4. 多线程编译
echo "🔧 开始编译（CPU核心数自动匹配）..."
make -j$(nproc)

# 5. 安装库文件
echo "🔧 执行安装到 ${ARUCO_INSTALL} ..."
make install

# 6. 刷新系统动态链接库缓存
echo "🔧 更新动态库缓存..."
echo "${OPENCV_INSTALL}/lib" >> /etc/ld.so.conf.d/opencv4-arm64.conf
ldconfig

echo "✅ Aruco-3.1.12 ARM64 编译安装完成！"
echo "头文件目录: ${ARUCO_INSTALL}/include"
echo "库文件目录: ${ARUCO_INSTALL}/lib"