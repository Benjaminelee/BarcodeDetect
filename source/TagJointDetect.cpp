#include "../include/TagJointDetect.h"

// 构造函数：初始化公共成员变量
TagJointDetect::TagJointDetect(double tag_size, int rows, int cols)
    : m_tag_size(tag_size)
    , m_board_rows(rows)
    , m_board_cols(cols)
    , m_dx(0.05)
    , m_dy(0.05)
    , m_joint_estimate(true)
    , m_multi_array(true)
    , m_stag(0)
    , m_gridN(4)
{
}

// 设置单/多阵列标志位
void TagJointDetect::setMultiArrayType(bool mutiArray)
{
    m_multi_array = mutiArray;
}

// 设置联合估计标志位
void TagJointDetect::setJointEstimate(bool jointEst)
{
    m_joint_estimate = jointEst;
}

// 设置阵列起始tag值
void TagJointDetect::setStartTagValue(int svalue)
{
    m_stag = svalue;
}

void TagJointDetect::setTagInnerGrids(int gridN) {
    m_gridN = gridN;
}

// 设置tag间距
void TagJointDetect::setTagDist(double dx, double dy)
{
    m_dx = dx;
    m_dy = dy;
    buildBoardObjectPoints();
}

// 设置阵列尺寸
void TagJointDetect::setBoardSize(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        throw std::invalid_argument("Board rows/cols must be positive");
    m_board_rows = rows;
    m_board_cols = cols;
    buildBoardObjectPoints();
}

// 设置相机内参
void TagJointDetect::setCameraParam(const cv::Mat& K, const cv::Mat& D)
{
    // 校验内参K是3x3浮点矩阵
    if (K.type() != CV_32F && K.type() != CV_64F) {
        throw std::invalid_argument("K must be CV_32F/CV_64F");
    }
    if (K.size() != cv::Size(3, 3)) {
        throw std::invalid_argument("K must be 3x3 matrix");
    }
    // 校验畸变系数D维度（4/5/8维）
    if (!D.empty() && D.cols != 1 && D.rows != 4 && D.rows != 5 && D.rows != 8) {
        throw std::invalid_argument("D must be 1x4/1x5/1x8 matrix");
    }
    m_K = K.clone();
    m_D = D.clone();
    m_K_inv = m_K.inv();
    /*cv::Mat Dot = m_K * m_K_inv;
    double a00 = Dot.at<double>(0, 0);
    double a01 = Dot.at<double>(0, 1);
    double a02 = Dot.at<double>(0, 2);
    double a10 = Dot.at<double>(1, 0);
    double a11 = Dot.at<double>(1, 1);
    double a12 = Dot.at<double>(1, 2);
    double a20 = Dot.at<double>(2, 0);
    double a21 = Dot.at<double>(2, 1);
    double a22 = Dot.at<double>(2, 2);
    std::cout << Dot << std::endl;*/
}



// 构建阵列3D点集（公共逻辑）
void TagJointDetect::buildBoardObjectPoints()
{
    m_tag_3d_centers.clear();
    // 阵列中tag ID按行优先排列（0,1,2...cols-1; cols,cols+1...2cols-1...）
    for (int r = 0; r < m_board_rows; ++r)
    {
        for (int c = 0; c < m_board_cols; ++c)
        {
            int tag_id = r * m_board_cols + c;
            // 计算当前tag的3D中心坐标（阵列中心为原点，行方向x，列方向y）
            double x = (c - (m_board_cols - 1) / 2.0) * m_dx;
            double y = (r - (m_board_rows - 1) / 2.0) * m_dy;
            double z = 0.0;
            m_tag_3d_centers[tag_id] = cv::Point3d(x, y, z);
        }
    }
}

