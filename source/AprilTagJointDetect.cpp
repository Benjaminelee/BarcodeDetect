#include "../include/AprilTagJointDetect.h"
#include <cmath>

AprilTagJointDetect::AprilTagJointDetect(AprilTagFamily apr_family, double tag_size, int rows, int cols)
    :TagJointDetect(tag_size, rows, cols) {
    // 初始化阵列3D点集
    buildBoardObjectPoints();
    if (apr_family == AprilTagFamily::TAG_36H11) {
        m_apr_family = tag36h11_create();
    }
    else if (apr_family == AprilTagFamily::TAG_16H5) {
        m_apr_family = tag16h5_create();
    }
    else if (apr_family == AprilTagFamily::TAG_25H9) {
        m_apr_family = tag25h9_create();
    }
    else if (apr_family == AprilTagFamily::TAG_36H10) {
        m_apr_family = tag36h10_create();
    }
    else {
        m_apr_family = tag36h11_create();
    }
    m_apr_det = apriltag_detector_create();
    //设置每个tag内部格子点数
    setTagInnerGrids(m_apr_family->width_at_border - 2); 
    apriltag_detector_add_family(m_apr_det, m_apr_family);
    m_apr_det->nthreads = 4;
    // 调高亚像素精度
    m_apr_det->quad_decimate = 1.5f;        // 四边形检测阶段的图像下采样系数（1.0=不采样，精度最高，速度最慢）
    m_apr_det->quad_sigma = 0.0f;           // 四边形检测阶段的高斯模糊标准差sigma（0=不平滑，高纹理场景可设0.5）
    m_apr_det->refine_edges = true;         // 是否调整四边形边缘以贴合强梯度（关键：提升角点精度）
    m_apr_det->decode_sharpening = 0.25;    // 解码阶段的图像锐化程度（默认0.25，可调0.1~0.5）
    m_apr_det->debug = false;               // 是否输出检测过程中的调试图像（保存到当前目录）
    //四边形阈值参数，用于筛选“候选四边形”（排除非标签的无效轮廓）
    //m_apr_det->qtp.min_cluster_pixels = 10;         // 拒绝像素数过少的候选四边形
    //m_apr_det->qtp.max_nmaxima = 10;                // 分割像素团为四边形时考虑的角点候选数
    //m_apr_det->qtp.critical_rad = 10 * CV_PI / 180;  // 边夹角临界角度（过滤畸形四边形）
    //m_apr_det->qtp.max_line_fit_mse = 1.0;          // 直线拟合最大均方误差（过滤噪声）
    //m_apr_det->qtp.min_white_black_diff = 5;        // 黑白像素模型的最小亮度差（过滤低对比度）
    //m_apr_det->qtp.deglitch = 0;                    // 是否对阈值化图像去噪（仅极噪图像有用）
}

// 析构：释放C内存，防泄漏
AprilTagJointDetect::~AprilTagJointDetect()
{
    if (m_apr_det) apriltag_detector_destroy(m_apr_det);
    if (m_apr_family)
    {
        // 根据不同的tag family释放
        if (m_apr_family == tag36h11_create()) tag36h11_destroy(m_apr_family);
        else if (m_apr_family == tag16h5_create()) tag16h5_destroy(m_apr_family);
        else if (m_apr_family == tag25h9_create()) tag25h9_destroy(m_apr_family);
        else if (m_apr_family == tag36h10_create()) tag36h10_destroy(m_apr_family);
    }
}

BoardResult AprilTagJointDetect::detect(const cv::Mat& img)
{
    BoardResult board_res;
    std::vector<TagResult> tag_results;
    tag_results.clear();
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    {
        // 原生Apriltag检测
        image_u8_t im{ gray.cols, gray.rows, gray.cols, gray.data };
        zarray_t* dets = apriltag_detector_detect(m_apr_det, &im);
        int startV = getStartTagValue();
        for (int i = 0; i < zarray_size(dets); i++)
        {
            apriltag_detection* det;
            zarray_get(dets, i, &det);
            TagResult res{};
            res.corners.clear();
            res.inners.clear();
            res.tag_id = det->id - startV;
            res.value_id = det->id;
            res.detect_ok = true;
            // 填入4个角点，左上，右上，右下，左下
            // [0]左上───────[1]右上
            //       │            │
            //       │            │
            //       │   Tag区域  │
            //       │            │
            //       │            │
            // [3]左下───────[2]右下
            // det->p[1] = 左上
            res.corners.emplace_back(det->p[1][0], det->p[1][1]);
            // det->p[0] = 右上
            res.corners.emplace_back(det->p[0][0], det->p[0][1]);
            // det->p[3] = 右下
            res.corners.emplace_back(det->p[3][0], det->p[3][1]);
            // det->p[2] = 左下
            res.corners.emplace_back(det->p[2][0], det->p[2][1]);
            // 亚像素角点细化,注释掉重投影误差更小
            /*bool all_in_img = true;
            cv::Size img_sz = gray.size();
            for (auto& pt : res.corners)
            {
                if (pt.x < 0 || pt.y < 0 || pt.x >= img_sz.width || pt.y >= img_sz.height)
                {
                    all_in_img = false;
                    break;
                }
            }
            if (all_in_img)
            {
                cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 20, 0.0005);
                std::vector<cv::Point2f> pts_f(res.corners.begin(), res.corners.end());
                cv::cornerSubPix(gray, pts_f, cv::Size(2, 2), cv::Size(-1, -1), criteria);
                res.corners.assign(pts_f.begin(), pts_f.end());
            }*/
            //generateMarkerInnerCorners(gray, res);
            tag_results.emplace_back(res);
        }
        apriltag_detections_destroy(dets);
    }
    board_res.tag_results = tag_results;
    //估计阵列的相机位姿
    computeImageToCameraJointPose(board_res);
    return board_res;
}

void AprilTagJointDetect::setAprilParams(float quad_decimate, float quad_sigma, int refine_edges, float decode_sharpening)
{
    if (m_apr_det) {
        m_apr_det->quad_decimate = quad_decimate;
        m_apr_det->quad_sigma = quad_sigma;
        m_apr_det->refine_edges = refine_edges;
        m_apr_det->decode_sharpening = decode_sharpening;
    }
}