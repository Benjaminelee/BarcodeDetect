#include "../include/ArucoTagJointDetect.h"
#include <cmath>

ArucoTagJointDetect::ArucoTagJointDetect(aruco::Dictionary::DICT_TYPES dict_type, double tag_size, int rows, int cols)
    :TagJointDetect(tag_size, rows, cols) {
    // 初始化阵列3D点集
    buildBoardObjectPoints();
    if (dict_type < 0 || dict_type > 14) dict_type = aruco::Dictionary::DICT_TYPES::ARUCO_MIP_36h12;
    //aruco::Dictionary::DICT_TYPES dict_type = static_cast<aruco::Dictionary::DICT_TYPES>(dict_id);
    aruco::Dictionary aruco_dict = aruco::Dictionary::loadPredefined(dict_type);
    uint64_t total = aruco_dict.size(); // 返回 250
    // 设置每个tag内部格子点数（适配字典位数）
    int inner_grid_count = std::sqrt(aruco_dict.nbits());
    setTagInnerGrids(inner_grid_count);
    // 角点细化配置（原生库枚举类型）
    m_aruco_param.cornerRefinementM = aruco::CORNER_SUBPIX; // 亚像素细化
    m_aruco_param.markerWarpPixSize = 5; // 标记透视变换尺寸
    m_aruco_param.borderDistThres = 0.015f; // 边界安静区校验距离阈值
    m_aruco_param.closingSize = 0; // 形态学闭运算尺寸
    m_aruco_param.maxThreads = -1; // 最大线程数
    // 阈值参数配置（原生库阈值方法）
    auto thresMethod = m_aruco_param.getCornerThresMethodFromString("THRES_ADAPTIVE");
    m_aruco_param.thresMethod = thresMethod;
    m_aruco_param.AdaptiveThresWindowSize = -1; // 自适应阈值窗口尺寸
    m_aruco_param.ThresHold = 7; // 自适应阈值常数
    m_aruco_param.AdaptiveThresWindowSize_range = 0;

    //构造marker检测器，及参数配置和检测模式
    m_marker_detector.setParameters(m_aruco_param);
    m_marker_detector.setDictionary(dict_type, 0.0f);
    //设置快速模式，最小marker尺寸为图像2%
    m_marker_detector.setDetectionMode(aruco::DM_FAST, 0.02f);
}

BoardResult ArucoTagJointDetect::detect(const cv::Mat& img)
{
    BoardResult board_res;
    std::vector<TagResult> tag_results;
    tag_results.clear();
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    {
        std::vector<aruco::Marker> markers;
        markers = m_marker_detector.detect(gray);
        int startV = getStartTagValue();
        for (const auto& marker : markers) {
            if (!marker.isValid()) {
                continue;
            }
            TagResult res{};
            res.corners.clear();
            res.inners.clear();
            res.tag_id = marker.id - startV;
            res.value_id = marker.id;
            if (res.value_id >= 250) {
                continue;
            }
            // 填入4个角点，左上，右上，右下，左下
            // [0]左上───────[1]右上
            //       │            │
            //       │            │
            //       │   Tag区域  │
            //       │            │
            //       │            │
            // [3]左下───────[2]右下
            for (const auto& p : marker) {
                res.corners.emplace_back(cv::Point2d(p.x, p.y));
            }
            //res.corners.emplace_back(marker[0]);
            //res.corners.emplace_back(marker[1]);
            //res.corners.emplace_back(marker[2]);
            //res.corners.emplace_back(marker[3]);
            res.detect_ok = true;
            //generateMarkerInnerCorners(gray, res);
            tag_results.emplace_back(res);
        }
    }
    board_res.tag_results = tag_results;
    //估计阵列的相机位姿
    computeImageToCameraJointPose(board_res);
    return board_res;
}

void ArucoTagJointDetect::setArucoParams(const aruco::MarkerDetector::Params& params)
{
    m_aruco_param = params;
}