// 计算单个tag内部角点（公共逻辑）
void TagJointDetect::generateMarkerInnerCorners(const cv::Mat& gray_img, TagResult& result)
{
    const auto& src = result.corners;
    // 前置合法性校验
    if (src.size() != 4 || m_gridN < 4 || m_gridN > 6)
    {
        result.inners.clear();
        return;
    }
    const int WARP_SIZE = 160;
    std::vector<cv::Point2d> dst = {
        {0.0, 0.0},
        {(double)WARP_SIZE, 0.f},
        {(double)WARP_SIZE, (double)WARP_SIZE},
        {0.f, (double)WARP_SIZE}
    };
    cv::Mat H = cv::getPerspectiveTransform(src, dst);
    double det_H = cv::determinant(H);
    if (fabs(det_H) < 1E-8)
    {
        result.inners.clear();
        return;
    }
    cv::Mat warp;
    cv::warpPerspective(gray_img, warp, H, cv::Size(WARP_SIZE, WARP_SIZE));
    cv::imwrite("../warp_gray.png", warp);

    /*cv::perspectiveTransform(inner_pts, inner_pts, H_inv);
    result.inners = inner_pts;*/
    return;
}

// 计算重投影误差（公共逻辑）
void TagJointDetect::computeImageReprojectionError(BoardResult& result) const {
    std::vector<double> point_errors;
    point_errors.clear();
    if (m_object_pts.empty() || m_image_pts.empty() || m_object_pts.size() != m_image_pts.size())
    {
        return ; // 点数不匹配
    }
    // 1. 将3D点根据位姿投影回图像平面
    std::vector<cv::Point2d> reproj_pts;
    cv::projectPoints(m_object_pts, result.board_rvec, result.board_tvec, m_K, cv::Mat(), reproj_pts);
    point_errors.reserve(m_object_pts.size());
    double total_err = 0.0;
    // 2. 逐点计算欧式像素距离
    for (size_t i = 0; i < m_object_pts.size(); ++i)
    {
        double dx = m_image_pts[i].x - reproj_pts[i].x;
        double dy = m_image_pts[i].y - reproj_pts[i].y;
        double err = std::sqrt(dx * dx + dy * dy);
        point_errors.emplace_back(err);
        total_err += err;
    }
    result.point_errors.swap(point_errors);
    // 3. 返回平均误差
    result.avg_error = total_err / (double)m_object_pts.size();
    // std::cout << "重投影平均误差 avg_error = " << result.avg_error << std::endl;
    return ;
}

// 获取tag行列索引（公共逻辑）
bool TagJointDetect::getTagIndex(const int tag_id, int& row, int& col) const
{
    if (tag_id < 0 || tag_id >= m_board_rows * m_board_cols)
        return false;
    row = tag_id / m_board_cols;
    col = tag_id % m_board_cols;
    return true;
}

int TagJointDetect::getStartTagValue() const
{
    return m_stag;
}

int TagJointDetect::getTagInnerGrids() const
{
    return m_gridN;
}

