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

//双目相机联合标定
bool runStereoFisheyeCalib(const std::string& imgDir,
    const std::string& saveXml,
    int capId,
    int totalWidth,
    int totalHeight)
{
    // 创建存储目录
    if (!fs::exists(imgDir)) {
        fs::create_directories(imgDir);
    }
    cv::VideoCapture cap(capId, cv::CAP_DSHOW);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, totalWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, totalHeight);
    if (!cap.isOpened())
    {
        std::cout << "双目鱼眼标定：打开相机失败\n";
        return false;
    }
    // ----------------------棋盘世界坐标点（所有图像对共用）----------------------
    std::vector<cv::Point3f> objSingle;
    for (int y = 0; y < FISH_BOARD_SIZE.height; y++)
        for (int x = 0; x < FISH_BOARD_SIZE.width; x++)
            objSingle.emplace_back(x * FISH_SQUARE_MM, y * FISH_SQUARE_MM, 0.f);
    // 标定数据容器：成对存储
    std::vector<std::vector<cv::Point3f>> objPtsAll;      // 世界坐标(左右图共用一套)
    std::vector<std::vector<cv::Point2f>> imgPtsLeft;    // 左图像素角点
    std::vector<std::vector<cv::Point2f>> imgPtsRight;    // 右图像素角点
    cv::Mat frame;
    int collectCnt = 0;
    const int halfW = totalWidth / 2;    // 分割边界：大图一半宽度
    std::cout << "【双目鱼眼标定采集】空格保存图像对｜q/ESC退出\n";
    cv::namedWindow("Preview-Left", cv::WINDOW_NORMAL);
    cv::namedWindow("Preview-Right", cv::WINDOW_NORMAL);
    cv::setWindowProperty("Preview-Left", cv::WND_PROP_TOPMOST, 1);
    cv::setWindowProperty("Preview-Right", cv::WND_PROP_TOPMOST, 1);

    // ========================实时采集循环========================
    while (cap.read(frame))
    {
        // 1. 分割拼接大图 → 【原始无标记原图】和【预览画布】分离
        cv::Mat leftRaw = frame(cv::Rect(0, 0, halfW, totalHeight)).clone();
        cv::Mat rightRaw = frame(cv::Rect(halfW, 0, halfW, totalHeight)).clone();

        // 预览画布，后续可以画上棋盘角点，不会污染原始图像
        cv::Mat leftImg = leftRaw.clone();
        cv::Mat rightImg = rightRaw.clone();

        cv::Mat grayL, grayR;
        cv::cvtColor(leftImg, grayL, cv::COLOR_BGR2GRAY);
        cv::cvtColor(rightImg, grayR, cv::COLOR_BGR2GRAY);
        std::vector<cv::Point2f> cornersL, cornersR;

        // 棋盘检测flag 和离线标定代码保持一致，补齐 CALIB_CB_FAST_CHECK
        int chessFlag = cv::CALIB_CB_ADAPTIVE_THRESH
            + cv::CALIB_CB_NORMALIZE_IMAGE
            + cv::CALIB_CB_FAST_CHECK
            + cv::CALIB_CB_FILTER_QUADS;

        // 2. 分别检测左右棋盘角点
        bool okL = cv::findChessboardCorners(grayL, FISH_BOARD_SIZE, cornersL, chessFlag);
        bool okR = cv::findChessboardCorners(grayR, FISH_BOARD_SIZE, cornersR, chessFlag);

        // 亚像素优化角点
        if (okL)
        {
            cv::cornerSubPix(grayL, cornersL, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));
            // 仅绘制到预览画布，原始leftRaw完全不受影响
            cv::drawChessboardCorners(leftImg, FISH_BOARD_SIZE, cornersL, okL);
        }
        if (okR)
        {
            cv::cornerSubPix(grayR, cornersR, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));
            cv::drawChessboardCorners(rightImg, FISH_BOARD_SIZE, cornersR, okR);
        }
        // 预览画面（显示带棋盘标记的画面）
        cv::imshow("Preview-Left", leftImg);
        cv::imshow("Preview-Right", rightImg);

        int key = cv::waitKey(1);
        // =========采集保存条件：必须左右两张图同时检出棋盘！=========
        if (key == ' ' && okL && okR)
        {
            objPtsAll.push_back(objSingle);
            imgPtsLeft.push_back(cornersL);
            imgPtsRight.push_back(cornersR);

            // ⭐关键改动：保存【原始无角点图像】leftRaw / rightRaw，而不是预览画布leftImg
            cv::imwrite(imgDir + "/left_" + std::to_string(collectCnt) + ".jpg", leftRaw);
            cv::imwrite(imgDir + "/right_" + std::to_string(collectCnt) + ".jpg", rightRaw);

            collectCnt++;
            std::cout << "成功采集图像对: " << collectCnt << "\n";
        }
        if (key == 'q' || key == 27) break;
    }
    cap.release();
    cv::destroyAllWindows();
    // ========================样本数量校验========================
    const int MIN_SAMPLE = 15;
    if ((int)objPtsAll.size() < MIN_SAMPLE)
    {
        std::cout << "图像对样本不足15组，标定终止！\n";
        return false;
    }
    cv::Size singleImgSize(halfW, totalHeight); //单目图像分辨率
    // ========================【第一步】左相机单目标定(鱼眼)========================
    cv::Mat K1, D1;
    std::vector<cv::Mat> rvecsL, tvecsL;
    int flagFish = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC
        | cv::fisheye::CALIB_CHECK_COND
        | cv::fisheye::CALIB_FIX_SKEW;
    double errLeft = cv::fisheye::calibrate(
        objPtsAll, imgPtsLeft, singleImgSize,
        K1, D1, rvecsL, tvecsL,
        flagFish,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6)
    );
    std::cout << "左相机单目标定完成，重投影误差:" << errLeft << "\n";
    // ========================【第二步】右相机单目标定(鱼眼)========================
    cv::Mat K2, D2;
    std::vector<cv::Mat> rvecsR, tvecsR;
    double errRight = cv::fisheye::calibrate(
        objPtsAll, imgPtsRight, singleImgSize,
        K2, D2, rvecsR, tvecsR,
        flagFish,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6)
    );
    std::cout << "右相机单目标定完成，重投影误差:" << errRight << "\n";
    // ========================【第三步】鱼眼双目联合标定（求R、T）========================
    cv::Mat R, T;
    std::vector<cv::Vec3d> E, F;
    // 关键标志位：CALIB_FIX_INTRINSIC → 固定已经标定好的K1 D1 K2 D2，只求解双目外参R T
    int stereoFlag = cv::fisheye::CALIB_FIX_INTRINSIC;
    double stereoErr = cv::fisheye::stereoCalibrate(
        objPtsAll, imgPtsLeft, imgPtsRight,
        K1, D1, K2, D2,
        singleImgSize,
        R, T, E, F,
        stereoFlag,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 1e-6)
    );
    std::cout << "双目鱼眼联合标定完成！双目重投影误差:" << stereoErr << "\n";
    // ========================保存全部标定参数到XML文件========================
    cv::FileStorage write(saveXml, cv::FileStorage::WRITE);
    write << "imageSize" << singleImgSize;
    write << "K1" << K1 << "D1" << D1;
    write << "K2" << K2 << "D2" << D2;
    write << "R" << R << "T" << T;
    write << "errLeft" << errLeft;
    write << "errRight" << errRight;
    write << "stereoError" << stereoErr;
    write.release();
    std::cout << "所有标定参数已保存至：" << saveXml << "\n";
    return true;
}

