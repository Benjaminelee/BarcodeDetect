#include "../include/VizBoardViewer.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

// 基于 tagSize = 0.02255 优化后的全套常量
constexpr double WORLD_AXIS_SIZE = 0.10;        // 世界坐标系轴长
constexpr double WORLD_AXIS_WIDTH = 0.03;       // 世界坐标系轴宽度
constexpr double CAMERA_AXIS_SIZE = 0.008;      // 相机局部坐标系轴长
constexpr double CAM_BOX_OPACITY = 0.5;         // 相机立方体透明度(保留原值，视觉舒适)
constexpr double FRUSTUM_SCALE = 0.5;           // 视锥体缩放，避免过大遮挡画面
constexpr double GLOBAL_ERR_TEXT_H = 0.25;      // 全局误差文字高度
constexpr double GLOBAL_ERR_TEXT_SZ = 0.25;     // 全局文字字号
constexpr double TAG_SPHERE_RATIO = 0.05;       // Tag角点小球相对比例
constexpr int    TAG_SPHERE_RES = 8;            // 小球分段(不变)
constexpr double TAG_ID_SZ = 0.18;              // Tag ID 文字大小
constexpr double TAG_ERR_SZ = 0.10;             // 单个Tag误差文字大小
constexpr double TAG_ERR_H = 0.012;             // Tag误差文字离地高度
constexpr int    SPIN_DELAY_MS = 1;             // 刷新延时(不变)
constexpr int    BUF_MAX = 64;                  // 缓冲区(不变)

VizBoardViewer::VizBoardViewer(double tag_size, const cv::Mat& K, const cv::Size& img_size, int rows, int cols)
    : m_tag_size(tag_size), m_viz_win("Tag阵列全局3D视图"), m_rows(rows), m_cols(cols) {
    // 入参校验
    if (tag_size <= 0)
        throw std::invalid_argument("tag_size must > 0");
    if (K.empty() || K.rows != 3 || K.cols != 3)
        throw std::invalid_argument("Invalid camera intrinsic K");
    if (img_size.width <= 0 || img_size.height <= 0)
        throw std::invalid_argument("Invalid image size");
    m_K = K.clone();
    m_img_size = img_size;
    initStaticScene(K, img_size);
    m_viz_win.setViewerPose(cv::Affine3d::Identity());
    m_viz_win.setBackgroundColor(cv::viz::Color::black());
}

VizBoardViewer::~VizBoardViewer()
{
    m_viz_win.removeAllWidgets();
    if (!m_viz_win.wasStopped())
        m_viz_win.close();
}

void VizBoardViewer::closeWindow()
{
    if (!m_viz_win.wasStopped())
        m_viz_win.close();
}

bool VizBoardViewer::isWindowClosed() const
{
    return m_viz_win.wasStopped();
}

void VizBoardViewer::clearDynamicTagWidgets()
{
    for (const auto& name : m_dynamic_tag_widgets)
    {
        if (!m_viz_win.wasStopped())
            m_viz_win.removeWidget(name);
    }
    m_dynamic_tag_widgets.clear();
}

void VizBoardViewer::initStaticScene(const cv::Mat& K, const cv::Size& img_size)
{
    m_static_widgets = { "world_axis", "board_plane", "board_grid" };
    // 1. 世界坐标系
    cv::viz::WCoordinateSystem world_axis(WORLD_AXIS_SIZE);
    world_axis.setRenderingProperty(cv::viz::LINE_WIDTH, WORLD_AXIS_WIDTH);
    m_viz_win.showWidget("world_axis", world_axis);
    // 2. 标定板平面
    cv::viz::WPlane board_plane(cv::Size2d(m_tag_size * m_cols * 2, m_tag_size * m_rows * 2), cv::viz::Color::gray());
    // 设置半透明，取值范围 0.0(完全透明) ~ 1.0(完全不透明)
    board_plane.setRenderingProperty(cv::viz::OPACITY, 0.3);
    m_viz_win.showWidget("board_plane", board_plane);
    // 3. 标定板网格
    cv::viz::WGrid board_grid(cv::Vec2i(m_cols, m_rows), cv::Vec2d(m_tag_size * 2, m_tag_size * 2), cv::viz::Color::white());
    board_grid.setRenderingProperty(cv::viz::OPACITY, 0.5);
    m_viz_win.showWidget("board_grid", board_grid);

    // 4. 相机立方体（静态控件，仅初始化一次）
    cv::viz::WCube cam_box(
        cv::Point3d(-m_tag_size * 0.5, -m_tag_size * 0.4, -m_tag_size * 0.2),
        cv::Point3d(m_tag_size * 0.5, m_tag_size * 0.4, m_tag_size * 0.2),
        true, cv::viz::Color::yellow()
    );
    cam_box.setRenderingProperty(cv::viz::OPACITY, CAM_BOX_OPACITY);
    m_viz_win.showWidget("camera_cube", cam_box);

    // 5. 相机局部坐标系（静态控件）
    m_viz_win.showWidget("camera_axis", cv::viz::WCoordinateSystem(CAMERA_AXIS_SIZE));

    // 6. 相机视锥体（静态控件）
    cv::Matx33d K_matx(K);
    cv::viz::WCameraPosition cam_frust(K_matx, FRUSTUM_SCALE, cv::viz::Color::yellow());
    m_viz_win.showWidget("cam_frust", cam_frust);

    // 删除相机控件，仅保留Tag动态控件列表 ==========
    m_dynamic_tag_widgets.clear();
}

