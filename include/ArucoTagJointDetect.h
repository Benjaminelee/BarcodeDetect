#ifndef ARUCO_TAG_JOINT_DETECT_H
#define ARUCO_TAG_JOINT_DETECT_H

//原生Aruco头文件
#include "aruco/aruco.h"
#include "aruco/marker.h"
#include "aruco/dictionary.h"
#include "aruco/markerdetector.h"

#include "TagJointDetect.h"

class ArucoTagJointDetect : public TagJointDetect
{
public:
    // 构造：新增ArUco特有参数（字典ID），dict_id映射为原生ArUco的DICT_TYPES枚举
    ArucoTagJointDetect(aruco::Dictionary::DICT_TYPES dict_type = aruco::Dictionary::ARUCO_MIP_36h12,
        double tag_size = 0.05, int rows = 2, int cols = 3);

    // 设置ArUco检测器参数（适配原生MarkerDetector的参数）
    void setArucoParams(const aruco::MarkerDetector::Params& params);

    // 析构：默认实现（子类特有）
    ~ArucoTagJointDetect() override = default;

    // ========== 实现基类纯虚函数 ==========
    // 单帧检测（ArUco特有逻辑）
    BoardResult detect(const cv::Mat& img) override;

private:
    // ========== ArUco特有成员 ==========
    aruco::MarkerDetector::Params m_aruco_param; //原生ArUco参数表
    aruco::MarkerDetector m_marker_detector; //原生Aruco marker检测器
};

#endif // ARUCO_TAG_JOINT_DETECT_H