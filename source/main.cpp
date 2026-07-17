#include "../include/CalibCam.h"
#include "../include/FisheyeCalib.h"
#include "../include/UndistortCam.h"
#include "../include/TagJointDetect.h"
#include "../include/ArucoModJointDetect.h"
#include "../include/ArucoTagJointDetect.h"
#include "../include/AprilTagJointDetect.h"
#include "../include/ChessBoardDetect.h"
#include "../include/BarcodeGlobalApi.h"
#include "../include/VizBoardViewer.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>

using namespace std::chrono;

// true = USB UVC RGB相机；false=虚拟/平台/MIPI设备
static bool isUsbUvcCamera(int devId)
{
    std::string ueventPath = "/sys/class/video4linux/video" + std::to_string(devId) + "/device/uevent";
    std::ifstream fs(ueventPath);
    if (!fs.is_open()) return false;
    std::string line;
    while (std::getline(fs, line))
    {
        if (line.find("DRIVER=uvcvideo") != std::string::npos) return true;
    }
    return false;
}

static cv::VideoCapture openCamera(int devId, int width, int height)
{
#ifdef _WIN32
    cv::VideoCapture cap(devId, cv::CAP_DSHOW);
    if(cap.isOpened()){
        cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    }
    return cap;
#else
    // Linux/Docker 移除CAP_DSHOW，使用V4L2默认后端
    // cv::VideoCapture cap("/dev/video12");
    for(int dev=0;dev<=32;dev++){
        cv::VideoCapture cap;
        if(!isUsbUvcCamera(dev)) continue;
        if(cap.open(dev,cv::CAP_V4L2) && cap.get(cv::CAP_PROP_FRAME_WIDTH)>0){
            cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
            cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
            std::cout << "成功打开设备 /dev/video" << dev << std::endl;
            return cap;
        }
        cap.release();
    }
    std::cerr << "所有摄像头设备打开失败" << std::endl;
    return cv::VideoCapture();
#endif
}

static aruco::Dictionary::DICT_TYPES StringToArucoDictType(const std::string& str)
{
    aruco::Dictionary::DICT_TYPES arucoType = aruco::Dictionary::DICT_TYPES::ARUCO_MIP_36h12;
    if (aruco::Dictionary::isPredefinedDictinaryString(str)) {
        arucoType = aruco::Dictionary::getTypeFromString(str);
    }
    return arucoType;
}

