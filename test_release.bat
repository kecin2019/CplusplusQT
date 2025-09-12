@echo off
chcp 65001 >nul
echo 测试 MedYOLO11Qt 发行版本...
echo.

REM 检查是否在项目根目录
if not exist "build\Release\medapp.exe" (
    echo 错误：请先运行 build_release.bat 构建发行版本
    pause
    exit /b 1
)

echo 检查必要的文件...
echo.

REM 检查主程序
if exist "build\Release\medapp.exe" (
    echo ✓ medapp.exe 存在
) else (
    echo ✗ medapp.exe 不存在
)

REM 检查Qt DLLs
if exist "build\Release\Qt6Core.dll" (
    echo ✓ Qt6Core.dll 存在
) else (
    echo ✗ Qt6Core.dll 不存在
)

if exist "build\Release\Qt6Gui.dll" (
    echo ✓ Qt6Gui.dll 存在
) else (
    echo ✗ Qt6Gui.dll 不存在
)

if exist "build\Release\Qt6Widgets.dll" (
    echo ✓ Qt6Widgets.dll 存在
) else (
    echo ✗ Qt6Widgets.dll 不存在
)

REM 检查ONNXRuntime
if exist "build\Release\onnxruntime.dll" (
    echo ✓ onnxruntime.dll 存在
) else (
    echo ✗ onnxruntime.dll 不存在
)

REM 检查GDCM DLLs
set gdcm_count=0
for %%i in (build\Release\gdcm*.dll) do set /a gdcm_count+=1
if !gdcm_count! gtr 0 (
    echo ✓ 找到 !gdcm_count! 个 GDCM DLL 文件
) else (
    echo ✗ 没有找到 GDCM DLL 文件
)

REM 检查平台插件
if exist "build\Release\platforms\qwindows.dll" (
    echo ✓ Qt平台插件存在
) else (
    echo ✗ Qt平台插件不存在
)

REM 检查模型文件
if exist "build\Release\..\models\fai_xray.onnx" (
    echo ✓ 模型文件存在
) else (
    echo ✗ 模型文件不存在
)

echo.
echo 文件检查完成。
echo.
echo 现在尝试运行程序（5秒后自动关闭）...
start "" /wait "build\Release\medapp.exe"

echo.
echo 测试完成！如果程序正常运行，发行版本构建成功。
echo.
echo 下一步：使用 NSIS 编译 installer.nsi 创建安装包
pause