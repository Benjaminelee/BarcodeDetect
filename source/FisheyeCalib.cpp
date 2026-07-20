#include "../include/FisheyeCalib.h"
#include "../include/HikCamera.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace cv;

bool runFisheyeCalib(const std::string& imgDir,
                     const std::string& saveXml,
                     int capId,
                     int width,
                     int height)
{
    // SetConsoleOutputCP(65001);
    if (!fs::exists(imgDir)) {
        fs::create_directories(imgDir);
    }
    VideoCapture cap(capId, CAP_DSHOW);
    cap.set(CAP_PROP_FRAME_WIDTH, width);
    cap.set(CAP_PROP_FRAME_HEIGHT, height);
    if(!cap.isOpened())
    {
        std::cout<<"鱼眼标定：打开相机失败\n";
        return false;
    }

    std::vector<std::vector<Point3f>> objPts;
    std::vector<std::vector<Point2f>> imgPts;
    std::vector<Point3f> objSingle;

    for(int y=0;y<FISH_BOARD_SIZE.height;y++)
        for(int x=0;x<FISH_BOARD_SIZE.width;x++)
            objSingle.emplace_back(x*FISH_SQUARE_MM, y*FISH_SQUARE_MM, 0.f);

    Mat frame,gray;
    int cnt=0;
    std::cout<<"【鱼眼标定】空格存图｜q/ESC退出\n";
    namedWindow("FishCollect", WINDOW_NORMAL);
    setWindowProperty("FishCollect",WND_PROP_TOPMOST,1);

    while(cap.read(frame))
    {
        cvtColor(frame,gray,COLOR_BGR2GRAY);
        std::vector<Point2f> corners;
        bool ok = findChessboardCorners(gray,FISH_BOARD_SIZE,corners,
            CALIB_CB_ADAPTIVE_THRESH+CALIB_CB_NORMALIZE_IMAGE+CALIB_CB_FILTER_QUADS);

        if(ok)
        {
            cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
                TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 1e-4));
            drawChessboardCorners(frame, FISH_BOARD_SIZE, corners, ok);
        }

        imshow("FishCollect",frame);
        int key = waitKey(1);
        if(key == ' ' && ok)
        {
            objPts.push_back(objSingle);
            imgPts.push_back(corners);
            imwrite(imgDir+"/fish_"+std::to_string(cnt)+".jpg",frame);
            cnt++;
            std::cout<<"已采集:"<<cnt<<"\n";
        }
        if(key=='q'||key==27) break;
    }
    cap.release();cv::destroyAllWindows();

    const int MIN_NUM=15;
    if((int)objPts.size()<MIN_NUM)
    {
        std::cout<<"样本不足15张，标定失败\n";
        return false;
    }

    // ========== OpenCV鱼眼专用标定API ==========
    Mat K,D;
    std::vector<Mat> rvec,tvec;
    int flags = fisheye::CALIB_RECOMPUTE_EXTRINSIC
              | fisheye::CALIB_CHECK_COND
              | fisheye::CALIB_FIX_SKEW;

    double err = fisheye::calibrate(objPts, imgPts, gray.size(), K, D, rvec, tvec, flags,
        TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 1e-6));

    // 保存参数
    FileStorage fs(saveXml, FileStorage::WRITE);
    fs<<"K"<<K<<"D"<<D<<"imgSize"<<gray.size()<<"reproError"<<err;
    fs.release();

    std::cout<<"鱼眼标定完成，重投影误差:"<<err<<"\n";
    return true;
}