//海康工业相机在线标定
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

        const double TARGET_GRAY = 128.0;
        const int MIN_EXP_US = 1000;
        const int MAX_EXP_US = 120000;
        double integralSum = 0.0;
        double prevPiError = 0.0;
        const int SKIP_FRAME_AFTER_EXP_CHANGE = 2;

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
                cv::waitKey(1);
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
                std::string savePath = imgSaveDir + "/img_" + std::to_string(saveCount) + ".png";
                imwrite(savePath, bgr);
                saveCount++;
                std::cout << "已保存第" << saveCount << "张标定图" << std::endl;
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
    std::vector<std::string> validFileNames;
    for (auto& e : fs::directory_iterator(imgDir))
    {
        auto ext = e.path().extension().string();
        if (ext != ".jpg" && ext != ".png" && ext != ".bmp") continue;
        cv::Mat gray = cv::imread(e.path().string(), cv::IMREAD_GRAYSCALE);
        //cv::resize(gray, gray, cv::Size(960, 540), 0, 0, cv::INTER_AREA);
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
            validFileNames.push_back(e.path().filename().string());
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

    std::vector<double> eachImgError;
    double totalSquareErr = 0.0;
    int totalPoints = 0;

    for (int i = 0; i < (int)objPts.size(); i++)
    {
        std::vector<cv::Point2f> projPoints;
        // 鱼眼专用投影函数，不要用普通 cv::projectPoints
        cv::fisheye::projectPoints(objPts[i], projPoints, r[i], t[i], K, D);

        double squareSum = 0.0;
        int pointNum = (int)imgPts[i].size();
        for (int p = 0; p < pointNum; p++)
        {
            double dx = imgPts[i][p].x - projPoints[p].x;
            double dy = imgPts[i][p].y - projPoints[p].y;
            squareSum += dx * dx + dy * dy;
        }
        // 当前图片 RMS 重投影误差
        double rms = std::sqrt(squareSum / pointNum);
        eachImgError.push_back(rms);

        totalSquareErr += squareSum;
        totalPoints += pointNum;

        std::cout << "图片[" << i << "] " << validFileNames[i]
            << " 单张RMS重投影误差 = " << rms << " 像素" << std::endl;
    }

    // 整体RMS，用来和calibrate返回值做对比
    double overallRms = std::sqrt(totalSquareErr / totalPoints);
    std::cout << "\ncalibrate接口返回整体误差：" << err << std::endl;
    std::cout << "手动计算整体RMS误差：" << overallRms << std::endl;

    cv::FileStorage write(saveXml, cv::FileStorage::WRITE);
    write << "K" << K << "D" << D << "imgSize" << sz << "reproError" << err;
    write << "singleImageRMS" << eachImgError;
    write.release();
    std::cout << "鱼眼离线标定完成，误差:" << err << "\n";
    return true;
}

