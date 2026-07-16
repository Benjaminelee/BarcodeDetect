#ifndef BARCODE_SDK_EXPORT_H
#define BARCODE_SDK_EXPORT_H

#ifdef _WIN32
#ifdef BARCODE_SDK_EXPORT_H
#define BARCODE_API __declspec(dllexport)
#else
#define BARCODE_API __declspec(dllimport)
#endif
#else
#define BARCODE_API
#endif

#endif //BARCODE_SDK_EXPORT_H