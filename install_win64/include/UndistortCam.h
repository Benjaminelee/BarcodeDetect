#ifndef UNDISTORTCAM_H
#define UNDISTORTCAM_H

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <string>

class CameraUndistorter
{
public:
    // 从xml加载内参、畸变系数，预生成映射表
    bool init(const std::string& xmlPath);

    // 输入原图，输出校正后图像
    cv::Mat undistortImage(const cv::Mat& src);

    // 获取图像尺寸
    cv::Size getImgSize() const { return m_imgSize; }

    // 获取相机内参、畸变系数
    cv::Mat getCameraMatrix() const { return m_cameraMat; }
    cv::Mat getDistCoeffs()   const { return m_distCoeff; }

private:
    cv::Mat m_cameraMat;
    cv::Mat m_distCoeff;
    cv::Mat m_map1, m_map2; // 畸变映射表（预计算一次）
    cv::Size m_imgSize;
};

// 鱼眼校正器类
class FisheyeUndist
{
public:
    bool init(const std::string& xmlPath);
    cv::Mat undistortImage(const cv::Mat& src);
    cv::Size getImgSize() const { return m_imgSz; }
    cv::Mat getCameraMatrix() const { return m_K; }
    cv::Mat getDistCoeffs()   const { return m_D; }

private:
    cv::Mat m_K, m_D;
    cv::Mat m_map1, m_map2;
    cv::Size m_imgSz;
};

#endif