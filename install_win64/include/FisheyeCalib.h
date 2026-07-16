#ifndef FISHEYE_CALIB_H
#define FISHEYE_CALIB_H

#include <opencv2/opencv.hpp>
#include <string>

// 和原有标定板参数保持一致：12×9内角、方格10mm
const cv::Size FISH_BOARD_SIZE = cv::Size(11,8);
const float    FISH_SQUARE_MM  = 10.0f;

// 鱼眼标定：采集+保存xml
bool runFisheyeCalib(const std::string& imgDir = "../fisheye_img",
                    const std::string& saveXml = "../fisheye_calib.xml",
                    int capId = 0,
                    int width = 640,
                    int height = 480);

//离线文件夹标定鱼眼
bool runFisheyeCalib_FromImages(const std::string& imgDir = "../fisheye_img", const std::string& saveXml = "../fisheye_calib.xml");

#endif