bool runHiKFisheyeCalib(const std::string& imgSaveDir,
                        const std::string& saveXml,
                        int capId,
                        int width,
                        int height)
{
    if (!fs::exists(imgSaveDir))
        fs::create_directories(imgSaveDir);
    
    std::vector<std::vector<cv::Point3f>> objPoints;
    std::vector<std::vector<Point2f>> imgPoints;
    std::vector<cv::Point3f> objSingle;
    // 生成世界坐标
    for (int y = 0; y < FISH_BOARD_SIZE.height; y++) {
        for (int x = 0; x < FISH_BOARD_SIZE.width; x++) {
            objSingle.emplace_back(x * FISH_SQUARE_MM, y * FISH_SQUARE_MM, 0.f);
        }
    }
    // 函数生命周期内全局SDK自动管理
    SdkLifeGuard sdkGuard;
    if (!sdkGuard.IsSdkReady())
    {
        std::cout << "SDK初始化失败，直接退出" << std::endl;
        return false;
    }
    HikCamera camera;
    try {
        if (!camera.OpenUsbCamera()) {
            std::cout << "[标定] 海康相机打开失败，退出采集" << std::endl;
            return false;
        }
        int saveCount = 0;
        int emptyFrameCnt = 0;
        Mat frame, bgr;
        std::cout << "【鱼眼标定】s存图｜q/ESC退出\n";
        while (emptyFrameCnt < 10) {
            frame = camera.GetFrame(30);
            if (frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                ++emptyFrameCnt;
                continue;
            }
            // 检测角点
            std::vector<cv::Point2f> corners;
            if (frame.channels() != 1) {
                std::cout << "HiK相机读取的不是灰度图像" << std::endl;
                break;
            }
            bool found = findChessboardCorners(frame, FISH_BOARD_SIZE, corners,
                CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_NORMALIZE_IMAGE + CALIB_CB_FAST_CHECK);
            cv::imwrite("HiK_test.jpg", frame);
            if (found) {
                // 亚像素优化角点
                cornerSubPix(frame, corners, cv::Size(15, 15), Size(-1, -1),
                    TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 1e-4));
                cvtColor(frame, bgr, COLOR_GRAY2BGR);
                //drawChessboardCorners(bgr, FISH_BOARD_SIZE, corners, found);
                
                cv::Mat showFrame;
                cv::resize(frame, showFrame, cv::Size(), 1.0 / 8.0, 1.0 / 8.0, cv::INTER_LINEAR);
                cv::imshow("current_frame", showFrame);
                cv::waitKey(-1);
            }
            
            std::cout << "请输入键决定是否存图还是退出" << std::endl;
            int key = 0;
            while (true) {
                key = getchar();
                // 过滤回车、换行、空格等无效字符，避免连续多次触发
                if (key == '\n' || key == '\r' || key == ' ')
                    continue;
                break;
            }
            // 保存有效图片
            if ((key == 's' || key == 'S') && found) {
                objPoints.push_back(objSingle);
                imgPoints.push_back(corners);
                std::string savePath = imgSaveDir + "/img_" + std::to_string(saveCount + 4) + ".png";
                imwrite(savePath, bgr);
                saveCount++;
                std::cout << "已保存第" << saveCount + 4 << "张标定图" << std::endl;
            }
            // ESC退出采集
            if (key == 'q' || key == 27) break;
        }
        std::cout << "采集结束，累计空帧：" << emptyFrameCnt << std::endl;
    }
    catch (...)
    {
        std::cout << "采集过程发生异常，释放资源" << std::endl;
        camera.CloseCamera();
        return false;
    }
    // 正常退出，先释放相机，再释放全局SDK
    camera.CloseCamera();
    if (objPoints.size() < 15)
    {
        std::cout << "有效标定图片不足15张，标定失败" << std::endl;
        return false;
    }

    cv::Mat K, D;
    cv::Size sz(width, height);
    std::vector<cv::Mat> rvecs, tvecs;
    int calibFlags = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC
        | cv::fisheye::CALIB_CHECK_COND
        | cv::fisheye::CALIB_FIX_SKEW;

    double reprojErr = cv::fisheye::calibrate(
        objPoints,
        imgPoints,
        sz,
        K, D,
        rvecs, tvecs,
        calibFlags,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6)
    );

    // 保存标定参数
    cv::FileStorage fs(saveXml, cv::FileStorage::WRITE);
    fs << "cameraMatrix" << K;
    fs << "distCoeffs" << D;
    fs << "imageSize" << sz;
    fs << "reprojectionError" << reprojErr;
    fs.release();

    std::cout << "==================== 标定完成 ====================" << std::endl;
    std::cout << "重投影误差：" << reprojErr << " 像素" << std::endl;
    std::cout << "标定参数已保存至：" << saveXml << std::endl;
    return true;
}

bool runFisheyeCalib_FromImages(const std::string& imgDir, const std::string& saveXml)
{
    // SetConsoleOutputCP(65001);
    namespace fs = std::filesystem;
    if (!fs::exists(imgDir)) { std::cout << "目录不存在\n"; return false; }

    std::vector<std::vector<cv::Point3f>> objPts;
    std::vector<std::vector<cv::Point2f>> imgPts;
    std::vector<cv::Point3f> objSingle;

    // 12列 × 9行内角点
    for (int y = 0; y < FISH_BOARD_SIZE.height; y++)
        for (int x = 0; x < FISH_BOARD_SIZE.width; x++)
            objSingle.emplace_back(x * FISH_SQUARE_MM, y * FISH_SQUARE_MM, 0.f);

    cv::Size sz;
    int valid = 0;
    for (auto& e : fs::directory_iterator(imgDir))
    {
        auto ext = e.path().extension().string();
        if (ext != ".jpg" && ext != ".png" && ext != ".bmp") continue;
        cv::Mat gray = cv::imread(e.path().string(), cv::IMREAD_GRAYSCALE);
        if (gray.empty())continue;
        sz = gray.size();

        // =====鱼眼专用预处理：解决畸变造成识别失败=====
        cv::blur(gray, gray, cv::Size(3, 3));          // 平滑降噪
        //cv::equalizeHist(gray, gray);                  // 均衡对比度，暗光/过曝补救

        std::vector<cv::Point2f> cor;
        // 完整四组合法参数
        int chessFlag = CALIB_CB_ADAPTIVE_THRESH
            + CALIB_CB_NORMALIZE_IMAGE
            + CALIB_CB_FAST_CHECK
            + CALIB_CB_FILTER_QUADS;

        bool ok = findChessboardCorners(gray, FISH_BOARD_SIZE, cor, chessFlag);
        if (ok)
        {
            cornerSubPix(gray, cor, cv::Size(15, 15), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));
            objPts.push_back(objSingle);
            imgPts.push_back(cor);
            valid++;
            std::cout << "鱼眼有效:" << valid << " " << e.path().filename() << "\n";
        }
        else
        {
            // 调试：识别失败输出文件名
            std::cout << "识别失败：" << e.path().filename() << std::endl;
        }
    }
    if (objPts.size() < 15) { std::cout << "样本不足\n"; return false; }

    cv::Mat K, D;
    std::vector<cv::Mat> r, t;
    int flag = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC
        | cv::fisheye::CALIB_CHECK_COND
        | cv::fisheye::CALIB_FIX_SKEW;
    double err = cv::fisheye::calibrate(objPts, imgPts, sz, K, D, r, t, flag,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6));

    cv::FileStorage write(saveXml, cv::FileStorage::WRITE);
    write << "K" << K << "D" << D << "imgSize" << sz << "reproError" << err;
    write.release();
    std::cout << "鱼眼离线标定完成，误差:" << err << "\n";
    return true;
}

