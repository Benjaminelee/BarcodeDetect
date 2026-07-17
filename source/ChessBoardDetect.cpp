#include "../include/ChessBoardDetect.h"

ChessBoardDetect::ChessBoardDetect(cv::Size boardSize, double squareSize)
    : TagJointDetect(squareSize, boardSize.height, boardSize.width)
    , m_chess_pattern(boardSize)
    , m_square_len(squareSize)
    , m_subpix_win(cv::Size(15, 15))
{
    // 默认亚像素收敛条件
    m_criteria = cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4);
    setJointEstimate(true);
}

void ChessBoardDetect::setChessParam(cv::Size subPixWin, int maxIter, double eps)
{
    m_subpix_win = subPixWin;
    m_criteria = cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, maxIter, eps);
}

BoardResult ChessBoardDetect::detect(const cv::Mat& img)
{
    BoardResult board_res;
    std::vector<TagResult> tag_results;
    tag_results.clear();

    cv::Mat gray;
    if (img.channels() == 3)
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else
        gray = img.clone();
    // 1. 检测棋盘内角点
    std::vector<cv::Point2f> corners;
    bool found = cv::findChessboardCorners(
        gray,
        m_chess_pattern,
        corners,
        /*cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_NORMALIZE_IMAGE + cv::CALIB_CB_FILTER_QUADS*/
        cv::CALIB_CB_FAST_CHECK
    );

    if (!found)
    {
        board_res.tag_results = tag_results;
        computeImageToCameraJointPose(board_res);
        return board_res;
    }

    // 2. 亚像素精修角点
    cv::cornerSubPix(gray, corners, m_subpix_win, cv::Size(-1, -1), m_criteria);

    // 3. 适配现有TagResult结构：把整块棋盘当作多个虚拟Tag
    for (size_t i = 0; i < corners.size(); ++i) {
        TagResult res{};
        res.detect_ok = true;
        res.tag_id = i;
        res.value_id = i;
        res.corners.clear();
        res.corners.emplace_back(corners[i]);
        tag_results.emplace_back(res);
    }
    board_res.tag_results = tag_results;

    // 4. 调用基类统一逻辑：构建3D/2D点、solvePnP求棋盘相对相机位姿、重投影误差
    computeImageToCameraJointPose(board_res);
    return board_res;
}