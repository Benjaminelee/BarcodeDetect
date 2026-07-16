#!/bin/bash
# ===================== 工程配置 =====================
# 宿主机工程根目录
HOST_PROJ=/data/lw/BarcodeDetect
# 编译镜像名称
IMG=barcode_arm64_env:base
# ====================================================

# 前置：解决容器图形界面显示（VTK可视化窗口必备）
# export DISPLAY=:0
# Windows本机局域网IP
WIN_HOST_IP=192.168.0.26

# 容器内第三方库路径（容器内路径，不要写宿主机路径）
CONTAINER_LIB_PATH="\
/workspace/third_party/opencv-4.12.0/install_arm64/lib:\
/workspace/third_party/VTK-9.2.6/install_arm64/lib:\
/workspace/third_party/aruco-3.1.12/install_arm64/lib:\
/workspace/third_party/apriltag/install_arm64/lib"

docker run -it --rm \
    --privileged \
    --group-add video \
    -e DISPLAY=${WIN_HOST_IP}:0.0 \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -e VTK_DEFAULT_RENDER_WINDOW_OFFSCREEN=1 \
    -e LANG=en_US.UTF-8 \
    -e LC_ALL=en_US.UTF-8 \
    -v /opt/sophon:/opt/sophon \
    -v ${HOST_PROJ}:/workspace \
    --device /dev/video12 \
    --device /dev/video13 \
    --net=host \
    -e LD_LIBRARY_PATH=${CONTAINER_LIB_PATH}:$LD_LIBRARY_PATH \
    ${IMG}