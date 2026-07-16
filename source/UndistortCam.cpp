#include "../include/UndistortCam.h"
#include <iostream>

using namespace cv;

bool CameraUndistorter::init(const std::string& xmlPath)
{
    FileStorage fs(xmlPath, FileStorage::READ);
    if (!fs.isOpened())
    {
        std::cout << "无法打开标定文件：" << xmlPath << std::endl;
        return false;
    }
    cv::Mat tempK, tempD;
    fs["cameraMatrix"] >> tempK;
    fs["distCoeffs"]  >> tempD;
    fs["imageSize"]   >> m_imgSize;
    fs.release();

    // 强制统一转为 double CV_64F
    tempK.convertTo(m_cameraMat, CV_64F);
    tempD.convertTo(m_distCoeff, CV_64F);
    if (m_cameraMat.type() != CV_64F || m_distCoeff.type() != CV_64F)
    {
        std::cout << "错误：相机内参/畸变系数转换double失败" << std::endl;
        return false;
    }

    // 优化内参 + 生成畸变查找表（只初始化算一次）
    Mat newCamMat = getOptimalNewCameraMatrix(m_cameraMat, m_distCoeff, m_imgSize, 1, m_imgSize);
    initUndistortRectifyMap(m_cameraMat, m_distCoeff, Mat(), newCamMat,
                            m_imgSize, CV_16SC2, m_map1, m_map2);
    return true;
}

Mat CameraUndistorter::undistortImage(const Mat& src)
{
    Mat dst;
    remap(src, dst, m_map1, m_map2, INTER_LINEAR);
    return dst;
}

//=====鱼眼校正实现=====
bool FisheyeUndist::init(const std::string& xmlPath)
{
    FileStorage fs(xmlPath, FileStorage::READ);
    if (!fs.isOpened()) return false;
    Mat tempK, tempD;
    fs["K"] >> tempK;
    fs["D"] >> tempD;
    fs["imgSize"] >> m_imgSz;
    fs.release();
    
    // 强制统一转为 double CV_64F
    tempK.convertTo(m_K, CV_64F);
    tempD.convertTo(m_D, CV_64F);
    if (m_K.type() != CV_64F || m_D.type() != CV_64F)
    {
        std::cout << "错误：鱼眼K/D转换double失败" << std::endl;
        return false;
    }

    Mat newK;
    fisheye::estimateNewCameraMatrixForUndistortRectify(m_K, m_D, m_imgSz, Mat(), newK);
    fisheye::initUndistortRectifyMap(m_K, m_D, Mat(), newK, m_imgSz, CV_16SC2, m_map1, m_map2);
    return true;
}

cv::Mat FisheyeUndist::undistortImage(const cv::Mat& src)
{
    Mat dst;
    remap(src, dst, m_map1, m_map2, INTER_LINEAR);
    return dst;
}