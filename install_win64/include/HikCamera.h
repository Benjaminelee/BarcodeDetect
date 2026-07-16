#ifndef HIK_CAMERA_H
#define HIK_CAMERA_H

#include "MvCameraControl.h"
#include <opencv2/opencv.hpp>
#include <iostream>

/**
 * @brief 海康工业相机MVS SDK封装类
 * 仅实现单相机(第0台)连续采集、取图、释放资源
 * 默认Mono8灰度输出，适配黑白工业相机
 */
class HikCamera
{
public:
    HikCamera();
    ~HikCamera();

    /// <summary>
    /// 枚举并打开第0台海康工业相机（USB/GigE）
    /// </summary>
    /// <returns>成功true，失败false</returns>
    bool OpenCamera();

    /// <summary>
    /// 抓取一帧图像，返回OpenCV灰度Mat（Mono8）
    /// </summary>
    /// <param name="timeoutMs">取图超时毫秒</param>
    /// <returns>空Mat代表取图失败</returns>
    cv::Mat GetFrame(int timeoutMs = 1000);

    /// <summary>
    /// 停止采集、关闭相机、释放句柄
    /// </summary>
    void CloseCamera();

    /// <summary>
    /// 判断相机是否已打开
    /// </summary>
    bool IsOpened() const { return m_hCamera != nullptr; }

private:
    void* m_hCamera;       // 海康相机句柄
    bool m_bGrabbing;      // 是否正在采集标记
};

#endif