// ========== 调整执行顺序：先更新相机位姿，再清空Tag动态控件 ==========
void VizBoardViewer::update3DView(const BoardResult& res)
{
    if (m_viz_win.wasStopped())
        return;

    // 分支1：无有效位姿，直接刷新窗口
    if (!res.board_pose_valid)
    {
        clearDynamicTagWidgets();
        m_viz_win.spinOnce(SPIN_DELAY_MS, true);
        return;
    }

    // 第一步：更新相机静态控件位姿（控件永久存在，不会崩溃）
    cv::Affine3d cam_pose(res.camera_rvec, res.camera_tvec);
    m_viz_win.setWidgetPose("camera_cube", cam_pose);
    m_viz_win.setWidgetPose("camera_axis", cam_pose);
    m_viz_win.setWidgetPose("cam_frust", cam_pose);

    // 第二步：清空上一帧Tag相关动态控件（仅删Tag，不碰相机）
    clearDynamicTagWidgets();

    // 全局重投影误差文字
    char buf[BUF_MAX] = { 0 };
    snprintf(buf, sizeof(buf), "Avg Err:%.2f px", res.avg_error);
    cv::viz::WText3D global_text(
        buf,
        cv::Point3d(0, 0, m_tag_size * GLOBAL_ERR_TEXT_H),
        m_tag_size * GLOBAL_ERR_TEXT_SZ,
        true,
        cv::viz::Color::green()
    );
    global_text.setRenderingProperty(cv::viz::LINE_WIDTH, 2);
    m_viz_win.showWidget("global_err", global_text);
    m_dynamic_tag_widgets.push_back("global_err");

    // 遍历所有Tag绘制标记与误差
    for (const auto& tag : res.tag_results)
    {
        if (!tag.detect_ok || tag.predicts.size() != 4)
            continue;
        std::string vid = std::to_string(tag.value_id);
        // Tag 轮廓线
        std::string box_name = "value_box_" + vid;
        std::vector<cv::Point3d> linePts(tag.predicts);
        linePts.emplace_back(linePts.front());
        cv::viz::WPolyLine line(linePts, cv::viz::Color::red());
        line.setRenderingProperty(cv::viz::LINE_WIDTH, 1);
        m_viz_win.showWidget(box_name, line);
        m_dynamic_tag_widgets.push_back(box_name);
        // 四角小球
        for (int i = 0; i < 4; ++i)
        {
            std::string sp_name = "val_p" + std::to_string(i) + "_" + vid;
            cv::viz::WSphere sp(tag.predicts[i], m_tag_size * TAG_SPHERE_RATIO, TAG_SPHERE_RES, cv::viz::Color::green());
            m_viz_win.showWidget(sp_name, sp);
            m_dynamic_tag_widgets.push_back(sp_name);
        }
        // Tag ID 文字
        std::string id_name = "val_id_" + vid;
        cv::viz::WText3D id_text(vid, tag.predicts[0], m_tag_size * TAG_ID_SZ, true, cv::viz::Color::red());
        m_viz_win.showWidget(id_name, id_text);
        m_dynamic_tag_widgets.push_back(id_name);
        // 单个Tag误差文字
        cv::Point3d center(
            (tag.predicts[0].x + tag.predicts[2].x) / 2.0,
            (tag.predicts[0].y + tag.predicts[2].y) / 2.0,
            m_tag_size * TAG_ERR_H
        );
        
        snprintf(buf, sizeof(buf), "Repro Err:%.5f", tag.single_err);
        std::string err_name = "val_err_" + vid;
        cv::viz::WText3D err_text(buf, center, m_tag_size * TAG_ERR_SZ, true, cv::viz::Color::orange());
        m_viz_win.showWidget(err_name, err_text);
        m_dynamic_tag_widgets.push_back(err_name);
    }

    m_viz_win.spinOnce(SPIN_DELAY_MS, true);
}