// 构建图像点集和3D点集（公共逻辑）
bool TagJointDetect::buildImageAndObjectPoints(const std::vector<TagResult>& tag_results) {
    m_image_pts.clear();
    m_object_pts.clear();
    if (m_multi_array) {
        m_image_pts.reserve(tag_results.size() * 4);
        m_object_pts.reserve(tag_results.size() * 4);
        for (const auto& res : tag_results) {
            if (!res.detect_ok) continue;
            int row = -1, col = -1;
            if (!getTagIndex(res.tag_id, row, col))
                continue;
            // 获取当前tag的3D中心
            auto it = m_tag_3d_centers.find(res.tag_id);
            if (it == m_tag_3d_centers.end())
                continue;
            cv::Point3d tag_center_3d = it->second;
            // 单个tag的4个角点相对于中心的3D坐标, y轴向下
            std::vector<cv::Point3d> tag_corners_3d = {
                cv::Point3d(-m_tag_size / 2, -m_tag_size / 2, 0),
                cv::Point3d(m_tag_size / 2, -m_tag_size / 2, 0),
                cv::Point3d(m_tag_size / 2, m_tag_size / 2, 0),
                cv::Point3d(-m_tag_size / 2, m_tag_size / 2, 0)
            };
            // 转换到阵列坐标系
            for (size_t i = 0; i < 4; ++i) {
                m_object_pts.emplace_back(tag_center_3d + tag_corners_3d[i]);
                m_image_pts.emplace_back(res.corners[i]);
            }
        }
    }
    else {
        m_image_pts.reserve(tag_results.size() * m_board_rows * m_board_cols);
        m_object_pts.reserve(tag_results.size() * m_board_rows * m_board_cols);
        double center_x = (m_board_cols + 1) * m_tag_size / 2.0;
        double center_y = (m_board_rows + 1) * m_tag_size / 2.0;
        // 类似棋盘完整3D世界点，Z=0平面，以棋盘中心为3D世界坐标系原点
        for (const auto& res : tag_results) {
            if (!res.detect_ok) continue;
            int row = -1, col = -1;
            if (!getTagIndex(res.tag_id, row, col))
                continue;
            //计算每个Tag角点坐标
            double wx = (col + 1) * m_tag_size;
            double wy = (row + 1) * m_tag_size;
            m_object_pts.emplace_back(wx - center_x, wy - center_y, 0.0);
            int tag_index = row * m_board_cols + col;
            for (const auto& pt : res.corners) {
                m_image_pts.emplace_back(pt);
            }
        }
    }
    // 至少需要4个点对（1个tag）才能进行PnP求解
    return (m_image_pts.size() >= 4 && m_object_pts.size() >= 4);
}

void TagJointDetect::computeImageToCameraJointPose(BoardResult& result) {
    // 填充单tag结果
    result.detected_count = 0;
    for (const auto& res : result.tag_results) {
        if (res.detect_ok)
            result.detected_count++;
    }
    // 联合估计阵列位姿
    if (m_joint_estimate && !m_K.empty() && result.detected_count > 0) {
        // 构建2D图像坐标点到3D世界坐标点对应点对
        if (buildImageAndObjectPoints(result.tag_results)) {
            // 使用SOLVEPNP_SQPNP（鲁棒性更好）求解阵列位姿 Pcam = Rw2c * Pw + T
            cv::solvePnP(m_object_pts, m_image_pts, m_K, cv::Mat(), result.board_rvec, result.board_tvec, false, cv::SOLVEPNP_ITERATIVE);
            //求解出的角度限制在(0,Π)之间
            double theta = cv::norm(result.board_rvec);
            if (theta > CV_PI) {
                cv::Vec3d axis = result.board_rvec / theta;
                double val_ang = 2 * CV_PI - theta;
                result.board_rvec = -val_ang * axis;
            }
            computeImageReprojectionError(result);
            if (result.avg_error < m_threshold) {
                result.board_pose_valid = true;
                updateBoardInformation(result);
            }
        }
    }
    
}

