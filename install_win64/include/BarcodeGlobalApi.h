#ifndef BARCODE_GLOBAL_API_H
#define BARCODE_GLOBAL_API_H

#include "BarcodeSDKExport.h"
#include "opencv2/core/types.hpp"

namespace barcode_detect {
    // 对外结构体、枚举
    struct BARCODE_API CameraPoseVec {
        bool valid = false;
        cv::Vec3d rvec;
        cv::Vec3d tvec;
    };

    // 对外接口，前面加 BARCODE_API
    BARCODE_API int InitBarcodeSDK(const char* cfgPath);
    BARCODE_API void ReleaseBarcodeSDK();
    BARCODE_API CameraPoseVec calculateCamera6DoFsPose(const cv::Mat& image);
}
#endif //BARCODE_GLOBAL_API_H