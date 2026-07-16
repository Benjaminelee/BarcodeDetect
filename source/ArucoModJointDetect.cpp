#include "../include/ArucoModJointDetect.h"
#include <cmath>

ArucoModJointDetect::ArucoModJointDetect(int dict_id, double tag_size, int rows, int cols)
    :TagJointDetect(tag_size, rows, cols)
{
    // 初始化阵列3D点集
    buildBoardObjectPoints();
    // ArUco初始化
    cv::aruco::Dictionary aruco_dict = cv::aruco::getPredefinedDictionary(dict_id);
    m_aruco_param = cv::aruco::DetectorParameters();
    //设置每个tag内部格子点数
    setTagInnerGrids(aruco_dict.markerSize);
    m_aruco_param.markerBorderBits = 1;
    // 新增：提升检测鲁棒性的关键参数
    //m_aruco_param.adaptiveThreshWinSizeMin = 3;       // 自适应阈值窗口最小尺寸
    //m_aruco_param.adaptiveThreshWinSizeMax = 23;      // 自适应阈值窗口最大尺寸
    //m_aruco_param.adaptiveThreshWinSizeStep = 10;     // 窗口步长
    //m_aruco_param.adaptiveThreshConstant = 7;         // 自适应阈值常数
    //m_aruco_param.minMarkerPerimeterRate = 0.01;      // 最小标记周长（相对图像尺寸），过滤小标记
    //m_aruco_param.maxMarkerPerimeterRate = 4.0;       // 最大标记周长，过滤超大噪声
    //m_aruco_param.polygonalApproxAccuracyRate = 0.05; // 多边形逼近精度
    //m_aruco_param.minCornerDistanceRate = 0.05;       // 角点最小距离，过滤伪角点
    //m_aruco_param.minDistanceToBorder = 3;            // 角点离图像边界最小距离
    //m_aruco_param.minMarkerDistanceRate = 0.05;       // 标记间最小距离
    //// 亚像素优化：提升角点定位精度（核心）
    m_aruco_param.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX; // 亚像素细化
    m_aruco_param.cornerRefinementWinSize = 5;     // 亚像素窗口尺寸
    m_aruco_param.cornerRefinementMaxIterations = 30; // 亚像素迭代次数
    m_aruco_param.cornerRefinementMinAccuracy = 0.1; // 亚像素收敛精度
    m_aruco_detector = cv::aruco::ArucoDetector(aruco_dict, m_aruco_param);
}

BoardResult ArucoModJointDetect::detect(const cv::Mat& img)
{
    BoardResult board_res;
    std::vector<TagResult> tag_results;
    tag_results.clear();
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    {
        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        m_aruco_detector.detectMarkers(gray, corners, ids);
        int startV = getStartTagValue();
        for (size_t i = 0; i < ids.size(); i++)
        {
            TagResult res{};
            res.corners.clear();
            res.inners.clear();
            res.tag_id = ids[i] - startV;
            res.value_id = ids[i];
            // 填入4个角点，左上，右上，右下，左下
            // [0]左上───────[1]右上
            //       │            │
            //       │            │
            //       │   Tag区域  │
            //       │            │
            //       │            │
            // [3]左下───────[2]右下
            //res.corners.assign(corners[i].begin(), corners[i].end());
            for (const auto& p : corners[i]) {
                res.corners.emplace_back(cv::Point2d(p.x, p.y));
            }
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

void ArucoModJointDetect::setArucoParams(const cv::aruco::DetectorParameters& params)
{
    m_aruco_param = params;
}