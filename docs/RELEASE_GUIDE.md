# MedYOLO11Qt 发行版本构建指南

## 准备工作

### 1. 安装必要的工具
- **CMake**: 版本 3.18 或更高
- **Visual Studio**: 2022 或更高版本（包含 C++ 开发工具）
- **NSIS (Nullsoft Install System)**: 用于创建安装程序

### 2. 依赖库路径确认
确保以下依赖库路径正确配置在 `CMakeLists.txt` 中：
- Qt 6.9.2: `E:\tools\Qt\6.9.2\msvc2022_64`
- GDCM 3.2.0: `E:\tools\GDCM-3.2.0-Windows-x86_64`
- ONNXRuntime 1.22.1: `E:\tools\onnxruntime-win-x64-1.22.1`

## 构建发行版本

### 方法一：使用批处理脚本（推荐）
1. 双击运行 `build_release.bat`
2. 脚本将自动：
   - 清理 build 目录
   - 配置 CMake (Release 模式)
   - 编译项目
   - 复制必要的依赖文件

### 方法二：手动构建
```bash
# 清理并创建构建目录
rmdir /s /q build
mkdir build
cd build

# 配置 CMake (Release 模式)
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译项目
cmake --build . --config Release

# 检查生成的文件
cd Release
dir *.exe *.dll
```

## 创建安装包

### 方法一：使用 CPack（推荐）
1. **安装 NSIS**: 
   - 下载地址: https://nsis.sourceforge.io/Download
   - 安装时选择"Add NSIS to Path"选项
   - 或者手动将NSIS安装目录（如 `C:\Program Files (x86)\NSIS`）添加到系统PATH
2. 运行 `package_release.bat`
3. 脚本将自动使用 CPack 生成安装包
4. 生成的安装包: `MedYOLO11Qt-1.0.0-win64.exe`

### 方法二：手动编译 NSIS 脚本
1. 安装 NSIS: https://nsis.sourceforge.io/Download
2. 确保 `makensis.exe` 在系统 PATH 中
3. 编译安装脚本：
   ```bash
   makensis installer.nsi
   ```
4. 生成的安装文件: `MedYOLO11Qt_Setup.exe`

### 安装包信息
- 文件大小: 约 200-300 MB（包含所有依赖）
- 包含完整的运行时依赖
- 自动安装 Qt 平台插件
- 包含 AI 模型文件

## 安装程序功能

### 包含的文件
- 主程序: `medapp.exe`
- Qt 运行时库: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`
- ONNXRuntime: `onnxruntime.dll`
- GDCM 库: 所有 `gdcm*.dll` 文件
- Qt 平台插件: `platforms/qwindows.dll`
- AI 模型: `models/fai_xray.onnx`

### 安装选项
- **主程序** (必需): 包含所有运行时依赖
- **示例文件** (可选): 测试用的 DICOM 和 BMP 文件

### 安装位置
- 默认: `C:\Program Files\MedYOLO11Qt`
- 用户可选择自定义安装路径

### 快捷方式
- 开始菜单: `MedYOLO11Qt` 程序组
- 桌面: `MedYOLO11Qt` 快捷方式

## 测试安装包

1. 在一台干净的 Windows 机器上测试
2. 确保没有安装 Qt、GDCM、ONNXRuntime 等依赖
3. 运行安装程序并完成安装
4. 启动程序验证功能正常

## 故障排除

### 常见问题
1. **缺少 DLL 错误**: 确保所有依赖 DLL 都已正确复制
2. **模型文件找不到**: 检查 `models` 目录是否存在且包含 `fai_xray.onnx`
3. **Qt 插件错误**: 确保 `platforms` 目录包含 `qwindows.dll`

### 调试建议
- 在构建完成后检查 `build\Release` 目录内容
- 使用 Dependency Walker 检查缺失的依赖
- 查看程序日志输出

## 版本发布

### 版本号管理
- 在 `CMakeLists.txt` 中更新项目版本号
- 在 `installer.nsi` 中更新显示版本

### 分发准备
- 测试安装包在多种 Windows 版本上的兼容性
- 准备用户手册和文档
- 创建数字签名（可选）

## 技术支持

如有问题，请检查：
1. 所有依赖库路径是否正确
2. 构建日志中是否有错误信息
3. 安装程序编译是否成功