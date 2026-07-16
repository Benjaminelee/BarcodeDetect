#ifndef TAG_JOINT_DETECT_H
#define TAG_JOINT_DETECT_H

#include "TypeDef.h"

class TagJointDetect
{
public:
    // 构造：公共参数初始化（tag尺寸、阵列行列数）
    TagJointDetect(double tag_size = 0.05, int rows = 2, int cols = 3);

    // 虚析构：确保子类析构被正确调用
    virtual ~TagJointDetect() = default;

    // ========== 公共接口（子类复用实现） ==========
    // 设置联合估计标志位
    void setJointEstimate(bool jointEst);

    // 设置阵列marker起始tag数值
    void setStartTagValue(int svalue);

    // 设置单个tag内部格子数
    void setTagInnerGrids(int gridN);

    // 设置marker之间间距（行/列相邻marker中心间距）
    void setTagDist(double dx, double dy);

    // 设置阵列尺寸（行数、列数）
    void setBoardSize(int rows, int cols);

    // 设置相机内参+畸变系数（从标定xml读取）
    void setCameraParam(const cv::Mat& K, const cv::Mat& D);

    // 公共绘图接口（默认实现）
    void drawBoardResult(cv::Mat& dst, const BoardResult& res) const;

    // ========== 纯虚函数（子类差异化实现） ==========
    // 单帧检测：输入校正后无畸变图像，输出阵列联合估计结果
    virtual BoardResult detect(const cv::Mat& img) = 0;

protected:
    // ========== 公共工具函数（子类可调用） ==========
    // 构建阵列的3D点集（用于联合估计）
    void buildBoardObjectPoints();

    // 计算单个tag内部角点像素坐标（复用函数）
    void generateMarkerInnerCorners(const cv::Mat& gray_img, TagResult& result);

    // 计算图像坐标系角点重投影误差
    void computeImageReprojectionError(BoardResult& result) const;

    // 从检测到的tag构建图像点集和对应的3D点集
    bool buildImageAndObjectPoints(const std::vector<TagResult>& tag_results);

    // 从检测的图像点集和对应的3D点集估计联合位姿
    void computeImageToCameraJointPose(BoardResult& result);

    // 根据tag id获取其在阵列中的行列索引
    bool getTagIndex(const int tag_id, int& row, int& col) const;
    
    // 获取阵列起始编码数值
    int getStartTagValue() const;

    // 获取单个tag内部格子数
    int getTagInnerGrids() const;

    // 更新board阵列及各个角点信息
    void updateBoardInformation(BoardResult& result) const;

private:
    // ========== 私有成员变量（子类不可访问） ==========
    double m_tag_size;                ///< 单个marker物理边长(m)
    int m_board_rows;                 ///< 阵列行数
    int m_board_cols;                 ///< 阵列列数
    double m_dx;                      ///< 行相邻marker中心间距(m)
    double m_dy;                      ///< 列相邻marker中心间距(m)
    bool m_joint_estimate;            ///< 是否启用联合估计

    int m_stag;                       ///< 阵列起始码数值
    int m_gridN;                      ///< 内部格子数（m_gridN * m_gridN）
    cv::Mat m_K, m_D, m_K_inv;        ///< 内参K、畸变D矩阵、内参逆矩阵
    double m_threshold = 5.0;         ///< 重投影误差阈值（过滤无效检测）
    std::unordered_map<int, cv::Point3d> m_tag_3d_centers; // tag id -> 3D中心坐标
    std::vector<cv::Point2d> m_image_pts; //RGB相机检测到的所有2D图像角点坐标
    std::vector<cv::Point3d> m_object_pts; //预定义的3D世界坐标系中所有角点3D坐标
};

#endif // TAG_JOINT_DETECT_H