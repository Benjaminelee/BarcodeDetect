#include "../include/HikCamera.h"
#include <cstring>
#include <chrono>
#include <cstdio>

SdkLifeGuard::SdkLifeGuard()
    : m_bInitSuccess(false)
{
    m_bInitSuccess = HikCamera::InitSDK();
    if (m_bInitSuccess)
    {
        printf("[SdkGuard] SDK全局初始化成功\n");
    }
    else
    {
        printf("[SdkGuard] SDK全局初始化失败\n");
    }
}

SdkLifeGuard::~SdkLifeGuard()
{
    if (m_bInitSuccess)
    {
        HikCamera::FinalizeSDK();
        printf("[SdkGuard] SDK全局资源自动释放完成\n");
    }
}

bool SdkLifeGuard::IsSdkReady() const
{
    return m_bInitSuccess;
}

HikCamera::HikCamera()
    : m_handle(nullptr)
    , m_bGrabbing(false)
    , m_bExit(false)
{
}

HikCamera::~HikCamera()
{
    CloseCamera();
}

bool HikCamera::InitSDK()
{
    int nRet = MV_CC_Initialize();
    if (MV_OK != nRet)
    {
        printf("[SDK] Initialize failed, nRet[0x%x]\n", nRet);
        return false;
    }
    printf("[SDK] Initialize success\n");
    return true;
}

void HikCamera::FinalizeSDK()
{
    MV_CC_Finalize();
    printf("[SDK] Finalize done\n");
}

bool HikCamera::IsOpened() const
{
    return m_handle != nullptr;
}

bool HikCamera::IsGrabbing() const
{
    return m_bGrabbing;
}

void HikCamera::PrintUsbDeviceInfo(MV_CC_DEVICE_INFO* pDevInfo)
{
    if (nullptr == pDevInfo)
    {
        printf("[DevInfo] Device info ptr is null\n");
        return;
    }
    if (MV_USB_DEVICE != pDevInfo->nTLayerType)
    {
        printf("[DevInfo] Not USB device\n");
        return;
    }
    auto& stUsbInfo = pDevInfo->SpecialInfo.stUsb3VInfo;
    printf("==== USB Camera Info ====\n");
    printf("UserDefinedName: %s\n", stUsbInfo.chUserDefinedName);
    printf("Serial Number: %s\n", stUsbInfo.chSerialNumber);
    printf("Device Number: %d\n", stUsbInfo.nDeviceNumber);
    printf("Model Name: %s\n\n", stUsbInfo.chModelName);
}

