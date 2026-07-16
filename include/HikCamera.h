#ifndef HIK_CAMERA_H
#define HIK_CAMERA_H

#include "MvCameraControl.h"
#include <opencv2/opencv.hpp>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <cstdint>

#define MAX_FRAME_QUEUE 12

// RAII SDK生命周期守卫，全局SDK单例管理，自动Init/Finalize
class SdkLifeGuard
{
public:
    SdkLifeGuard();
    ~SdkLifeGuard();
    // 判断SDK是否初始化成功
    bool IsSdkReady() const;
private:
    bool m_bInitSuccess;
};


class HikCamera
{
public:
    HikCamera();
    ~HikCamera();

    // 全局SDK初始化（程序启动调用一次）
    static bool InitSDK();
    // 全局SDK反初始化（程序退出调用一次）
    static void FinalizeSDK();

    // 打开第0台USB相机，完整流程：枚举->创建句柄->打开->参数配置->注册回调->启动采集
    bool OpenUsbCamera();

    // 阻塞获取图像，timeoutMs超时返回空Mat
    cv::Mat GetFrame(uint32_t timeoutMs = 10);

    // 停止采集、注销回调、关闭设备、销毁句柄
    void CloseCamera();

    // 状态查询
    bool IsOpened() const;
    bool IsGrabbing() const;

private:
    // 官方标准Ex2回调原型 __stdcall
    static void __stdcall ImageCallbackEx2(MV_FRAME_OUT* pstFrame, void* pUser, bool bAutoFree);

    // 打印USB相机设备信息（参考官方PrintDeviceInfo）
    static void PrintUsbDeviceInfo(MV_CC_DEVICE_INFO* pDevInfo);

    // 设置基础通用参数（连续采集、关闭触发）
    bool SetBaseParam(void* handle);

    void* m_handle;
    bool m_bGrabbing;

    std::mutex m_queueMtx;
    std::condition_variable m_cvFrame;
    std::queue<cv::Mat> m_frameQueue;
    std::atomic<bool> m_bExit;
};

#endif