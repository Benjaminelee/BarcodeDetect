#ifndef ARUCO_OBJ_JOINT_DETECT_H
#define ARUCO_OBJ_JOINT_DETECT_H

#include <opencv2/objdetect.hpp>
#include <opencv2/objdetect/aruco_board.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>

#include "TagJointDetect.h"

class ArucoModJointDetect : public TagJointDetect
{
public:
    // 构造：新增ArUco特有参数（字典ID），复用基类构造
    ArucoModJointDetect(int dict_id = cv::aruco::DICT_4X4_50,
        double tag_size = 0.05, int rows = 2, int cols = 3);

    // 设置ArUco检测器参数（子类特有参数）
    void setArucoParams(const cv::aruco::DetectorParameters& params);

    // 析构：默认实现（子类特有）
    ~ArucoModJointDetect() override = default;

    // ========== 实现基类纯虚函数 ==========
    // 单帧检测（ArUco特有逻辑）
    BoardResult detect(const cv::Mat& img) override;

private:
    // ========== ArUco特有成员 ==========
    cv::aruco::DetectorParameters m_aruco_param;
    cv::aruco::ArucoDetector m_aruco_detector;
};

#endif // ARUCO_OBJ_JOINT_DETECT_H