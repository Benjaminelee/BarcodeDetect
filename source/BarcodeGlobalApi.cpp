#include "../include/BarcodeGlobalApi.h"
#include "../include/BarcodeSDKExport.h"

#include "../include/TypeDef.h"
#include "../include/UndistortCam.h"
#include "../include/TagJointDetect.h"
#include "../include/ArucoModJointDetect.h"
#include "../include/ArucoTagJointDetect.h"
#include "../include/AprilTagJointDetect.h"

// 第三方JSON库，推荐单头文件nlohmann/json
#include "../include/json.hpp"
using json = nlohmann::json;

#include <fstream>
#include <iostream>
#include <string>
#include <memory>


// 全局缓存检测参数表
static FisheyeUndist g_undist;
static TagDetectParams g_tagDetectParams;

// ========== 私有工具：JSON字符串转枚举 ==========
static TagType StringToTagType(const std::string& str)
{
    if (str == "ARUCO_MOD") return TagType::ARUCO_MOD;
    if (str == "ARUCO_TAG") return TagType::ARUCO_TAG;
    if (str == "APRIL_TAG") return TagType::APRIL_TAG;
    return TagType::ARUCO_MOD;
}

static aruco::Dictionary::DICT_TYPES StringToArucoDictType(const std::string& str)
{
    aruco::Dictionary::DICT_TYPES arucoType = aruco::Dictionary::DICT_TYPES::ARUCO_MIP_36h12;
    if (aruco::Dictionary::isPredefinedDictinaryString(str)) {
        arucoType = aruco::Dictionary::getTypeFromString(str);
    }
    return arucoType;
}

static AprilTagFamily StringToAprilFamily(const std::string& str)
{
    if (str == "TAG_36H11") return AprilTagFamily::TAG_36H11;
    else if (str == "TAG_16H5") return AprilTagFamily::TAG_16H5;
    else if (str == "TAG_25H9") return AprilTagFamily::TAG_25H9;
    else if (str == "TAG_36H10") return AprilTagFamily::TAG_36H10;
    return AprilTagFamily::TAG_36H11;
}

namespace barcode_detect {
    // ========== 对外导出API：仅传入图像，全部参数从JSON读取 ==========
    int InitBarcodeSDK(const char* cfgPath)
    {
        if (cfgPath == nullptr) return -1;
        std::string sdkInstallRoot = std::string(cfgPath);
        // 拼接JSON配置完整路径
        std::string jsonPath = sdkInstallRoot + "/config/camera_calc_config.json";
        std::ifstream jsonFile(jsonPath);
        if (!jsonFile.is_open())
        {
            std::cout << "无法打开6DoF计算配置文件：" << jsonPath << std::endl;
            return -1;
        }
        // 读取并解析JSON
        json cfgJson;
        try
        {
            jsonFile >> cfgJson;
        }
        catch (const std::exception& e)
        {
            std::cout << "JSON配置解析失败：" << e.what() << std::endl;
            return -1;
        }
        // 从JSON读取全部检测算法参数
        std::string calibName = cfgJson.value<std::string>("fisheye_calib_name", "fisheye_calib.xml");
        g_tagDetectParams.calibPath = sdkInstallRoot + "/" + calibName;
        //鱼眼校正初始化
        if (!g_undist.init(g_tagDetectParams.calibPath))
        {
            std::cout << "加载鱼眼标定文件失败：" << g_tagDetectParams.calibPath << std::endl;
            return -1;
        }

        std::string tagTypeStr = cfgJson.value<std::string>("tagTypeStr", "APRIL_TAG");
        g_tagDetectParams.tagType = StringToTagType(tagTypeStr);

        g_tagDetectParams.dictId = cfgJson.value<int>("dictId", cv::aruco::DICT_4X4_50);
        g_tagDetectParams.arucoDictStr = cfgJson.value<std::string>("arucoDictStr", "ARUCO_MIP_36h12");

        std::string aprilFamilyStr = cfgJson.value<std::string>("aprilFamilyStr", "TAG_36H11");
        g_tagDetectParams.aprilFamily = StringToAprilFamily(aprilFamilyStr);

        g_tagDetectParams.tagSize = cfgJson.value<double>("tagSize", 0.02255);
        g_tagDetectParams.rows = cfgJson.value<int>("boardRows", 4);
        g_tagDetectParams.cols = cfgJson.value<int>("boardCols", 5);
        g_tagDetectParams.tagDistX = cfgJson.value<double>("tagDistX", 0.0451);
        g_tagDetectParams.tagDistY = cfgJson.value<double>("tagDistY", 0.0451);
        g_tagDetectParams.arucoModStart = cfgJson.value<int>("arucomod_start", 0);
        g_tagDetectParams.arucoTagStart = cfgJson.value<int>("arucotag_start", 0);
        g_tagDetectParams.aprilTagStart = cfgJson.value<int>("apriltag_start", 560);

        return 0;
    }

    CameraPoseVec calculateCamera6DoFsPose(const cv::Mat& image)
    {
        CameraPoseVec onePose;
        // 1. 鱼眼校正初始化
        cv::Mat K = g_undist.getCameraMatrix();
        cv::Mat D = g_undist.getDistCoeffs();

        // 2.初始化Tag检测参数表
        std::shared_ptr<TagJointDetect> tagDet;
        TagType tagType = g_tagDetectParams.tagType;
        int dictId = g_tagDetectParams.dictId;
        std::string arucoDictStr = g_tagDetectParams.arucoDictStr;
        auto arucoType = StringToArucoDictType(arucoDictStr);
        AprilTagFamily aprilFamily = g_tagDetectParams.aprilFamily;
        double tagSize = g_tagDetectParams.tagSize;
        int rows = g_tagDetectParams.rows;
        int cols = g_tagDetectParams.cols;
        double tagDistX = g_tagDetectParams.tagDistX;
        double tagDistY = g_tagDetectParams.tagDistY;
        int arucoModStart = g_tagDetectParams.arucoModStart;
        int arucoTagStart = g_tagDetectParams.arucoTagStart;
        int aprilTagStart = g_tagDetectParams.aprilTagStart;

        // 3.初始化Tag检测器
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

        // 4.计算单张图6DoF
        cv::Mat undist_image = g_undist.undistortImage(image);
        if (!undist_image.empty()) {
            const BoardResult& res = tagDet->detect(undist_image);
            if (res.board_pose_valid) {
                onePose.valid = res.board_pose_valid;
                onePose.rvec = res.camera_rvec;
                onePose.tvec = res.camera_tvec;
            }
            else {
                onePose.valid = false;
            }
        }
        else {
            onePose.valid = false;
        }
        return onePose;
    }

    void ReleaseBarcodeSDK()
    {
        g_tagDetectParams.calibPath = "";
        g_tagDetectParams.tagType = TagType::APRIL_TAG;
        g_tagDetectParams.dictId = 0;
        g_tagDetectParams.arucoDictStr = "ARUCO_MIP_36h12";
        g_tagDetectParams.aprilFamily = AprilTagFamily::TAG_36H11;
        g_tagDetectParams.tagSize = 0.02255;
        g_tagDetectParams.rows = 4;
        g_tagDetectParams.cols = 5;
        g_tagDetectParams.tagDistX = 0.0451;
        g_tagDetectParams.tagDistY = 0.0451;
        g_tagDetectParams.arucoModStart = 0;
        g_tagDetectParams.arucoTagStart = 0;
        g_tagDetectParams.aprilTagStart = 560;
    }
}