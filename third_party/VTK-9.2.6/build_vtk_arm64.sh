#!/bin/bash
set -euo pipefail

VTK_ROOT="/workspace/third_party/VTK-9.2.6"
INSTALL_DIR="${VTK_ROOT}/install_arm64"
BUILD_DIR="${VTK_ROOT}/build_arm64"

echo "===== 开始编译 VTK 9.2.6 arm64 Release ====="
cd "${VTK_ROOT}"

echo "1. 清理旧构建目录"
rm -rf build build_arm64 install_arm64

echo "2. 创建编译目录 ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}"

echo "3. CMake 配置"
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTING=OFF ..

echo "4. 多核编译"
make -j$(nproc)

echo "5. 安装编译产物"
make install

cd ../../
echo "===== 编译完成 ====="
echo "输出目录：${INSTALL_DIR}"