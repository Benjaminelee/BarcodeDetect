#!/bin/bash
set -euo pipefail  # 任意步骤报错直接终止脚本

# ===================== 可配置全局参数 =====================
# 默认参数，可外部传参覆盖：./build_arm64.sh --shared ON --install ./my_sdk
BUILD_SHARED_LIBS="ON"     # OFF静态库 / ON动态库
BUILD_DEMO="ON"            # 是否编译Demo程序
DOCKER_BUILD="ON"          # Docker无头编译（关闭VTK窗口）
CMAKE_BUILD_TYPE="Release" # Release/Debug
# ==========================================================

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        --shared)
            BUILD_SHARED_LIBS="$2"
            shift 2
            ;;
        --demo)
            BUILD_DEMO="$2"
            shift 2
            ;;
        --docker)
            DOCKER_BUILD="$2"
            shift 2
            ;;
        --type)
            CMAKE_BUILD_TYPE="$2"
            shift 2
            ;;
        --install)
            CUSTOM_INSTALL="$2"
            shift 2
            ;;
        *)
            echo "未知参数: $1"
            echo "用法示例："
            echo "  ./build_arm64.sh                          # 默认静态库+编译Demo+Release"
            echo "  ./build_arm64.sh --shared ON --type Debug # 动态库Debug版本"
            echo "  ./build_arm64.sh --install ./output_sdk   # 指定SDK输出目录"
            exit 1
            ;;
    esac
done

# 工程根目录（脚本放在项目根目录，自动获取路径，无需手动修改）
PROJ_ROOT=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="${PROJ_ROOT}/build_arm64"
if [[ -n "${CUSTOM_INSTALL:-}" ]]; then
    INSTALL_DIR=$(realpath "${CUSTOM_INSTALL}")
else
    INSTALL_DIR="${PROJ_ROOT}/install_arm64"
fi

echo "====================================="
echo "  Barcode ARM64 一键编译打包脚本"
echo "  工程根目录: ${PROJ_ROOT}"
echo "  构建目录:   ${BUILD_DIR}"
echo "  SDK输出目录:${INSTALL_DIR}"
echo "  构建类型:   ${CMAKE_BUILD_TYPE}"
echo "  动态库:     ${BUILD_SHARED_LIBS}"
echo "  编译Demo:   ${BUILD_DEMO}"
echo "  Docker无头: ${DOCKER_BUILD}"
echo "====================================="

# 前置依赖检查
check_cmd() {
    if ! command -v "$1" &> /dev/null; then
        echo "❌ 错误：未找到命令 $1，请先安装！"
        exit 1
    fi
}
check_cmd cmake
check_cmd make
check_cmd gcc
check_cmd g++

# 1. 删除旧构建目录 + 旧安装目录
echo "[1/6] 清理旧构建缓存、旧安装包..."
rm -rf "${BUILD_DIR}"
rm -rf "${INSTALL_DIR}"

# 2. 创建全新构建目录并进入
echo "[2/6] 创建构建文件夹..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || exit 1

# 3. CMake 配置
echo "[3/6] 执行 cmake 配置..."
cmake "${PROJ_ROOT}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
    -DBUILD_DEMO="${BUILD_DEMO}" \
    -DDOCKER_BUILD="${DOCKER_BUILD}" \
    -DRUN_IN_DOCKER="${DOCKER_BUILD}"

# 4. 多核编译
echo "[4/6] 开始并行编译，核心数: $(nproc)"
make -j$(nproc)

# 5. 安装库文件
echo "[5/6] 开始安装SDK至 ${INSTALL_DIR} ..."
cmake --install .

# 6. 打包SDK压缩包（交付第三方必备）
echo "[6/6] 打包SDK压缩包 barcode_sdk_arm64.tar.gz"
SDK_PACK="${PROJ_ROOT}/barcode_sdk_arm64.tar.gz"
rm -f "${SDK_PACK}"
tar -czf "${SDK_PACK}" -C "${INSTALL_DIR}" .

# 完成提示
echo "====================================="
echo "✅ 编译打包全部完成！"
if [[ "${BUILD_DEMO}" == "ON" ]]; then
    echo "Demo程序路径: ${BUILD_DIR}/BarcodeDemo"
fi
echo "完整SDK目录: ${INSTALL_DIR}"
echo "交付压缩包: ${SDK_PACK}"
echo ""
echo "【Linux运行提示】程序运行前配置动态库路径："
echo "export LD_LIBRARY_PATH=${INSTALL_DIR}/lib:\$LD_LIBRARY_PATH"
echo '通用模板：export LD_LIBRARY_PATH=SDK安装目录/lib:$LD_LIBRARY_PATH'
echo "====================================="