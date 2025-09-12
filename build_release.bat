@echo off
chcp 65001 >nul
echo 正在构建 MedYOLO11Qt 发行版本...
echo.

REM 检查是否在项目根目录
if not exist "CMakeLists.txt" (
    echo 错误：请在项目根目录运行此脚本
    pause
    exit /b 1
)

REM 清理build目录
echo 清理构建目录...
if exist "build" (
    rmdir /s /q build
)
mkdir build

REM 配置CMake（Release模式）
echo 配置CMake（Release模式）...
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake配置失败
    pause
    exit /b 1
)

REM 编译项目
echo 编译项目...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo 编译失败
    pause
    exit /b 1
)

REM 检查必要的文件是否存在
echo 检查生成的文件...
if not exist "Release\medapp.exe" (
    echo 错误：主程序未生成
    pause
    exit /b 1
)

if not exist "Release\platforms\qwindows.dll" (
    echo 错误：Qt平台插件未复制
    pause
    exit /b 1
)

if not exist "..\models\fai_xray.onnx" (
    echo 警告：模型文件不存在
)

REM 复制模型文件到build目录（如果存在）
if exist "..\models\fai_xray.onnx" (
    echo 复制模型文件...
    if not exist "Release\..\models" mkdir "Release\..\models"
    copy "..\models\fai_xray.onnx" "Release\..\models\" >nul
)

echo.
echo 构建完成！文件位于 build\Release 目录
echo.
echo 下一步：
echo 1. 安装 NSIS (Nullsoft Install System)
echo 2. 右键点击 installer.nsi -> 编译NSIS脚本
echo 3. 生成的 MedYOLO11Qt_Setup.exe 即可分发给客户

pause