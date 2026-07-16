#ifndef CALIBCAM_H
#define CALIBCAM_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

// ===== 标定板固定参数（适配你12×9、10mm方格标定板）=====
const cv::Size BOARD_SIZE = cv::Size(11, 8); //11*8表示的是内部角点的数量
const float    SQUARE_LEN_MM = 10.0f;

/**
 * @brief 单目相机标定函数：采集棋盘+计算内参畸变+保存xml
 * @param saveXmlPath 输出标定xml文件路径，默认"../camera_calib.xml"
 * @param capIdx      相机设备号，默认0
 * @param width/height 相机预览分辨率
 * @return true标定成功，false失败
 */
bool runCameraCalibration(const std::string& imgSaveDir = "../calib_img", 
                        const std::string& saveXmlPath = "../camera_calib.xml",
                         int capIdx = 0,
                         int width = 640,
                         int height = 480);

//离线从文件夹图片标定
bool runCameraCalibration_FromImages(const std::string& imgDir = "../calib_img", const std::string& saveXmlPath = "../camera_calib.xml");

#endif