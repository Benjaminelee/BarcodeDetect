#ifndef APRIL_TAG_JOINT_DETECT_H
#define APRIL_TAG_JOINT_DETECT_H

// 原生Apriltag头文件
#include "apriltag/apriltag.h"
#include "apriltag/tag36h11.h"
#include "apriltag/tag16h5.h"
#include "apriltag/tag25h9.h"
#include "apriltag/tag36h10.h"
#include "apriltag/common/zarray.h"

#include "TagJointDetect.h"

class AprilTagJointDetect : public TagJointDetect
{
public:
    // 构造：新增AprilTag特有参数（tag类型），复用基类构造
    AprilTagJointDetect(AprilTagFamily apr_family = AprilTagFamily::TAG_36H11,
        double tag_size = 0.05, int rows = 2, int cols = 3);

    // 设置AprilTag检测器参数（子类特有参数）
    void setAprilParams(float quad_decimate, float quad_sigma, int refine_edges, float decode_sharpening);

    // 析构：释放Apriltag C资源（子类特有）
    ~AprilTagJointDetect() override;

    // ========== 实现基类纯虚函数 ==========
    // 单帧检测（AprilTag特有逻辑）
    BoardResult detect(const cv::Mat& img) override;

private:
    // ========== AprilTag特有成员 ==========
    apriltag_detector* m_apr_det = nullptr;
    apriltag_family* m_apr_family = nullptr;
};

#endif // APRIL_TAG_JOINT_DETECT_H