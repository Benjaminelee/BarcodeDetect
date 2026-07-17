#ifndef VIZ_BOARD_VIEWER_H
#define VIZ_BOARD_VIEWER_H
#include <opencv2/opencv.hpp>
#include <opencv2/viz/viz3d.hpp>
#include <opencv2/viz/widgets.hpp>
#include <opencv2/viz.hpp>
#include <vector>
#include <string>
#include "../include/TypeDef.h"

class VizBoardViewer
{
public:
    VizBoardViewer(double tag_size, const cv::Mat& K, const cv::Size& img_size, int rows = 4, int cols = 5, bool mltiArray = true);
    ~VizBoardViewer();
    void setMultiArrayType(bool multiArray);
    void update3DView(const BoardResult& res);
    void closeWindow();
    bool isWindowClosed() const;

private:
    void initStaticScene(const cv::Mat& K, const cv::Size& img_size);
    void clearDynamicTagWidgets();

private:
    double m_tag_size;
    cv::viz::Viz3d m_viz_win;
    std::vector<std::string> m_dynamic_tag_widgets;
    std::vector<std::string> m_static_widgets;
    bool m_multi_array;
    cv::Mat m_K;
    cv::Size m_img_size;
    int m_rows;
    int m_cols;
};

#endif // VIZ_BOARD_VIEWER_H