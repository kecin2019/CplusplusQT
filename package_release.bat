@echo off
chcp 65001 >nul
echo 使用 CPack 生成 MedYOLO11Qt 安装包...
echo.

REM 检查是否在项目根目录
if not exist "CMakeLists.txt" (
    echo 错误：请在项目根目录运行此脚本
    pause
    exit /b 1
)

REM 检查是否已构建Release版本
if not exist "build\Release\medapp.exe" (
    echo 错误：请先运行 build_release.bat 构建发行版本
    pause
    exit /b 1
)

REM 进入build目录
cd build

REM 使用CPack生成安装包
echo 生成安装包...
cpack -C Release

if %errorlevel% neq 0 (
    echo CPack生成失败
    pause
    exit /b 1
)

echo.
echo 安装包生成完成！
echo.
REM 查找生成的安装包
for %%i in (MedYOLO11Qt-*.exe) do (
    echo 生成的安装包: %%i
    echo 文件大小: %%~zi 字节
)

echo.
echo 安装包已准备好分发给客户。
echo.
echo 安装包功能：
echo - 自动安装所有依赖库
echo - 创建开始菜单和桌面快捷方式
echo - 包含完整的程序文件
echo - 支持卸载功能

pause