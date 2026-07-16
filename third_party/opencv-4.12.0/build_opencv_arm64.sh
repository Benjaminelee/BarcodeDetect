#!/bin/bash
# 出错立即退出、变量未定义直接报错
set -euo pipefail

# 统一路径配置，修改只改这里
OPENCV_ROOT="/workspace/third_party/opencv-4.12.0"
CONTRIB_MODULE_PATH="${OPENCV_ROOT}/../opencv_contrib-4.12.0/modules"
BUILD_DIR="${OPENCV_ROOT}/build_arm64"
INSTALL_DIR="${OPENCV_ROOT}/install_arm64"

echo "==================== OpenCV4 + contrib ARM64 编译脚本 ===================="
echo "源码目录: ${OPENCV_ROOT}"
echo "contrib模块路径: ${CONTRIB_MODULE_PATH}"
echo "编译目录: ${BUILD_DIR}"
echo "安装输出目录: ${INSTALL_DIR}"

# 1. 进入opencv根目录
cd "${OPENCV_ROOT}"

# 2. 清理旧构建目录
echo "1. 清理旧构建文件夹 build / build_arm64"
rm -rf build build_arm64 install_arm64

# 3. 创建并进入编译目录
echo "2. 创建编译目录并进入"
mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}"

# 4. CMake 配置
echo "3. 执行 CMake 配置"
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DOPENCV_EXTRA_MODULES_PATH="${CONTRIB_MODULE_PATH}" \
      -DVTK_DIR=/workspace/third_party/VTK-9.2.6/install_arm64/lib/cmake/vtk-9.2 \
      -DBUILD_EXTRA_MODULES=ON \
      -DWITH_V4L=ON -DWITH_FFMPEG=ON \
      -DWITH_GTK=ON -DWITH_VTK=ON -DWITH_VIZ=ON \
      -DWITH_CUDA=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF \
      ..

# 5. 多核编译
echo "4. 开始多核编译，线程数: $(nproc)"
make -j$(nproc)

# 6. 安装头文件、库、工具
echo "5. 执行 install 安装编译产物"
make install

# 7. 返回上级目录
cd ../../

echo "================="
echo "OpenCV + opencv-contrib 编译安装完成！"
echo "输出路径: ${INSTALL_DIR}"
echo "viz模块已启用（依赖VTK，需提前编译安装VTK）"
echo "================="


# cmake -DCMAKE_BUILD_TYPE=Release \
# -DCMAKE_INSTALL_PREFIX=../install_arm64 \
# # 基础路径
# -DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib-4.12.0/modules \
# -DCMAKE_CXX_STANDARD=17 \
# -DCMAKE_C_STANDARD=11 \
# # 启用硬件/媒体底层
# -DWITH_V4L=ON \
# -DWITH_FFMPEG=ON \
# -DWITH_GTK=ON \
# -DWITH_GTK_2_X=OFF \
# -DWITH_JPEG=ON \
# -DWITH_PNG=ON \
# -DWITH_TIFF=ON \
# -DWITH_JASPER=ON \
# -DWITH_OPENEXR=ON \
# -DWITH_WEBP=ON \
# -DWITH_HDR=ON \
# -DWITH_GDAL=OFF \
# -DWITH_IMGCODEC_HDR=ON \
# -DWITH_IMGCODEC_SUNRASTER=ON \
# -DWITH_IMGCODEC_PXM=ON \
# -DWITH_IMGCODEC_PFM=ON \
# # 3D可视化（依赖VTK）
# -DWITH_VTK=ON \
# -DWITH_VIZ=ON \
# # 数学/矩阵
# -DWITH_EIGEN=ON \
# -DWITH_LAPACK=ON \
# -DWITH_CBLAS=ON \
# # 相机/视频
# -DWITH_1394=OFF \
# -DWITH_CAMERAS=ON \
# -DWITH_GSTREAMER=ON \
# -DWITH_OPENNI=OFF \
# -DWITH_OPENNI2=OFF \
# -DWITH_XIMEA=OFF \
# -DWITH_UEYE=OFF \
# # OpenGL渲染
# -DWITH_OPENGL=ON \
# -DWITH_QT=OFF \
# -DWITH_GTK=ON \
# # 并行加速
# -DWITH_TBB=ON \
# -DWITH_OPENMP=ON \
# -DWITH_PTHREADS_PF=ON \
# -DWITH_IPP=OFF \
# -DWITH_MKL=OFF \
# # 神经网络/AI模块
# -DWITH_OPENVX=OFF \
# -DWITH_CUDA=OFF \
# -DWITH_CUDNN=OFF \
# -DWITH_HALIDE=OFF \
# # 扩展模块总开关
# -DBUILD_opencv_world=OFF \
# -DBUILD_EXAMPLES=OFF \
# -DBUILD_TESTS=OFF \
# -DBUILD_PERF_TESTS=OFF \
# -DBUILD_DOCS=OFF \
# # 启用全部核心功能模块
# -DBUILD_opencv_core=ON \
# -DBUILD_opencv_imgproc=ON \
# -DBUILD_opencv_imgcodecs=ON \
# -DBUILD_opencv_highgui=ON \
# -DBUILD_opencv_videoio=ON \
# -DBUILD_opencv_video=ON \
# -DBUILD_opencv_calib3d=ON \
# -DBUILD_opencv_features2d=ON \
# -DBUILD_opencv_flann=ON \
# -DBUILD_opencv_ml=ON \
# -DBUILD_opencv_objdetect=ON \
# -DBUILD_opencv_photo=ON \
# -DBUILD_opencv_stitching=ON \
# -DBUILD_opencv_viz=ON \
# -DBUILD_opencv_gapi=ON \
# # 启用全部contrib扩展模块（aruco/ AprilTag/ fisheye等）
# -DBUILD_opencv_aruco=ON \
# -DBUILD_opencv_fisheye=ON \
# -DBUILD_opencv_ccalib=ON \
# -DBUILD_opencv_cvv=ON \
# -DBUILD_opencv_dnn=ON \
# -DBUILD_opencv_dnn_objdetect=ON \
# -DBUILD_opencv_dnn_superres=ON \
# -DBUILD_opencv_freetype=ON \
# -DBUILD_opencv_hfs=ON \
# -DBUILD_opencv_img_hash=ON \
# -DBUILD_opencv_line_descriptor=ON \
# -DBUILD_opencv_optflow=ON \
# -DBUILD_opencv_phase_unwrapping=ON \
# -DBUILD_opencv_plot=ON \
# -DBUILD_opencv_reg=ON \
# -DBUILD_opencv_rgbd=ON \
# -DBUILD_opencv_saliency=ON \
# -DBUILD_opencv_shape=ON \
# -DBUILD_opencv_stereo=ON \
# -DBUILD_opencv_structured_light=ON \
# -DBUILD_opencv_surface_matching=ON \
# -DBUILD_opencv_tracking=ON \
# -DBUILD_opencv_ximgproc=ON \
# -DBUILD_opencv_xobjdetect=ON \
# -DBUILD_opencv_xphoto=ON \
# -DBUILD_opencv_mcc=ON \
# -DBUILD_opencv_barcode=ON \
# -DBUILD_opencv_apriltag=ON \
# ..