# MedYOLO11Qt - 医学图像处理应用

## 项目简介
MedYOLO11Qt是一个基于Qt和YOLOv11的医学图像处理应用，支持X光检测和MRI分割等任务。该应用同时支持Windows和Linux两种开发环境。

## 环境要求

### Windows 环境
- Qt 6.0+（推荐Qt 6.2+）
- CMake 3.18+
- MSVC 2019+ 或 MinGW 8.0+
- GDCM 3.2.0+
- ONNX Runtime 1.10+

### Linux 环境
- Qt 6.0+（推荐通过包管理器安装）
- CMake 3.18+
- GCC 9.0+ 或 Clang 10.0+
- GDCM 3.0+（`sudo apt install libgdcm3-dev`）
- ONNX Runtime 1.10+（可通过包管理器或源码安装）

## 开发配置

### Windows 开发
1. 安装Qt、CMake、MSVC等基础工具
2. 下载并解压GDCM和ONNX Runtime预编译库
3. 在CMakeLists.txt中配置Qt、GDCM和ONNX Runtime的安装路径
4. 使用CMake生成Visual Studio项目并构建

### Linux 开发
1. 通过包管理器安装依赖：
   ```bash
   sudo apt update
   sudo apt install qt6-base-dev libgdcm-dev onnxruntime-dev cmake build-essential
   ```
2. 克隆项目代码
3. 使用CMake构建项目：
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

## 构建指南

### Windows 构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Linux 构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 运行应用

### Windows
直接运行生成的`medapp.exe`文件。

### Linux
可以通过以下方式运行：
1. 直接运行可执行文件：
   ```bash
   ./medapp
   ```
2. 使用提供的运行脚本：
   ```bash
   ./run_medapp.sh
   ```

## 打包

### Windows
项目使用NSIS生成Windows安装包：
```bash
cd build
cmake --build . --config Release --target package
```

### Linux
项目支持生成DEB包和TGZ压缩包：
```bash
cd build
make package
```

## 注意事项
1. 在Linux环境下，依赖库通常通过系统包管理器安装，而在Windows下则需要手动指定依赖路径
2. 模型文件会自动复制到构建目录的models文件夹中
3. 项目支持Debug和Release两种构建模式
4. 如果遇到依赖问题，请参考CMake错误信息安装相应的依赖库

## 任务模式
应用支持两种主要任务模式：
- FAI X光检测
- MRI分割

在应用启动时可以选择任务模式，也可以在运行过程中通过工具栏的"Switch Task"按钮切换任务模式。

## 功能特性
- 支持DICOM和常见图像格式的加载
- 提供X光检测和MRI分割功能
- 批处理功能
- 结果导出功能
- 跨平台支持（Windows和Linux）

## 开发建议
1. 建议使用CMake 3.18或更高版本以获得最佳的Qt支持
2. 在修改CMakeLists.txt时，请注意保持跨平台兼容性
3. 代码中应避免使用平台特定的API，尽可能使用Qt提供的跨平台接口

## 已知问题
1. 在Linux某些发行版上可能需要手动安装特定版本的依赖库
2. 在高DPI显示器上可能需要调整Qt的缩放设置

## 更新日志
- v1.0.0：初始版本，支持基本的X光检测和MRI分割功能，实现跨平台支持