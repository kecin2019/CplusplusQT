# MedYOLO11Qt 快速开始指南

## 🚀 30 秒快速启动

### 方案一：一键运行（推荐）
```batch
# 1. 运行测试（验证环境）
test_release.bat

# 2. 启动程序
build\Release\medapp.exe
```

### 方案二：完整构建
```batch
# 1. 构建项目
build_release.bat

# 2. 运行测试
test_release.bat

# 3. 启动程序
build\Release\medapp.exe
```

---

## 📋 系统要求

### 必需环境
- **操作系统**：Windows 10/11 (64位)
- **运行时**：Visual C++ Redistributable 2022
- **磁盘空间**：2GB 可用空间

### 开发环境（可选）
- **Visual Studio**：2022 社区版或更高
- **CMake**：3.18 或更高版本
- **Qt 6.9.2**：已配置在 `E:/tools/Qt/6.9.2/msvc2022_64`
- **GDCM 3.2.0**：已配置在 `E:/tools/GDCM-3.2.0-Windows-x86_64`
- **ONNXRuntime 1.22.1**：已配置在 `E:/tools/onnxruntime-win-x64-1.22.1`

---

## 🎯 基本操作流程

### 1. 启动程序
- 双击 `build\Release\medapp.exe`
- 或运行 `test_release.bat`（自动启动并检查环境）

### 2. 选择任务模式
- **X光检测模式**：胸部 X 光异常检测
- **MRI分割模式**：MRI 图像器官分割

### 3. 加载图像
- **单张图像**：点击 "Open" 按钮选择图像文件
- **整个文件夹**：点击 "Open Folder" 批量加载
- **支持的格式**：DICOM、BMP、PNG、JPG、TIFF

### 4. 执行分析
- **自动分析**：选择图像后自动开始推理
- **手动触发**：点击 "Run Inference" 按钮
- **查看结果**：在输出面板查看检测/分割结果

### 5. 查看结果
- **图像显示**：主窗口显示原始图像和分析结果
- **元数据**：右侧面板显示 DICOM 标签信息
- **日志信息**：底部状态栏显示操作日志

---

## 📁 文件结构速览

```
build\Release\                  # 主程序目录
├── medapp.exe                   # 主程序
├── encrypt_model.exe            # 模型加密工具
├── config.ini                   # 配置文件
├── models\                     # AI 模型
│   ├── fai_xray.onnx           # X光检测模型
│   └── mri_segmentation.onnx   # MRI分割模型
├── logs\                       # 日志文件
│   └── app_YYYY-MM-DD.log      # 每日日志
└── platforms\                  # Qt平台插件
    └── qwindows.dll            # Windows平台插件

test\                           # 测试数据
├── bmp\                        # X光测试图像（4张）
│   ├── 2222202217正.bmp
│   ├── J0010725正.bmp
│   ├── J0010726正.bmp
│   └── J0010727正.bmp
└── dcm\                        # DICOM测试图像（4张）
    ├── image-00000.dcm
    ├── image-00001.dcm
    ├── image-00002.dcm
    └── image-00003.dcm
```

---

## ⚙️ 配置说明

### 配置文件（config.ini）
```ini
[Paths]
QtPath=E:/tools/Qt/6.9.2/msvc2022_64
GDCMPath=E:/tools/GDCM-3.2.0-Windows-x86_64
ONNXRuntimePath=E:/tools/onnxruntime-win-x64-1.22.1
ModelPath=models

[Inference]
ConfidenceThreshold=0.25
IoUThreshold=0.45

[Security]
ModelProtectionKey=MedYOLO11SecureKey2024

[Logging]
LogLevel=INFO
LogPath=logs
MaxLogSize=10MB

[UI]
Theme=Light
Language=Chinese

[Performance]
UseGPU=false
BatchSize=1
```

### 关键参数说明
- **ConfidenceThreshold**：AI 检测置信度阈值（0.0-1.0）
- **IoUThreshold**：非极大值抑制 IoU 阈值
- **UseGPU**：是否启用 GPU 加速（需要 CUDA 支持）
- **ModelProtectionKey**：模型文件加密密钥

---

## 🔧 常见问题速解

### Q1: 程序无法启动？
**解决方案**：
```batch
# 运行环境检查
test_release.bat

# 手动检查依赖
build\Release\medapp.exe
```

### Q2: 缺少 DLL 错误？
**解决方案**：
- 运行 `test_release.bat` 自动检查
- 检查 Visual C++ Redistributable 2022 是否安装
- 确认所有依赖库路径正确

### Q3: DICOM 文件无法加载？
**解决方案**：
- 确认 GDCM 库正确安装
- 检查 config.ini 中 GDCMPath 设置
- 验证 DICOM 文件格式是否正确

### Q4: AI 推理失败？
**解决方案**：
- 检查模型文件是否存在
- 验证 ONNXRuntime 安装
- 查看日志文件了解详细错误

### Q5: 界面显示异常？
**解决方案**：
- 确认 Qt 运行时库完整
- 检查平台插件 qwindows.dll
- 尝试切换显示主题

---

## 📊 性能优化建议

### 1. GPU 加速（如可用）
```ini
[Performance]
UseGPU=true  # 启用 GPU 加速
BatchSize=4  # 增加批处理大小
```

### 2. 内存优化
```ini
[Performance]
UseGPU=false  # 如显存不足，使用 CPU
BatchSize=1   # 减少批处理大小
```

### 3. 缓存优化
- 程序自动缓存推理结果
- 重复图像无需重新分析
- 缓存大小可配置

---

## 🎯 高级功能

### 1. 批量处理
- **文件夹模式**：支持递归子目录
- **格式过滤**：自动识别支持的图像格式
- **进度显示**：实时显示处理进度

### 2. 模型加密
```batch
# 加密自定义模型
build\Release\encrypt_model.exe input.onnx output.encrypted
```

### 3. 日志分析
```batch
# 查看最新日志
notepad build\Release\logs\app_2025-10-17.log

# 实时监控日志（需要 PowerShell）
Get-Content build\Release\logs\app_2025-10-17.log -Wait
```

---

## 📞 技术支持

### 自助排错
1. **运行测试脚本**：`test_release.bat`
2. **检查日志文件**：查看 `logs\` 目录
3. **验证配置文件**：检查 `config.ini` 设置
4. **确认依赖完整**：运行环境检查

### 问题报告
如遇到无法解决的问题，请提供以下信息：
- 操作系统版本和位数
- 错误现象详细描述
- 相关日志文件内容
- 配置文件内容

---

## 🏆 下一步操作

### 立即体验
1. **运行测试**：`test_release.bat`
2. **启动程序**：`build\Release\medapp.exe`
3. **加载测试图像**：使用 `test\` 目录下的样本
4. **体验 AI 分析**：选择不同模式进行测试

### 功能验证
- ✅ **X光检测**：使用 `test\bmp\` 目录图像
- ✅ **MRI分割**：使用 `test\dcm\` 目录图像
- ✅ **批量处理**：测试文件夹加载功能
- ✅ **结果查看**：验证分析结果展示

### 性能评估
- 🚀 **启动速度**：程序启动时间
- ⚡ **推理速度**：单张图像分析时间
- 💾 **内存使用**：运行时的内存占用
- 🔄 **批量效率**：大批量处理性能

---

**🎉 恭喜！您现在可以开始使用 MedYOLO11Qt 进行专业的医学影像 AI 分析了！**

**预计首次体验时间：5-10 分钟**