void TagJointDetect::updateBoardInformation(BoardResult& result) const {
    // 构建变换矩阵：阵列->相机
    cv::Mat R_w2c;
    cv::Rodrigues(result.board_rvec, R_w2c);
    const cv::Mat R_c2w = R_w2c.t(); // 相机->世界旋转矩阵
    const cv::Vec3d& t_w2c = result.board_tvec;
    const cv::Mat t_w2c_mat = (cv::Mat_<double>(3, 1) << t_w2c[0], t_w2c[1], t_w2c[2]);
    const cv::Mat t_c2w_mat = -R_c2w * t_w2c_mat; // 相机光心世界平移
    // 构造 T_board2cam 4x4外参变换矩阵T
    cv::Mat T_board2cam = cv::Mat::eye(4, 4, CV_64F);
    R_w2c.copyTo(T_board2cam(cv::Rect(0, 0, 3, 3)));
    T_board2cam.at<double>(0, 3) = t_w2c[0];
    T_board2cam.at<double>(1, 3) = t_w2c[1];
    T_board2cam.at<double>(2, 3) = t_w2c[2];
    // 相机在标定板阵列坐标系下的rvec/tvec
    cv::Vec3d camera_rvec;
    cv::Rodrigues(R_c2w, camera_rvec);
    result.camera_rvec = camera_rvec;
    result.camera_tvec = cv::Vec3d(
        t_c2w_mat.at<double>(0),
        t_c2w_mat.at<double>(1),
        t_c2w_mat.at<double>(2)
    );
    // 更新每个Tag的相机位姿
    for (auto& tag_res : result.tag_results) {
        if (!tag_res.detect_ok) continue;
        int row = -1, col = -1;
        if (!getTagIndex(tag_res.tag_id, row, col))
            continue;
        // 查找tag世界中心3D点
        auto it = m_tag_3d_centers.find(tag_res.tag_id);
        if (it == m_tag_3d_centers.end())
            continue;
        const cv::Point3f& tag_center_3d = it->second;
        // 栈上局部齐次4x1向量，避免Mat_动态分配
        double tag_homo[4] = { tag_center_3d.x, tag_center_3d.y, tag_center_3d.z, 1.0 };
        cv::Mat tag_center_homo(4, 1, CV_64F, tag_homo);
        // tag中心转到相机坐标系
        cv::Mat tag_center_cam = T_board2cam * tag_center_homo;
        // Tag与标定板共面，旋转完全一致
        tag_res.rvec = result.board_rvec;
        tag_res.tvec = cv::Vec3d(
            tag_center_cam.at<double>(0),
            tag_center_cam.at<double>(1),
            tag_center_cam.at<double>(2)
        );
        tag_res.pose_valid = true;
    }
    // 平面约束使用 R_c2w 第三行
    const double r20 = R_c2w.at<double>(2, 0);
    const double r21 = R_c2w.at<double>(2, 1);
    const double r22 = R_c2w.at<double>(2, 2);
    // 预提取R_c2w全部9个系数，手动旋转展开提速
    const double r00 = R_c2w.at<double>(0, 0);
    const double r01 = R_c2w.at<double>(0, 1);
    const double r02 = R_c2w.at<double>(0, 2);
    const double r10 = R_c2w.at<double>(1, 0);
    const double r11 = R_c2w.at<double>(1, 1);
    const double r12 = R_c2w.at<double>(1, 2);
    // 预提取t_c2w全部3个系数，手动旋转展开提速
    const double tc2w_x = t_c2w_mat.at<double>(0);
    const double tc2w_y = t_c2w_mat.at<double>(1);
    const double tc2w_z = t_c2w_mat.at<double>(2);

    const double numerator_const = -(r20 * tc2w_x + r21 * tc2w_y + r22 * tc2w_z);
    // 循环复用临时矩阵，避免反复分配内存
    cv::Mat uv_h(3, 1, CV_64F);
    cv::Mat ray_c(3, 1, CV_64F);
    for (auto& tag_res : result.tag_results) {
        if (!tag_res.detect_ok) continue;
        tag_res.predicts.clear();
        tag_res.predicts.reserve(tag_res.corners.size());
        for (const auto& corner : tag_res.corners) {
            // 像素齐次坐标，直接写入预分配uv_h
            const double u = static_cast<double>(corner.x);
            const double v = static_cast<double>(corner.y);
            // 像素 -> 相机归一化射线
            uv_h.at<double>(0) = u;
            uv_h.at<double>(1) = v;
            uv_h.at<double>(2) = 1.0;
            ray_c = m_K_inv * uv_h;
            
            const double* r_ptr = ray_c.ptr<double>();
            const double rx = r_ptr[0];
            const double ry = r_ptr[1];
            const double rz = r_ptr[2];
            const double denom = r20 * rx + r21 * ry + r22 * rz;
            if (fabs(denom) < 1E-8) {
                continue; // 射线几乎平行标定板平面，无有效交点，跳过防止Inf/NaN
            }
            // 原始未修正深度,后续需要Z残差反向补偿
            const double s0 = numerator_const / denom;
            if (s0 < 1E-3) {
                continue; // 过滤相机后方、深度极小的无效点
            }
            // 用原始s0计算Z向浮点残差δ_z
            const double pc_x0 = s0 * rx;
            const double pc_y0 = s0 * ry;
            const double pc_z0 = s0 * rz;
            const double delta_z = r20 * pc_x0 + r21 * pc_y0 + r22 * pc_z0 + tc2w_z;
            // 计算深度修正量Δs
            const double delta_s = -delta_z / denom;
            // 修正后高精度深度
            const double s_fix = s0 + delta_s;
            // 使用修正后的s_fix重新计算相机坐标系点
            const double pc_x = s_fix * rx;
            const double pc_y = s_fix * ry;
            const double pc_z = s_fix * rz;
            // 相机坐标系转标定板世界坐标系，手动展开
            double pw_x = r00 * pc_x + r01 * pc_y + r02 * pc_z + tc2w_x;
            double pw_y = r10 * pc_x + r11 * pc_y + r12 * pc_z + tc2w_y;
            double pw_z = r20 * pc_x + r21 * pc_y + r22 * pc_z + tc2w_z;
            //pw_z = 0.0; // 抹平浮点微小误差，强制落在板面Z=0
            // 存入float世界坐标
            tag_res.predicts.emplace_back(
                static_cast<double>(pw_x),
                static_cast<double>(pw_y),
                static_cast<double>(pw_z)
            );
        }
    }

    // 计算每个tag的平均投影误差
    if (result.tag_results.size() == 0) return;
    if (m_object_pts.size() % result.tag_results.size() != 0) return;
    int nums = m_object_pts.size() / result.tag_results.size();
    for (int i = 0; i < result.tag_results.size(); ++i) {
        auto& tag_res = result.tag_results[i];
        double single_err = 0.0;
        for (int j = 0; j < tag_res.predicts.size(); ++j) {
            int k = nums * i + j;
            const auto& mp = m_object_pts[k];
            const auto& pre = tag_res.predicts[j];
            cv::Point3d diff = mp - pre;
            single_err += cv::norm(diff);
        }
        tag_res.single_err = single_err;
    }
}