//离线双目相机联合标定
bool runStereoFisheyeCalib_FromImages(const std::string& imgDir, const std::string& saveXml)
{
    if (!fs::exists(imgDir))
    {
        std::cout << "图像目录不存在!\n";
        return false;
    }

    // --------------------------1.加载成对图像--------------------------
    // key:图像序号; value: <左图路径,右图路径>
    std::map<int, std::pair<std::string, std::string>> pairMap;

    for (auto& entry : fs::directory_iterator(imgDir))
    {
        std::string fname = entry.path().filename().string();
        std::string ext = entry.path().extension().string();
        if (ext != ".jpg" && ext != ".png" && ext != ".bmp")
            continue;

        //匹配文件名 left_0.jpg / right_0.jpg
        if (fname.substr(0, 5) == "left_")
        {
            int idx = std::stoi(fname.substr(5));
            pairMap[idx].first = entry.path().string();
        }
        else if (fname.substr(0, 6) == "right_")
        {
            int idx = std::stoi(fname.substr(6));
            pairMap[idx].second = entry.path().string();
        }
    }

    //--------------------------2.遍历每一对图片，检测棋盘角点--------------------------
    std::vector<std::vector<cv::Point3f>> objPtsAll;
    std::vector<std::vector<cv::Point2f>> imgPtsLeft;
    std::vector<std::vector<cv::Point2f>> imgPtsRight;
    std::vector<std::string> validPairNames;

    // 世界坐标点，所有图片共用
    std::vector<cv::Point3f> objSingle;
    for (int y = 0; y < FISH_BOARD_SIZE.height; y++)
        for (int x = 0; x < FISH_BOARD_SIZE.width; x++)
            objSingle.emplace_back(x * FISH_SQUARE_MM, y * FISH_SQUARE_MM, 0.f);

    cv::Size singleImgSize;
    int validPairCnt = 0;

    int chessFlag = cv::CALIB_CB_ADAPTIVE_THRESH
        + cv::CALIB_CB_NORMALIZE_IMAGE
        + cv::CALIB_CB_FAST_CHECK
        + cv::CALIB_CB_FILTER_QUADS;

    for (auto& item : pairMap)
    {
        std::string leftPath = item.second.first;
        std::string rightPath = item.second.second;

        //跳过缺失左图或右图的残缺对
        if (leftPath.empty() || rightPath.empty())
        {
            std::cout << "序号 " << item.first << " 图像残缺，跳过\n";
            continue;
        }

        cv::Mat grayL = cv::imread(leftPath, cv::IMREAD_GRAYSCALE);
        cv::Mat grayR = cv::imread(rightPath, cv::IMREAD_GRAYSCALE);
        if (grayL.empty() || grayR.empty())
        {
            std::cout << "序号 " << item.first << " 图像读取失败\n";
            continue;
        }

        singleImgSize = grayL.size();

        // 【完全复用你原有鱼眼预处理】3×3高斯平滑降噪
        cv::blur(grayL, grayL, cv::Size(3, 3));
        cv::blur(grayR, grayR, cv::Size(3, 3));

        std::vector<cv::Point2f> corL, corR;
        bool okL = cv::findChessboardCorners(grayL, FISH_BOARD_SIZE, corL, chessFlag);
        bool okR = cv::findChessboardCorners(grayR, FISH_BOARD_SIZE, corR, chessFlag);

        //必须左右同时检出棋盘，该组才算有效
        if (okL && okR)
        {
            //亚像素优化角点
            cv::cornerSubPix(grayL, corL, cv::Size(15, 15), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));
            cv::cornerSubPix(grayR, corR, cv::Size(15, 15), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-4));

            objPtsAll.push_back(objSingle);
            imgPtsLeft.push_back(corL);
            imgPtsRight.push_back(corR);
            validPairNames.push_back("pair_" + std::to_string(item.first));
            validPairCnt++;
            std::cout << "✅有效图像对 " << validPairCnt << "  id:" << item.first << "\n";
        }
        else
        {
            std::cout << "❌序号:" << item.first << "棋盘检测失败, okL:" << okL << " okR:" << okR << "\n";
        }
    }

    //样本数量校验
    const int MIN_PAIR = 15;
    if ((int)objPtsAll.size() < MIN_PAIR)
    {
        std::cout << "有效成对图像不足15组，标定终止！当前有效：" << objPtsAll.size() << "\n";
        return false;
    }

    //=====================第一步：左相机鱼眼单目标定=====================
    cv::Mat K1, D1;
    std::vector<cv::Mat> rvecsL, tvecsL;
    int fishFlag = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC
        | cv::fisheye::CALIB_CHECK_COND
        | cv::fisheye::CALIB_FIX_SKEW;

    double errLeft = cv::fisheye::calibrate(
        objPtsAll, imgPtsLeft, singleImgSize,
        K1, D1, rvecsL, tvecsL,
        fishFlag,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6)
    );

    //----------计算左相机每张图片单目RMS重投影误差（沿用你的误差统计逻辑）----------
    std::vector<double> leftImgRMS;
    double leftTotalSqErr = 0.0;
    int leftTotalPt = 0;
    for (int i = 0; i < (int)objPtsAll.size(); i++)
    {
        std::vector<cv::Point2f> proj;
        cv::fisheye::projectPoints(objPtsAll[i], proj, rvecsL[i], tvecsL[i], K1, D1);
        double sqSum = 0.0;
        int n = (int)imgPtsLeft[i].size();
        for (int p = 0; p < n; p++)
        {
            double dx = imgPtsLeft[i][p].x - proj[p].x;
            double dy = imgPtsLeft[i][p].y - proj[p].y;
            sqSum += dx * dx + dy * dy;
        }
        double rms = std::sqrt(sqSum / n);
        leftImgRMS.push_back(rms);
        leftTotalSqErr += sqSum;
        leftTotalPt += n;
        std::cout << "左图[" << i << "] " << validPairNames[i] << " RMS误差=" << rms << " 像素\n";
    }
    double leftOverallRms = std::sqrt(leftTotalSqErr / leftTotalPt);
    std::cout << "====左相机标定完成,接口返回err=" << errLeft << " 手动计算总RMS=" << leftOverallRms << "\n";

    //=====================第二步：右相机鱼眼单目标定=====================
    cv::Mat K2, D2;
    std::vector<cv::Mat> rvecsR, tvecsR;
    double errRight = cv::fisheye::calibrate(
        objPtsAll, imgPtsRight, singleImgSize,
        K2, D2, rvecsR, tvecsR,
        fishFlag,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 1e-6)
    );

    //----------右相机单张RMS误差统计----------
    std::vector<double> rightImgRMS;
    double rightTotalSqErr = 0.0;
    int rightTotalPt = 0;
    for (int i = 0; i < (int)objPtsAll.size(); i++)
    {
        std::vector<cv::Point2f> proj;
        cv::fisheye::projectPoints(objPtsAll[i], proj, rvecsR[i], tvecsR[i], K2, D2);
        double sqSum = 0.0;
        int n = (int)imgPtsRight[i].size();
        for (int p = 0; p < n; p++)
        {
            double dx = imgPtsRight[i][p].x - proj[p].x;
            double dy = imgPtsRight[i][p].y - proj[p].y;
            sqSum += dx * dx + dy * dy;
        }
        double rms = std::sqrt(sqSum / n);
        rightImgRMS.push_back(rms);
        rightTotalSqErr += sqSum;
        rightTotalPt += n;
        std::cout << "右图[" << i << "] " << validPairNames[i] << " RMS误差=" << rms << " 像素\n";
    }
    double rightOverallRms = std::sqrt(rightTotalSqErr / rightTotalPt);
    std::cout << "====右相机标定完成,接口返回err=" << errRight << " 手动计算总RMS=" << rightOverallRms << "\n";

    //=====================第三步：鱼眼双目联合标定 固定内参求R、T=====================
    cv::Mat R, T;
    std::vector<cv::Vec3d> E, F;
    int stereoFlag = cv::fisheye::CALIB_FIX_INTRINSIC;
    double stereoErr = cv::fisheye::stereoCalibrate(
        objPtsAll, imgPtsLeft, imgPtsRight,
        K1, D1, K2, D2,
        singleImgSize,
        R, T, E, F,
        stereoFlag,
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 1e-6)
    );
    std::cout << "====双目联合标定完成！双目重投影误差 = " << stereoErr << "\n";

    //=====================保存全部标定结果XML=====================
    cv::FileStorage write(saveXml, cv::FileStorage::WRITE);
    write << "singleImageSize" << singleImgSize;
    write << "K1" << K1 << "D1" << D1;
    write << "K2" << K2 << "D2" << D2;
    write << "R" << R << "T" << T;
    write << "errLeft" << errLeft;
    write << "errRight" << errRight;
    write << "stereoError" << stereoErr;
    write << "leftSingleRMS" << leftImgRMS;
    write << "rightSingleRMS" << rightImgRMS;
    write.release();

    std::cout << "✅双目鱼眼离线标定全部完成，参数文件已保存:" << saveXml << "\n";
    return true;
}

