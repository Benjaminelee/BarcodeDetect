#include "../include/CalibCam.h"
#include <filesystem>
#include <iostream>

using namespace cv;
namespace fs = std::filesystem;

bool runCameraCalibration(const std::string& imgSaveDir, const std::string& saveXmlPath, int capIdx, int width, int height)
{
    // SetConsoleOutputCP(65001);
    if (!fs::exists(imgSaveDir))
    {
        fs::create_directories(imgSaveDir);
        std::cout << "创建图片保存目录：" << imgSaveDir << std::endl;
    }
    // 打开相机
    VideoCapture cap(capIdx, CAP_DSHOW);
    cap.set(CAP_PROP_FRAME_WIDTH, width);
    cap.set(CAP_PROP_FRAME_HEIGHT, height);
    if (!cap.isOpened())
    {
        std::cout << "【标定失败】无法打开相机 idx:" << capIdx << std::endl;
        return false;
    }

    std::vector<std::vector<Point3f>> objPoints; // 世界坐标
    std::vector<std::vector<Point2f>> imgPoints; // 图像像素坐标
    std::vector<Point3f> objSingle;

    // 预生成单张棋盘世界坐标
    for (int y = 0; y < BOARD_SIZE.height; y++)
        for (int x = 0; x < BOARD_SIZE.width; x++)
            objSingle.emplace_back(x * SQUARE_LEN_MM, y * SQUARE_LEN_MM, 0.f);

    Mat frame, gray;
    int saveCnt = 0;
    std::cout << "\n=====标定采集开始=====\n空格保存有效样本 | q退出采集并计算标定" << std::endl;

    // 采集循环
    while (cap.read(frame))
    {
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        std::vector<Point2f> corners;
        bool findFlag = findChessboardCorners(gray, BOARD_SIZE, corners,
            CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_NORMALIZE_IMAGE + CALIB_CB_FAST_CHECK + CALIB_CB_FILTER_QUADS);
        std::cout << "findChessboardCorners结果值为：" << findFlag << std::endl;
        if (findFlag)
        {
            // 亚像素角点优化
            cornerSubPix(gray, corners, Size(11,11), Size(-1,-1),
                TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 1e-4));
            drawChessboardCorners(frame, BOARD_SIZE, corners, findFlag);
        }

        imshow("Collect Chessboard(12x9)", frame);
        setWindowProperty("Collect Chessboard(12x9)", WND_PROP_TOPMOST, 1);
        char key = static_cast<char>(waitKey(1) & 0xFF);

        // 空格保存样本
        if (key == ' ' && findFlag)
        {
            objPoints.push_back(objSingle);
            imgPoints.push_back(corners);
            std::string imgPath = imgSaveDir + "/img_" + std::to_string(saveCnt) + ".jpg";
            imwrite(imgPath, frame);
            saveCnt++;
            std::cout << "已保存样本数：" << saveCnt << std::endl;
        }
        // q / ESC(27)双按键退出, 结束采集
        if (key == 'q' || key == 27) break;
    }
    cap.release();
    destroyAllWindows();

    // 样本数量校验
    const int MIN_SAMPLE = 15;
    if ((int)objPoints.size() < MIN_SAMPLE)
    {
        std::cout << "【标定失败】有效样本不足" << MIN_SAMPLE << "张，当前：" << objPoints.size() << std::endl;
        return false;
    }

    // 执行相机标定求解参数
    Mat cameraMat, distCoeff;
    std::vector<Mat> rvecs, tvecs;
    double reproErr = calibrateCamera(objPoints, imgPoints, gray.size(), cameraMat, distCoeff, rvecs, tvecs);

    // 写入xml
    FileStorage fs(saveXmlPath, FileStorage::WRITE);
    fs << "cameraMatrix"    << cameraMat;
    fs << "distCoeffs"     << distCoeff;
    fs << "imageSize"      << gray.size();
    fs << "reproError"     << reproErr;
    fs.release();

    std::cout << "\n=====标定完成=====\n平均重投影误差：" << reproErr << "\n参数保存路径：" << saveXmlPath << std::endl;
    std::cout << "参考：<0.5优秀 0.5~1可用 >1.0建议重采" << std::endl;
    return true;
}

bool runCameraCalibration_FromImages(const std::string& imgDir, const std::string& saveXmlPath)
{
    // SetConsoleOutputCP(65001);
    if (!fs::exists(imgDir))
    {
        std::cout << "文件夹不存在：" << imgDir << std::endl;
        return false;
    }

    std::vector<std::vector<Point3f>> objPoints;
    std::vector<std::vector<Point2f>> imgPoints;
    std::vector<Point3f> objSingle;

    // 棋盘世界坐标
    for (int y = 0; y < BOARD_SIZE.height; y++)
        for (int x = 0; x < BOARD_SIZE.width; x++)
            objSingle.emplace_back(x * SQUARE_LEN_MM, y * SQUARE_LEN_MM, 0.f);

    cv::Size imgSize;
    int validCnt = 0;

    // 遍历目录所有jpg
    for (auto& entry : fs::directory_iterator(imgDir))
    {
        std::string path = entry.path().string();
        std::string suf = entry.path().extension().string();
        if (suf != ".jpg" && suf != ".png") continue;

        cv::Mat gray = cv::imread(path, cv::IMREAD_GRAYSCALE);
        if (gray.empty()) continue;
        imgSize = gray.size();

        std::vector<cv::Point2f> corners;
        bool findFlag = cv::findChessboardCorners(gray, BOARD_SIZE, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_NORMALIZE_IMAGE + cv::CALIB_CB_FAST_CHECK);

        if (findFlag)
        {
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));
            objPoints.push_back(objSingle);
            imgPoints.push_back(corners);
            validCnt++;
            std::cout << "有效:" << validCnt << " | " << entry.path().filename() << std::endl;
        }
    }

    const int MIN_SAMPLE = 15;
    if ((int)objPoints.size() < MIN_SAMPLE)
    {
        std::cout << "有效图片不足" << MIN_SAMPLE << "张" << std::endl;
        return false;
    }

    cv::Mat cameraMat, distCoeff;
    std::vector<cv::Mat> rvecs, tvecs;
    double err = cv::calibrateCamera(objPoints, imgPoints, imgSize, cameraMat, distCoeff, rvecs, tvecs);

    cv::FileStorage fs(saveXmlPath, cv::FileStorage::WRITE);
    fs << "cameraMatrix" << cameraMat << "distCoeffs" << distCoeff << "imageSize" << imgSize << "reproError" << err;
    fs.release();

    std::cout << "\n针孔离线标定完成，误差:" << err << "\n输出:" << saveXmlPath << std::endl;
    return true;
}