int static runUndistortDemo(const std::string& visionDir = "RGBVison") {

    std::string imgDir = "../" + visionDir + "/fisheye_img";
    std::string saveXml = "../" + visionDir + "/fisheye_calib.xml";
    // 1.如需重新标定，取消下一行注释
    //runCameraCalibration(imgDir, saveXml, 0, 640, 480);
    // 2.初始化校正器
    //CameraUndistorter undist;
    //鱼眼相机标定
    //runFisheyeCalib(imgDir, saveXml);
    //runHiKFisheyeCalib(imgDir, saveXml);
    runFisheyeCalib_FromImages(imgDir, saveXml);

    FisheyeUndist undist;
    if (!undist.init(saveXml))
    {
        return -1;
    }
    cv::Size sz = undist.getImgSize();

    cv::VideoCapture cap = openCamera(0, sz.width, sz.height);
    if (!cap.isOpened())
    {
        std::cout << "相机打开失败，尝试更换设备号1/2" << std::endl;
        return -1;
    }
    
#ifndef RUN_IN_DOCKER
    // 提前创建窗口，置顶只执行1次
    cv::namedWindow("origin image", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("correct image", cv::WINDOW_AUTOSIZE);
    cv::setWindowProperty("origin image", cv::WND_PROP_TOPMOST, 1);
    cv::setWindowProperty("correct image", cv::WND_PROP_TOPMOST, 1);
#endif

    cv::Mat frame, frameFix;
    while (true)
    {
        bool ret = cap.read(frame);
        if (!ret) break;

        // 实时去畸变
        frameFix = undist.undistortImage(frame);
#ifndef RUN_IN_DOCKER
        cv::imshow("origin image", frame);
        cv::imshow("correct image", frameFix);
#endif
        // waitKey(1)：1ms阻塞，保障窗口刷新+按键响应，实现实时刷新
        int key = cv::waitKey(1);
        if (key >= 0)
        {
            char ch = static_cast<char>(key & 0xFF);
            if (ch == 'q' || ch == 27) //27=ESC
                break;
        }
    }
    cap.release();
#ifndef RUN_IN_DOCKER
    cv::destroyAllWindows();
#endif
    return 0;
}

//录制tag检测视频
bool static recordVideoFromCamera(const std::string& outVideoPath,int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v'))
{
    // 先获取标定分辨率（保证录制分辨率和标定一致）
    FisheyeUndist undist;
    const std::string calibPath = "../fisheye_calib.xml";
    if (!undist.init(calibPath))
    {
        std::cout << "标定文件加载失败" << std::endl;
        return false;
    }
    cv::Size targetSize = undist.getImgSize();
    
    cv::VideoCapture cap = openCamera(0, targetSize.width, targetSize.height);
    if (!cap.isOpened())
    {
        std::cout << "相机打开失败，无法录制视频" << std::endl;
        return false;
    }
#ifndef RUN_IN_DOCKER    
    cv::namedWindow("Camera Preview", cv::WINDOW_AUTOSIZE);
    cv::setWindowProperty("Camera Preview", cv::WND_PROP_TOPMOST, 1);
#endif
    cv::VideoWriter writer;
    bool isRecording = false;
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 60.0;

    std::cout << "===== 视频录制 =====" << std::endl;
    std::cout << "操作：b 开始录制 | q/ESC 停止并退出" << std::endl;
    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame))
        {
            std::cout << "相机读帧中断" << std::endl;
            break;
        }
        // 绘制状态文字
        cv::Mat show = frame.clone();
        if (isRecording)
        {
            cv::putText(show, "RECORDING...", cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        }
        else
        {
            cv::putText(show, "Press 'b' to record", cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
        }
#ifndef RUN_IN_DOCKER
        cv::imshow("Camera Preview", show);
#endif
        // 录制中写入帧
        if (isRecording && writer.isOpened())
        {
            writer.write(frame);
        }
        // 按键响应
        int key = cv::waitKey(1);
        if (key == 'b' || key == 'B')
        {
            if (!isRecording)
            {
                if (writer.open(outVideoPath, fourcc, fps, targetSize))
                {
                    isRecording = true;
                    std::cout << "开始录制 -> " << outVideoPath << std::endl;
                }
                else
                {
                    std::cout << "视频写入器初始化失败" << std::endl;
                }
            }
        }
        // 停止录制并退出预览
        if (key == 'q' || key == 'Q' || key == 27)
        {
            if (isRecording)
            {
                isRecording = false;
                writer.release();
                std::cout << "录制完成，视频已保存：" << outVideoPath << std::endl;
            }
            break;
        }
    }
    // 资源释放
    cap.release();
    writer.release();
#ifndef RUN_IN_DOCKER
    cv::destroyWindow("Camera Preview");
#endif
    return true;
}

// 将摄像头+去畸变+Tag识别逻辑封装成独立函数
int static runTagDetectRealTime(const TagDetectParams& params) {
    // ===================== 1. 鱼眼校正初始化 =====================
    FisheyeUndist undist;
    const std::string calibPath = "../RGBVison/fisheye_calib.xml";
    if (!undist.init(calibPath))
    {
        std::cout << "加载鱼眼标定文件失败：" << calibPath << std::endl;
        return -1;
    }
    const cv::Mat K = undist.getCameraMatrix();
    const cv::Mat D = undist.getDistCoeffs();
    const cv::Size img_size = undist.getImgSize();
    // ===================== 2. 检测器初始化（只执行一次） =====================
    std::shared_ptr<TagJointDetect> tagDet;
    TagType tagType = params.tagType;
    int dictId = params.dictId;
    std::string arucoDictStr = params.arucoDictStr;
    auto arucoType = StringToArucoDictType(arucoDictStr);
    AprilTagFamily aprilFamily = params.aprilFamily;
    double tagSize = params.tagSize;
    int rows = params.rows;
    int cols = params.cols;
    double tagDistX = params.tagDistX;
    double tagDistY = params.tagDistY;
    int arucoModStart = params.arucoModStart;
    int arucoTagStart = params.arucoTagStart;
    int aprilTagStart = params.aprilTagStart;
    if (tagType == TagType::ARUCO_MOD) {
        tagDet = std::make_shared<ArucoModJointDetect>(dictId, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoModStart);
    }
    else if (tagType == TagType::ARUCO_TAG) {
        tagDet = std::make_shared<ArucoTagJointDetect>(arucoType, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoTagStart);
    }
    else if (tagType == TagType::APRIL_TAG) {
        tagDet = std::make_shared<AprilTagJointDetect>(aprilFamily, tagSize, rows, cols);
        tagDet->setStartTagValue(aprilTagStart);
    }
    tagDet->setCameraParam(K, D);
    tagDet->setBoardSize(rows, cols);
    tagDet->setTagDist(tagDistX, tagDistY);
#ifndef RUN_IN_DOCKER
    VizBoardViewer viz_viewer(tagSize, K, img_size, rows, cols);
#endif
    // ===================== 3. 打开相机 =====================
    cv::VideoCapture cap = openCamera(0, img_size.width, img_size.height);
    if (!cap.isOpened())
    {
        std::cout << "相机打开失败，尝试更换设备号1/2" << std::endl;
        return -1;
    }
#ifndef RUN_IN_DOCKER
    // ===================== 4. 窗口初始化 =====================
    cv::namedWindow("correct image", cv::WINDOW_AUTOSIZE);
    cv::setWindowProperty("correct image", cv::WND_PROP_TOPMOST, 1);
#endif
    // 关闭原原图窗口，减少渲染开销
    // cv::namedWindow("origin image", cv::WINDOW_AUTOSIZE);

    // ===================== 全局常量（提前定义，避免循环内重复计算） =====================
    const int sample_interval_ms = 10;    // 位姿采样间隔 1s
    const int frame_delay_ms = 20;          // 控制整体帧率 ~50FPS (1000/50)
    const std::ios_base::fmtflags out_flags = std::cout.flags();
    std::cout << std::fixed << std::setprecision(4);      // 输出格式只设置一次
    // ===================== 循环变量（复用内存） =====================
    steady_clock::time_point last_sample_time = steady_clock::now();
    cv::Mat frame, frameFix;
    BoardResult lastBoardRes;
    bool has_valid_frame = false;
    while (true)
    {
        // 1. 读取相机帧：grab 丢弃旧帧，保证取最新画面
        if (!cap.grab())
        {
            cv::waitKey(frame_delay_ms);
            continue;
        }
        if (!cap.retrieve(frame))
        {
            std::cout << "相机读帧失败" << std::endl;
            cv::waitKey(frame_delay_ms);
            continue;
        }

        const auto now = steady_clock::now();
        const long long sample_delta = duration_cast<milliseconds>(now - last_sample_time).count();
        // 2. 每 1 秒执行一次：去畸变 + 检测 + 绘制 + 打印
        if (sample_delta >= sample_interval_ms)
        {
            auto start = steady_clock::now();
            frameFix = undist.undistortImage(frame);
            if (!frameFix.empty())
            {
                lastBoardRes = tagDet->detect(frameFix);
                tagDet->drawBoardResult(frameFix, lastBoardRes);
                has_valid_frame = true;
                // 打印位姿
                if (lastBoardRes.board_pose_valid)
                {
                    double cx = lastBoardRes.camera_tvec[0];
                    double cy = lastBoardRes.camera_tvec[1];
                    double cz = lastBoardRes.camera_tvec[2];
                    std::cout << "相机光心在以阵列中心为世界坐标原点的坐标(m) X: " << cx
                        << "  Y: " << cy << "  Z: " << cz << std::endl;
                    auto end = steady_clock::now();
                    double cost_time = duration_cast<milliseconds>(end - start).count();
                    printf("畸变校正和检测耗时%.3f ms\n", cost_time);
                    // viz_viewer.update3DView(lastBoardRes);
                }
            }
            last_sample_time = now;
        }
#ifndef RUN_IN_DOCKER
        // 3. 仅有效帧刷新画面（中间帧复用上次画面，不重复绘制）
        if (has_valid_frame && !frameFix.empty())
        {
            cv::imshow("correct image", frameFix);
        }
#endif
        // 4. 按键监听 & 控频（核心降CPU：用延时代替空循环）
        int key = cv::waitKey(frame_delay_ms);
        if (key == 'q' || key == 27)
        {
            break;
        }
    }

    std::cout.flags(out_flags);
    cap.release();
#ifndef RUN_IN_DOCKER
    viz_viewer.closeWindow();
    cv::destroyAllWindows();
#endif
    return 0;
}

int static runTagDetectDemo(const TagDetectParams& params) {
    // 1. 鱼眼校正初始化
    FisheyeUndist undist;
    const std::string calibPath = "../RGBVison/fisheye_calib.xml";
    if (!undist.init(calibPath))
    {
        std::cout << "加载鱼眼标定文件失败：" << calibPath << std::endl;
        return -1;
    }
    cv::Mat K = undist.getCameraMatrix();
    cv::Mat D = undist.getDistCoeffs();
    // 2. 初始化Tag检测器
    std::shared_ptr<TagJointDetect> tagDet;
    TagType tagType = params.tagType;
    int dictId = params.dictId;
    std::string arucoDictStr = params.arucoDictStr;
    auto arucoType = StringToArucoDictType(arucoDictStr);
    AprilTagFamily aprilFamily = params.aprilFamily;
    double tagSize = params.tagSize;
    int rows = params.rows;
    int cols = params.cols;
    double tagDistX = params.tagDistX;
    double tagDistY = params.tagDistY;
    int arucoModStart = params.arucoModStart;
    int arucoTagStart = params.arucoTagStart;
    int aprilTagStart = params.aprilTagStart;
    if (tagType == TagType::ARUCO_MOD) {
        tagDet = std::make_shared<ArucoModJointDetect>(dictId, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoModStart);
    }
    else if (tagType == TagType::ARUCO_TAG) {
        tagDet = std::make_shared<ArucoTagJointDetect>(arucoType, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoTagStart);
    }
    else if (tagType == TagType::APRIL_TAG) {
        tagDet = std::make_shared<AprilTagJointDetect>(aprilFamily, tagSize, rows, cols);
        tagDet->setStartTagValue(aprilTagStart);
    }
    tagDet->setCameraParam(K, D);
    tagDet->setBoardSize(rows, cols);
    tagDet->setTagDist(tagDistX, tagDistY);
#ifndef RUN_IN_DOCKER
    const cv::Size img_size = undist.getImgSize();
    VizBoardViewer viz_viewer(tagSize, K, img_size, rows, cols);
#endif
    // 3. 加载录制好的视频
    std::string videoPath;
    if (tagType == TagType::ARUCO_MOD) {
        videoPath = "../RGBVison/mp4/record_aruco_mod.mp4";
    }
    else if (tagType == TagType::ARUCO_TAG) {
        videoPath = "../RGBVison/mp4/record_aruco_tag.mp4";
    }
    else if (tagType == TagType::APRIL_TAG) {
        videoPath = "../RGBVison/mp4/record_april_tag.mp4";
    }
    cv::VideoCapture videoCap(videoPath);
    if (!videoCap.isOpened())
    {
        std::cout << "加载视频失败：" << videoPath << std::endl;
        return -1;
    }
#ifndef RUN_IN_DOCKER
    cv::namedWindow("Correct Image", cv::WINDOW_NORMAL);
    cv::setWindowProperty("Correct Image", cv::WND_PROP_TOPMOST, cv::WINDOW_GUI_NORMAL);
#endif
    // 定时参数：25毫秒采样一次位姿
    const int sample_interval_ms = 25;
    auto last_sample_time = steady_clock::now();
    cv::Mat frameVideo, frameFix;
    BoardResult lastBoardRes;

    std::cout << "\n===== 开始解析视频并检测Tag =====" << std::endl;

    while (true)
    {
        if (!videoCap.read(frameVideo))
        {
            std::cout << "视频播放完毕" << std::endl;
            break;
        }

        auto now = steady_clock::now();
        auto sample_delta = duration_cast<milliseconds>(now - last_sample_time).count();

        // 每1秒执行一次校正+检测
        if (sample_delta >= sample_interval_ms)
        {
            auto start = steady_clock::now();
            frameFix = undist.undistortImage(frameVideo);
            if (!frameFix.empty())
            {
                lastBoardRes = tagDet->detect(frameFix);
                tagDet->drawBoardResult(frameFix, lastBoardRes);
                // 打印相机光心在世界坐标系中的坐标
                if (lastBoardRes.board_pose_valid)
                {
                    double cx = lastBoardRes.camera_tvec[0];
                    double cy = lastBoardRes.camera_tvec[1];
                    double cz = lastBoardRes.camera_tvec[2];
                    std::cout << std::fixed << std::setprecision(4);
                    std::cout << "相机光心在以阵列中心为世界坐标原点的坐标(m) X: " << cx
                        << "  Y: " << cy << "  Z: " << cz << std::endl;
                    auto end = steady_clock::now();
                    double cost_time = duration_cast<milliseconds>(end - start).count();
                    printf("畸变校正和检测耗时%.3f ms\n", cost_time);
                    //viz_viewer.update3DView(lastBoardRes);
                }
            }
            last_sample_time = now;
        }
#ifndef RUN_IN_DOCKER
        // 画面刷新
        if (!frameFix.empty())
        {
            cv::imshow("Correct Image", frameFix);
        }
#endif
        // 中途退出
        int key = cv::waitKey(1);
        if (key == 'q' || key == 27)
        {
            std::cout << "手动退出视频解析" << std::endl;
            break;
        }
    }

    videoCap.release();
#ifndef RUN_IN_DOCKER
    viz_viewer.closeWindow();
    cv::destroyAllWindows();
#endif
    std::cout << "程序执行完成" << std::endl;
    return 0;
}

int static runChessBoardDetectDemo(const TagDetectParams& params) {
    std::string projectRoot = std::string(PROJECT_ROOT);
    // 1. 鱼眼校正初始化
    FisheyeUndist undist;
    const std::string calibPath = projectRoot + "/HiKVision/fisheye_calib.xml";
    if (!undist.init(calibPath))
    {
        std::cout << "加载鱼眼标定文件失败：" << calibPath << std::endl;
        return -1;
    }
    cv::Mat K = undist.getCameraMatrix();
    cv::Mat D = undist.getDistCoeffs();
    std::cout << K << std::endl;
    std::cout << D << std::endl;
    // 2. 初始化Tag检测器
    std::shared_ptr<TagJointDetect> tagDet;
    TagType tagType = params.tagType;
    int dictId = params.dictId;
    std::string arucoDictStr = params.arucoDictStr;
    auto arucoType = StringToArucoDictType(arucoDictStr);
    AprilTagFamily aprilFamily = params.aprilFamily;
    double tagSize = params.tagSize;
    int rows = params.rows;
    int cols = params.cols;
    double tagDistX = params.tagDistX;
    double tagDistY = params.tagDistY;
    int arucoModStart = params.arucoModStart;
    int arucoTagStart = params.arucoTagStart;
    int aprilTagStart = params.aprilTagStart;
    if (tagType == TagType::ARUCO_MOD) {
        tagDet = std::make_shared<ArucoModJointDetect>(dictId, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoModStart);
    }
    else if (tagType == TagType::ARUCO_TAG) {
        tagDet = std::make_shared<ArucoTagJointDetect>(arucoType, tagSize, rows, cols);
        tagDet->setStartTagValue(arucoTagStart);
    }
    else if (tagType == TagType::APRIL_TAG) {
        tagDet = std::make_shared<AprilTagJointDetect>(aprilFamily, tagSize, rows, cols);
        tagDet->setStartTagValue(aprilTagStart);
    }
    else if (tagType == TagType::CHESS_BOARD) {
        tagDet = std::make_shared<ChessBoardDetect>(cv::Size(cols, rows), tagSize);
        tagDet->setMultiArrayType(false);
    }
    tagDet->setCameraParam(K, D);
    tagDet->setBoardSize(rows, cols);
    tagDet->setTagDist(tagDistX, tagDistY);
#ifndef RUN_IN_DOCKER
    const cv::Size img_size = undist.getImgSize();
    VizBoardViewer viz_viewer(tagSize, K, img_size, rows, cols, false);
#endif
    // 3. 加载录制好的视频
    std::string videoPath;
    if (tagType == TagType::ARUCO_MOD) {
        videoPath = projectRoot + "/HiKVision/mp4/record_aruco_mod.mp4";
    }
    else if (tagType == TagType::ARUCO_TAG) {
        videoPath = projectRoot + "/HiKVision/mp4/record_aruco_tag.mp4";
    }
    else if (tagType == TagType::APRIL_TAG) {
        videoPath = projectRoot + "/HiKVision/mp4/record_april_tag.mp4";
    }
    else if (tagType == TagType::CHESS_BOARD) {
        videoPath = projectRoot + "/HiKVision/mp4/record_april_tag.mp4";
    }
    cv::VideoCapture videoCap(videoPath);
    /*if (!videoCap.isOpened())
    {
        std::cout << "加载视频失败：" << videoPath << std::endl;
        return -1;
    }*/
//#ifndef RUN_IN_DOCKER
//    cv::namedWindow("Correct Image", cv::WINDOW_NORMAL);
//    cv::setWindowProperty("Correct Image", cv::WND_PROP_TOPMOST, cv::WINDOW_GUI_NORMAL);
//#endif
    // 定时参数：25毫秒采样一次位姿
    const int sample_interval_ms = 25;
    auto last_sample_time = steady_clock::now();
    cv::Mat frameVideo, frameFix;
    BoardResult lastBoardRes;

    for (int i = 0; ; ++i) {
        std::string jpgPath = projectRoot + cv::format("/HiKVision/fisheye_img/Image_20260715101804668.bmp", i);
        cv::Mat img = cv::imread(jpgPath);
        if (img.empty()) break;
        auto start = steady_clock::now();
        frameFix = undist.undistortImage(img);
        if (!frameFix.empty())
        {
            lastBoardRes = tagDet->detect(frameFix);
            tagDet->drawBoardResult(frameFix, lastBoardRes);
            // 打印相机光心在世界坐标系中的坐标
            if (lastBoardRes.board_pose_valid)
            {
                double cx = lastBoardRes.camera_tvec[0];
                double cy = lastBoardRes.camera_tvec[1];
                double cz = lastBoardRes.camera_tvec[2];
                std::cout << std::fixed << std::setprecision(4);
                std::cout << "相机光心在以阵列中心为世界坐标原点的坐标(m) X: " << cx
                    << "  Y: " << cy << "  Z: " << cz << std::endl;
                auto end = steady_clock::now();
                double cost_time = duration_cast<milliseconds>(end - start).count();
                printf("畸变校正和检测耗时%.3f ms\n", cost_time);
                viz_viewer.update3DView(lastBoardRes);
            }
        }
    }

//  std::cout << "\n===== 开始解析视频并检测Tag =====" << std::endl;

//    while (true)
//    {
//        if (!videoCap.read(frameVideo))
//        {
//            std::cout << "视频播放完毕" << std::endl;
//            break;
//        }
//
//        auto now = steady_clock::now();
//        auto sample_delta = duration_cast<milliseconds>(now - last_sample_time).count();
//
//        // 每1秒执行一次校正+检测
//        if (sample_delta >= sample_interval_ms)
//        {
//            auto start = steady_clock::now();
//            frameFix = undist.undistortImage(frameVideo);
//            if (!frameFix.empty())
//            {
//                lastBoardRes = tagDet->detect(frameFix);
//                tagDet->drawBoardResult(frameFix, lastBoardRes);
//                // 打印相机光心在世界坐标系中的坐标
//                if (lastBoardRes.board_pose_valid)
//                {
//                    double cx = lastBoardRes.camera_tvec[0];
//                    double cy = lastBoardRes.camera_tvec[1];
//                    double cz = lastBoardRes.camera_tvec[2];
//                    std::cout << std::fixed << std::setprecision(4);
//                    std::cout << "相机光心在以阵列中心为世界坐标原点的坐标(m) X: " << cx
//                        << "  Y: " << cy << "  Z: " << cz << std::endl;
//                    auto end = steady_clock::now();
//                    double cost_time = duration_cast<milliseconds>(end - start).count();
//                    printf("畸变校正和检测耗时%.3f ms\n", cost_time);
//                    // viz_viewer.update3DView(lastBoardRes);
//                }
//            }
//            last_sample_time = now;
//        }
//#ifndef RUN_IN_DOCKER
//        // 画面刷新
//        if (!frameFix.empty())
//        {
//            cv::imshow("Correct Image", frameFix);
//        }
//#endif
//        // 中途退出
//        int key = cv::waitKey(1);
//        if (key == 'q' || key == 27)
//        {
//            std::cout << "手动退出视频解析" << std::endl;
//            break;
//        }
//    }

    videoCap.release();
#ifndef RUN_IN_DOCKER
    viz_viewer.closeWindow();
    cv::destroyAllWindows();
#endif
    std::cout << "程序执行完成" << std::endl;
    return 0;
}

static int runCalculateCameraPose() {
    //初始化SDK，传入JSON配置根目录
    using namespace barcode_detect;
    std::string projectRoot = std::string(PROJECT_ROOT);
    int ret = InitBarcodeSDK(projectRoot.c_str());
    if (ret != 0)
    {
        std::cout << "SDK初始化失败！" << std::endl;
        return -1;
    }
    //构造测试图像输入
    for (int i = 0; ; ++i) {
        std::string jpgPath = projectRoot + cv::format("/mp4/aprilFrameVideo/%02d.jpg", i);
        cv::Mat img = cv::imread(jpgPath);
        if (img.empty()) break;
        //计算calculateCamera6DoFsPose
        auto start = steady_clock::now();
        const auto& pose = calculateCamera6DoFsPose(img);
        auto end = steady_clock::now();
        double cost_time = duration_cast<milliseconds>(end - start).count();
        printf("畸变校正和检测耗时%.3f ms\n", cost_time);
        //打印输出结果，验证返回值
        std::cout << "第" << i << "张图像：" << (pose.valid ? "检测成功" : "无Tag") << std::endl;
        if (pose.valid)
        {
            std::cout << "Rvec:" << pose.rvec << " Tvec:" << pose.tvec << std::endl;
        }
    }
    
    ReleaseBarcodeSDK();

    return 0;
}

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001); // 控制台切换UTF8(65001),中文不乱码
#endif
    int ret = -1;
    std::string visionDir = "HIKVision";
    //ret = runUndistortDemo(visionDir);
    
    //调用独立录制函数：摄像头录制视频
    /*const std::string videoFile = "../mp4/record_aruco_tag.mp4";
    bool recOk = recordVideoFromCamera(videoFile);
    if (!recOk)
    {
        std::cout << "视频录制流程异常，退出" << std::endl;
        return -1;
    }*/
    TagDetectParams params;
    params.rows = 8;
    params.cols = 11;
    params.tagSize = 0.01;
    params.tagDistX = 0.01;
    params.tagDistY = 0.01;
    params.tagType = TagType::CHESS_BOARD;
    std::cout<<"rows="<<params.rows<<" cols="<<params.cols<<std::endl;
    ret = runChessBoardDetectDemo(params);
    //ret = runTagDetectRealTime(params);
    //ret = runTagDetectDemo(params);
    //ret = runCalculateCameraPose();
    return ret;
}
