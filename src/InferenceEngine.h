#pragma once
#include <QImage>
#include <QString>
#include <memory>
#include <vector>

#ifndef ORT_DIAG
#define ORT_DIAG 1 // 调试期建议开；发布可关
#endif

class InferenceEngine
{
public:
    enum class Task
    {
        Auto,
        Detect,
        FAI_XRay,
        HipMRI_Seg
    };

    struct Result
    {
        QImage outputImage;
        QString summary;
    };

    InferenceEngine();
    ~InferenceEngine();

    bool loadModel(const QString &weightsPath, Task taskHint);
    Result run(const QImage &input, Task taskHint) const;

    // 阈值（与 Ultralytics 一致的默认）
    void setThresholds(float conf, float iou)
    {
        m_confThr = conf;
        m_iouThr = iou;
    }

    // 直接设置标签或从文件加载
    void setLabels(const std::vector<QString> &labels) { m_labels = labels; }
    bool loadLabelsFromFile(const QString &path); // <--- 新增

private:
    struct OrtPack; // 在 .cpp 中定义（HAVE_ORT 时）
    std::unique_ptr<OrtPack> m_ort;

    // 模型输入分辨率（来自 ONNX 输入张量）
    int m_inW{640};
    int m_inH{640};

    // 阈值
    float m_confThr{0.25f};
    float m_iouThr{0.45f};

    // 模型信息（检测头）
    bool m_isYoloDetect{false};
    int m_channels{0}; // 每条候选的通道数 C
    int m_numClasses{0};
    bool m_hasObj{false}; // YOLOv5 风格是否包含 objectness

    QString m_modelPath;
    std::vector<QString> m_labels; // 类别名
};