// 绘制实现：绘制单个tag + 阵列坐标轴
void TagJointDetect::drawBoardResult(cv::Mat& dst, const BoardResult& res)const
{
    // 绘制单个tag
    if (res.tag_results.size() == 0) return;
    int nums = m_object_pts.size() / res.tag_results.size();
    for (auto& item : res.tag_results)
    {
        if (!item.detect_ok) continue;
        // 画四角边框
        for (size_t i = 0; i < nums; i++)
        {
            cv::line(dst, item.corners[i], item.corners[(i + 1) % nums], cv::Scalar(0, 255, 0), 1);
        }
        // 绘制ID文字
        cv::putText(dst, std::to_string(item.value_id), item.corners[0],
            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);
    }

    // 绘制阵列坐标轴（在阵列中心）
    if (res.board_pose_valid && !m_K.empty())
    {
        // 计算阵列中心的2D投影
        cv::Point3d board_center_3d(0, 0, 0);
        std::vector<cv::Point3d> board_center_3d_vec = { board_center_3d };
        std::vector<cv::Point2d> board_center_2d;
        cv::projectPoints(board_center_3d_vec, res.board_rvec, res.board_tvec,
            m_K, m_D, board_center_2d);
        // 绘制阵列坐标轴（长度为tag尺寸的0.5倍）
        double axis_length = m_tag_size * 0.5;
        cv::drawFrameAxes(dst, m_K, m_D, res.board_rvec, res.board_tvec, axis_length);
        // 绘制阵列中心标记
        cv::circle(dst, board_center_2d[0], 5, cv::Scalar(255, 0, 0), -1);
    }
}