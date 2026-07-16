#!/bin/bash
set -e  # 任意命令失败直接退出，避免错误继续执行

# 1. 定义路径常量，方便后续修改
APRILTAG_ROOT="/workspace/third_party/apriltag"
OPENCV_ARM64_CMAKE_DIR="/workspace/third_party/opencv-4.12.0/install_arm64/lib/cmake/opencv4"
BUILD_DIR="${APRILTAG_ROOT}/build_arm64"
INSTALL_DIR="${APRILTAG_ROOT}/install_arm64"

echo "==================== 开始编译 Apriltag ARM64 库 ===================="
echo "源码目录: ${APRILTAG_ROOT}"
echo "构建目录: ${BUILD_DIR}"
echo "安装输出目录: ${INSTALL_DIR}"
echo "ARM64 OpenCV CMake路径: ${OPENCV_ARM64_CMAKE_DIR}"
echo "=================================================================="

# 2. 进入源码根目录
cd "${APRILTAG_ROOT}" || exit 1

# 3. 清理旧构建、安装目录
echo "[1/5] 清理历史构建缓存..."
rm -rf build build_arm64 install_arm64

# 4. 创建构建目录并进入
echo "[2/5] 创建编译目录 build_arm64"
mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}" || exit 1

# 5. CMake配置（已修复换行语法，关闭Python绑定、指定ARM64 OpenCV）
echo "[3/5] CMake 配置生成Makefile..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DOpenCV_DIR="${OPENCV_ARM64_CMAKE_DIR}" \
      -DBUILD_PYTHON_WRAPPER=OFF \
      ..

# 6. 多线程编译
echo "[4/5] 多线程编译源码..."
make -j$(nproc)

# 7. 安装头文件+库文件到install_arm64
echo "[5/5] 安装编译产物..."
make install

# 8. 编译完成校验
echo "=================================================================="
echo "Apriltag ARM64 编译&安装全部完成！"
echo "头文件目录: ${INSTALL_DIR}/include"
echo "库文件目录: ${INSTALL_DIR}/lib"
echo "产物列表:"
ls -la "${INSTALL_DIR}/lib"
ls -la "${INSTALL_DIR}/include"
echo "=================================================================="