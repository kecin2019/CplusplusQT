#pragma once
#include <QImage>
#include <QString>
#include <QColor>
#include <memory>
#include <vector>

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

    struct Detection
    {
        float x1, y1, x2, y2; // 像素坐标（原图）
        float score;          // 0..1
        int cls;              // 0:CAM, 1:PINCER, 2:MIXED, 3:NORMAL
    };

    struct Result
    {
        QImage outputImage;          // 已绘制框的图
        QString summary;             // 统计摘要
        std::vector<Detection> dets; // 检测框
    };

    InferenceEngine();
    ~InferenceEngine();

    bool loadModel(const QString &weightsPath, Task taskHint);
    Result run(const QImage &input, Task taskHint) const;

    void setThresholds(float conf, float iou)
    {
        m_confThr = conf;
        m_iouThr = iou;
    }

    // 固定 4 类：名字 + 颜色
    static QString className(int cls);
    static QColor classColor(int cls);

private:
    struct OrtPack;
    std::unique_ptr<OrtPack> m_ort;

    int m_inW{640}, m_inH{640};
    float m_confThr{0.25f};
    float m_iouThr{0.45f};

    // 模型输出结构推断
    bool m_isYoloDetect{false};
    int m_channels{0};
    int m_numClasses{0};
    bool m_hasObj{false};

    QString m_modelPath;
};
