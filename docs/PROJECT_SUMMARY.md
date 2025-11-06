# MedYOLO11Qt 项目整理报告

## 项目概述
MedYOLO11Qt 是一个基于 Qt 的医学图像分析软件，集成了 YOLO11 AI 模型进行医学图像检测和分割。项目支持两种模式：X光检测模式和MRI分割模式。

## 项目结构

### 核心架构
```
MedYOLO11Qt/
├── src/                    # 源代码目录
│   ├── main.cpp           # 应用程序入口
│   ├── MainWindow.h/.cpp  # 主窗口界面
│   ├── TaskSelectionDialog.h/.cpp  # 任务选择对话框
│   ├── InferenceEngine.h/.cpp      # AI推理引擎
│   ├── AppConfig.h/.cpp   # 配置管理
│   ├── ErrorHandler.h/.cpp # 错误处理和日志
│   ├── DicomUtils.h/.cpp  # DICOM文件处理
│   ├── ImageView.h/.cpp   # 图像显示组件
│   └── MetaTable.h/.cpp   # 元数据表格组件
├── models/                # AI模型文件
│   ├── fai_xray.onnx     # X光检测模型
│   └── mri_segmentation.onnx  # MRI分割模型
├── test/                  # 测试数据
│   ├── bmp/              # BMP测试图像
│   └── dcm/              # DICOM测试图像
├── build/                 # 构建输出
│   ├── Debug/            # 调试版本
│   └── Release/          # 发布版本
└── tools/                 # 工具程序
    └── encrypt_model.cpp  # 模型加密工具
```

### 依赖库
- **Qt 6.9.2**: GUI框架，安装在 `E:/tools/Qt/6.9.2/msvc2022_64`
- **GDCM 3.2.0**: DICOM文件处理，安装在 `E:/tools/GDCM-3.2.0-Windows-x86_64`
- **ONNXRuntime 1.22.1**: AI模型推理，安装在 `E:/tools/onnxruntime-win-x64-1.22.1`

## 功能特性

### 核心功能
1. **双模式支持**
   - X光检测模式：基于YOLO11的X光图像异常检测
   - MRI分割模式：基于YOLO11的MRI图像分割

2. **图像处理**
   - DICOM格式支持（通过GDCM库）
   - BMP格式支持
   - 图像预处理和后处理
   - 检测结果可视化

3. **AI推理**
   - ONNXRuntime集成
   - 模型加密保护
   - 置信度和IoU阈值可配置
   - GPU加速支持

4. **用户界面**
   - 任务选择对话框
   - 主窗口图像显示
   - 元数据显示表格
   - 多语言支持（中英文）

5. **配置管理**
   - INI格式配置文件
   - 路径管理（Qt、GDCM、ONNXRuntime）
   - 推理参数配置
   - 模型保护密钥
   - 调试模式开关

6. **日志系统**
   - 分级日志（INFO、WARNING、ERROR、CRITICAL）
   - 文件日志输出
   - 日志轮转（最大10MB）
   - 模块化和错误代码支持

## 当前状态

### 构建状态
✅ **编译成功**: 项目已修复所有编译错误
- 修复了AppConfig中QSettings赋值问题（重构为指针类型）
- 修复了InferenceEngine中LOG宏参数问题
- 修复了ErrorHandler中endl未声明问题
- 修复了ONNXRuntime日志级别常量问题

### 文件状态
- **可执行文件**: `build/Release/medapp.exe` 已生成
- **依赖库**: 所有必需的DLL文件已正确复制
- **模型文件**: 已放置在 `build/Release/models/` 目录
- **配置文件**: `config.ini` 已正确配置

### 运行状态
- 程序可以正常启动
- 日志系统工作正常（见 `build/Release/logs/app_2025-10-17.log`）
- 任务选择对话框功能正常

### 测试资源
- **BMP测试图像**: 4张X光图像在 `test/bmp/` 目录
- **DICOM测试图像**: 4张DICOM图像在 `test/dcm/` 目录

## 构建和部署

### 开发构建
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 安装包生成
- 使用CPack自动生成安装包
- 支持NSIS安装程序
- 包含所有运行时依赖
- 自动创建桌面和开始菜单快捷方式

### 发布文件
- 主程序：`medapp.exe`
- Qt运行时：`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`
- ONNXRuntime：`onnxruntime.dll`
- GDCM库：`gdcm*.dll`（共15个）
- Qt平台插件：`platforms/qwindows.dll`
- AI模型：`models/fai_xray.onnx`, `models/mri_segmentation.onnx`

## 技术亮点

1. **模块化设计**: 清晰的类结构和职责分离
2. **单例模式**: AppConfig和ErrorHandler使用单例模式
3. **异常安全**: 完善的错误处理和日志记录
4. **跨平台**: 基于Qt的跨平台GUI框架
5. **AI集成**: 完整的ONNXRuntime集成流程
6. **数据保护**: 模型文件加密保护机制
7. **配置灵活**: 可配置的系统参数和推理参数

## 后续建议

1. **功能测试**: 使用测试图像验证AI推理功能
2. **性能优化**: 评估GPU加速效果和批处理性能
3. **用户体验**: 优化界面响应和进度显示
4. **错误处理**: 增强异常情况的用户提示
5. **文档完善**: 添加用户使用手册和技术文档

项目当前处于可运行状态，具备完整的医学图像AI分析功能框架。