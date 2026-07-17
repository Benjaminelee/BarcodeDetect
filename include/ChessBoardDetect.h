#ifndef CHESS_BOARD_DETECT_H
#define CHESS_BOARD_DETECT_H

#include "TagJointDetect.h"
#include <opencv2/opencv.hpp>

// 棋盘格检测子类，继承通用阵列位姿基类
class ChessBoardDetect : public TagJointDetect
{
public:
    /**
     * @brief 棋盘格阵列检测器构造
     * @param boardW 棋盘内角点列数
     * @param boardH 棋盘内角点行数
     * @param squareSize 方格物理边长(m)
     */
    ChessBoardDetect(cv::Size boardSize, double squareSize = 0.01);

    // 设置棋盘检测参数（自适应阈值、亚像素窗口）
    void setChessParam(cv::Size subPixWin, int maxIter, double eps);

    ~ChessBoardDetect() override = default;

    // 实现基类纯虚检测接口
    BoardResult detect(const cv::Mat& img) override;

private:
    cv::Size m_chess_pattern;       // 棋盘内角点尺寸 (w,h)
    double m_square_len;            // 单格边长 m
    cv::Size m_subpix_win;          // 亚像素窗口
    cv::TermCriteria m_criteria;    // 亚像素收敛条件
};
#endif // CHESS_BOARD_DETECT_H