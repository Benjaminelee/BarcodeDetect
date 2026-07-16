@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
:: ===================== 默认配置 =====================
set "BUILD_SHARED_LIBS=OFF"
set "BUILD_DEMO=ON"
set "TARGET_CONFIG=Release"
set "CUSTOM_INSTALL="
set "VS_TOOLS_PATH="
:: ====================================================

:: 解析命令行参数
:parse_args
if "%~1"=="" goto parse_end
if /i "%~1"=="--shared" (
    set "BUILD_SHARED_LIBS=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--demo" (
    set "BUILD_DEMO=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--type" (
    set "TARGET_CONFIG=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--install" (
    set "CUSTOM_INSTALL=%~2"
    shift
    shift
    goto parse_args
)
echo "未知参数: %~1"
echo "使用示例:"
echo "build_win64.bat              默认静态库 Release 编译Demo"
echo "build_win64.bat --shared ON --type Debug 动态库Debug版本"
echo "build_win64.bat --install D:\SDK\BarcodeWin64 自定义输出目录"
pause
exit /b 1
:parse_end

:: 校验配置参数合法性
if /i not "!BUILD_SHARED_LIBS!"=="ON" if /i not "!BUILD_SHARED_LIBS!"=="OFF" (
    echo "错误：--shared 仅支持 ON/OFF"
    pause
    exit /b 1
)
if /i not "!BUILD_DEMO!"=="ON" if /i not "!BUILD_DEMO!"=="OFF" (
    echo "错误：--demo 仅支持 ON/OFF"
    pause
    exit /b 1
)
if /i not "!TARGET_CONFIG!"=="Release" if /i not "!TARGET_CONFIG!"=="Debug" (
    echo "错误：--type 仅支持 Release/Debug"
    pause
    exit /b 1
)

:: 获取脚本所在目录 = 工程根目录
set "PROJ_ROOT=%~dp0"
set "PROJ_ROOT=!PROJ_ROOT:~0,-1!"
set "BUILD_DIR=!PROJ_ROOT!\build_win64"

:: 安装目录处理
if defined CUSTOM_INSTALL (
    set "INSTALL_DIR=!CUSTOM_INSTALL!"
) else (
    set "INSTALL_DIR=!PROJ_ROOT!\install_win64"
)

echo "====================================="
echo "  Barcode Win64 一键编译打包脚本"
echo "  工程根目录: !PROJ_ROOT!"
echo "  构建目录:   !BUILD_DIR!"
echo "  SDK输出目录:!INSTALL_DIR!"
echo "  编译配置:   !TARGET_CONFIG!"
echo "  动态库:     !BUILD_SHARED_LIBS!"
echo "  编译Demo:   !BUILD_DEMO!"
echo "====================================="

:: 1. 自动加载VS x64编译环境（无需手动打开VS命令行）
echo "[0/6] 检索VS编译环境..."
for /f "usebackq delims=" %%i in (`"%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL=%%i"
)
if not defined VS_INSTALL (
    echo "警告：未自动检索到VS，请手动在 VS x64 Native Tools 命令行运行脚本！"
) else (
    call "!VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
    echo "✅ 已自动加载VS x64编译环境"
)

:: 依赖检查 cmake
where cmake >nul 2>&1
if errorlevel 1 (
    echo "[错误] 未找到 cmake，请配置环境变量后重试！"
    pause
    exit /b 1
)

:: 检查7z（修复逻辑：找到7z=1，无=0）
set "HAVE_7Z=0"
where 7z >nul 2>&1
if errorlevel 0 set "HAVE_7Z=1"

echo "[1/6] 清理旧构建缓存、旧SDK文件夹..."
rmdir /s /q "!BUILD_DIR!" 2>nul
rmdir /s /q "!INSTALL_DIR!" 2>nul

echo "[2/6] 创建构建目录"
mkdir "!BUILD_DIR!"
cd /d "!BUILD_DIR!"

echo "[3/6] CMake 生成x64 VS工程"
cmake "!PROJ_ROOT!" ^
-DCMAKE_INSTALL_PREFIX="!INSTALL_DIR!" ^
-DBUILD_SHARED_LIBS="!BUILD_SHARED_LIBS!" ^
-DBUILD_DEMO="!BUILD_DEMO!" ^
-A x64
if errorlevel 1 (
    echo "❌ CMake 配置失败！"
    pause
    exit /b 1
)

echo "[4/6] MSVC 多核编译 !TARGET_CONFIG!"
cmake --build . --config !TARGET_CONFIG! -j !NUMBER_OF_PROCESSORS!
if errorlevel 1 (
    echo "❌ 编译失败！"
    pause
    exit /b 1
)

echo "[5/6] Install 输出完整SDK"
cmake --install . --config !TARGET_CONFIG!
echo "输出完整SDK完成"

:: 打包SDK压缩包（修复目录结构问题）
echo "[6/6] SDK压缩包"

set "ZIP_PACK=!PROJ_ROOT!\barcode_sdk_win64.zip"
echo "调试：HAVE_7Z=!HAVE_7Z!"
echo "调试：INSTALL_DIR=!INSTALL_DIR!"
echo "调试：ZIP_PACK=!ZIP_PACK!"
if exist "!ZIP_PACK!" (
    del /f /q "!ZIP_PACK!" 2>nul || echo "旧压缩包被占用，跳过删除"
)
if !HAVE_7Z! equ 1 (
    7z a -tzip "!ZIP_PACK!" -mx=9 "!INSTALL_DIR!\*" 2>&1 >nul
    echo "已生成压缩包：!ZIP_PACK!"
) else (
    echo "未检测到7z，跳过打包，手动压缩 !INSTALL_DIR! 交付"
)

:: 完成提示
echo "====================================="
echo "Windows 编译打包全部完成！"
if /i "!BUILD_DEMO!"=="ON" (
    echo "Demo程序路径: !BUILD_DIR!\!TARGET_CONFIG!\BarcodeDemo.exe"
)
echo "完整SDK目录: !INSTALL_DIR!"
echo ""
echo "重要运行提示"
echo "1. 动态库模式：将 !INSTALL_DIR!\bin 加入PATH，或复制所有dll到exe同级目录"
echo "2. 编译运行库统一使用 /MD，客户工程请勿混用 /MT"
echo "====================================="
pause
endlocal