bool HikCamera::SetBaseParam(void* handle)
{
    int nRet = MV_OK;
    // 连续采集，关闭触发
    nRet = MV_CC_SetEnumValue(handle, "TriggerMode", MV_TRIGGER_MODE_OFF);
    if (MV_OK != nRet)
    {
        printf("[Param] Set TriggerMode fail, nRet[0x%x]\n", nRet);
        return false;
    }
    // 固定曝光，关闭自动曝光
    nRet = MV_CC_SetEnumValue(handle, "ExposureAuto", 0);
    if (MV_OK == nRet)
    {
        MV_CC_SetFloatValue(handle, "ExposureTime", 39000.0f);
    }
    // 关闭自动增益
    nRet = MV_CC_SetEnumValue(handle, "GainAuto", 0);
    if (MV_OK == nRet)
    {
        MV_CC_SetFloatValue(handle, "Gain", 3.0f);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    return true;
}

bool HikCamera::OpenUsbCamera()
{
    if (m_handle != nullptr)
    {
        printf("[Cam] Camera already opened\n");
        return true;
    }

    int nRet = MV_OK;
    MV_CC_DEVICE_INFO_LIST stDevList;
    memset(&stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    // 仅枚举USB设备，和需求匹配
    nRet = MV_CC_EnumDevices(MV_USB_DEVICE, &stDevList);
    if (MV_OK != nRet)
    {
        printf("[Enum] Enum USB device fail, nRet[0x%x]\n", nRet);
        return false;
    }
    if (0 == stDevList.nDeviceNum)
    {
        printf("[Enum] No USB camera found\n");
        return false;
    }

    // 打印第0台USB相机信息
    PrintUsbDeviceInfo(stDevList.pDeviceInfo[0]);

    // 创建句柄
    nRet = MV_CC_CreateHandle(&m_handle, stDevList.pDeviceInfo[0]);
    if (MV_OK != nRet)
    {
        printf("[Handle] Create handle fail, nRet[0x%x]\n", nRet);
        m_handle = nullptr;
        return false;
    }

    // 打开设备
    nRet = MV_CC_OpenDevice(m_handle);
    if (MV_OK != nRet)
    {
        printf("[Open] Open device fail, nRet[0x%x]\n", nRet);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // 设置基础参数
    if (!SetBaseParam(m_handle))
    {
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // 注册Ex2回调，自动释放帧缓存 bAutoFree=true（官方标准）
    nRet = MV_CC_RegisterImageCallBackEx2(m_handle, ImageCallbackEx2, this, true);
    if (MV_OK != nRet)
    {
        printf("[Callback] Register Ex2 callback fail, nRet[0x%x]\n", nRet);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // 启动采集
    nRet = MV_CC_StartGrabbing(m_handle);
    if (MV_OK != nRet)
    {
        printf("[Grab] Start grabbing fail, nRet[0x%x]\n", nRet);
        // 异常分支严格按官方流程清理
        MV_CC_RegisterImageCallBackEx2(m_handle, nullptr, nullptr, true);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_bGrabbing = true;
    m_bExit = false;
    // 官方Demo等待流稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    printf("[Cam] USB camera grab start success\n");
    return true;
}

void __stdcall HikCamera::ImageCallbackEx2(MV_FRAME_OUT* pstFrame, void* pUser, bool bAutoFree)
{
    HikCamera* pCam = static_cast<HikCamera*>(pUser);
    if (!pCam || pCam->m_bExit.load() || !pstFrame || !pstFrame->pBufAddr)
    {
        // 官方自动释放模式无需手动FreeImageBuffer
        return;
    }

    auto& stInfo = pstFrame->stFrameInfo;
    unsigned int w = stInfo.nWidth;
    unsigned int h = stInfo.nHeight;
    cv::Mat rawMat;

    // 仅拷贝原始数据，不做耗时转换（官方推荐轻量回调）
    if (stInfo.enPixelType == PixelType_Gvsp_Mono8)
    {
        rawMat = cv::Mat(h, w, CV_8UC1, pstFrame->pBufAddr).clone();
    }
    else
    {
        rawMat = cv::Mat(h, w, CV_8UC3, pstFrame->pBufAddr).clone();
    }

    // 入队，队列满丢弃旧帧防内存溢出
    std::lock_guard<std::mutex> lock(pCam->m_queueMtx);
    if (pCam->m_frameQueue.size() >= MAX_FRAME_QUEUE)
    {
        pCam->m_frameQueue.pop();
    }
    pCam->m_frameQueue.push(rawMat);
    pCam->m_cvFrame.notify_one();
}

cv::Mat HikCamera::GetFrame(uint32_t timeoutMs)
{
    if (!IsOpened() || !m_bGrabbing || m_bExit.load())
    {
        printf("[GetFrame] Camera not ready\n");
        return cv::Mat();
    }

    std::unique_lock<std::mutex> lock(m_queueMtx);
    bool hasFrame = m_cvFrame.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]()
        {
            return !m_frameQueue.empty() || m_bExit.load();
        });

    if (m_bExit.load())
        return cv::Mat();
    if (!hasFrame)
    {
        printf("[GetFrame] Timeout\n");
        return cv::Mat();
    }

    cv::Mat src = m_frameQueue.front();
    m_frameQueue.pop();
    lock.unlock();

    // 彩色Bayer转BGR放到主线程，不阻塞回调
    if (src.channels() == 1)
    {
        return src;
    }
    else
    {
        unsigned int w = src.cols;
        unsigned int h = src.rows;
        MV_CC_PIXEL_CONVERT_PARAM conv = { 0 };
        conv.pSrcData = src.data;
        conv.nSrcDataLen = w * h * 3;
        conv.nWidth = w;
        conv.nHeight = h;
        conv.enSrcPixelType = PixelType_Gvsp_BayerRG8;
        conv.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
        conv.nDstBufferSize = w * h * 3;

        cv::Mat dst(h, w, CV_8UC3);
        conv.pDstBuffer = dst.data;
        MV_CC_ConvertPixelType(m_handle, &conv);
        return dst;
    }
}

bool HikCamera::SetExposureTimeUs(int expUs)
{
    int nRet = MV_CC_SetFloatValue(m_handle, "ExposureTime", static_cast<float>(expUs));
    if (MV_OK != nRet)
    {
        std::cout << "设置曝光失败!" << std::endl;
        return false;
    }
    return true;
}

int HikCamera::GetExposureTimeUs()
{
    MVCC_FLOATVALUE stFloatValue;
    memset(&stFloatValue, 0, sizeof(MVCC_FLOATVALUE));
    MV_CC_GetFloatValue(m_handle, "ExposureTime", &stFloatValue);
    return static_cast<int>(stFloatValue.fCurValue);
}

void HikCamera::CloseCamera()
{
    m_bExit = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (m_handle == nullptr)
        return;

    // ========== 严格复刻官方释放顺序 ==========
    if (m_bGrabbing)
    {
        int nRet = MV_CC_StopGrabbing(m_handle);
        if (MV_OK != nRet)
        {
            printf("[Close] StopGrab fail, nRet[0x%x]\n", nRet);
        }
        m_bGrabbing = false;
    }
    // 注销回调
    int nRet = MV_CC_RegisterImageCallBackEx2(m_handle, nullptr, nullptr, true);
    if (MV_OK != nRet)
    {
        printf("[Close] Unreg callback fail, nRet[0x%x]\n", nRet);
    }
    // 关闭设备
    nRet = MV_CC_CloseDevice(m_handle);
    if (MV_OK != nRet)
    {
        printf("[Close] CloseDevice fail, nRet[0x%x]\n", nRet);
    }
    // 销毁句柄
    nRet = MV_CC_DestroyHandle(m_handle);
    if (MV_OK != nRet)
    {
        printf("[Close] DestroyHandle fail, nRet[0x%x]\n", nRet);
    }
    m_handle = nullptr;

    // 清空图像队列
    std::lock_guard<std::mutex> lock(m_queueMtx);
    std::queue<cv::Mat>().swap(m_frameQueue);
    printf("[Cam] Camera resource released\n");
}