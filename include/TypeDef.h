#ifndef TYPE_DEF_H
#define TYPE_DEF_H

#include "opencv2/core/types.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d.hpp"
#include <string>
#include <vector>

// 检测条码类型
enum class TagType
{
    ARUCO_MOD, ///< opencv aruco模块
    ARUCO_TAG, ///< aruco原生库
    APRIL_TAG, ///< april原生库
    CHESS_BOARD ///<棋盘格单阵列
};

enum class AprilTagFamily {
    TAG_36H11,
    TAG_16H5,
    TAG_25H9,
    TAG_36H10
};

//Tag检测配置参数表
struct TagDetectParams {
    std::string calibPath = "";
    TagType tagType = TagType::APRIL_TAG;
    int dictId = 0;    ///< 0对应cv::aruco::PredefinedDictionaryType::DICT_4X4_50
    std::string arucoDictStr = "ARUCO_MIP_36h12";  ///< aruco原生库Tag字典类型
    AprilTagFamily aprilFamily = AprilTagFamily::TAG_36H11; ///< april原生库Tag字典类型
    double tagSize = 0.02255;   ///< 阵列的每个Tag边长
    int rows = 4;   ///< 阵列的行数
    int cols = 5;   ///< 阵列的列数
    double tagDistX = 0.0451;   ///< 行方向上相邻两个Tag中心间距
    double tagDistY = 0.0451;   ///< 列方向上相邻两个Tag中心间距
    int arucoModStart = 0;      ///< opencv的arucoTag起始码
    int arucoTagStart = 0;      ///< aruco原生库Tag起始码
    int aprilTagStart = 560;    ///< april原生库Tag起始码
};

// 单Tag检测结果结构体：ID、角点、位姿
struct TagResult {
    int tag_id = -1;                       //每个tag的编号
    int value_id = -1;                     //每个tag对应解码值
    std::vector<cv::Point2d> corners;      // 图像四个角点(像素)
    std::vector<cv::Point2d> inners;       // 图像内部角点(像素)
    std::vector<cv::Point3d> predicts;     // 图像角点的3D预测坐标
    cv::Vec3d rvec, tvec;                  // 旋转向量、平移向量(相机坐标系)
    bool detect_ok = false;
    bool pose_valid = false;
    double single_err = 0.0;
    TagResult()
    {
        corners.clear();
        inners.clear();
        detect_ok = false;
        tag_id = -1;
        value_id = -1;
        pose_valid = false;
        rvec.zeros();
        tvec.zeros();
    }
};

// 多阵列联合估计结果
struct BoardResult {
    std::vector<TagResult> tag_results;    // 所有检测到的单tag结果
    cv::Vec3d board_rvec, board_tvec;      // 整个阵列的联合估计位姿
    bool board_pose_valid = false;         // 阵列位姿是否有效
    int detected_count = 0;                // 检测到的tag数量
    cv::Vec3d camera_rvec, camera_tvec;    // 整个相机的联合估计位姿
    std::vector<double> point_errors;             // 整个阵列的各个角点误差
    double avg_error = 0.0f;
    BoardResult() {
        tag_results.clear();
        board_rvec.zeros();
        board_tvec.zeros();
        board_pose_valid = false;
        detected_count = 0;
        camera_rvec.zeros();
        camera_tvec.zeros();
        point_errors.clear();
    }
};

#endif //TYPE_DEF_H