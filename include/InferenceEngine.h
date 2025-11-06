#pragma once
#include <QString>
#include <QVector>
#include <QImage>
#include <memory>
#include <vector>

namespace medyolo11qt::ai
{

    /**
     * @brief AI推理结果结构
     */
    struct InferenceResult
    {
        int classId;
        QString className;
        float confidence;
        QRect boundingBox;

        InferenceResult() : classId(-1), confidence(0.0f) {}
        InferenceResult(int id, const QString &name, float conf, const QRect &box)
            : classId(id), className(name), confidence(conf), boundingBox(box) {}
    };

    /**
     * @brief AI推理引擎基类
     * 提供统一的AI推理接口，支持多种模型格式
     */
    class InferenceEngine
    {
    public:
        virtual ~InferenceEngine() = default;

        /**
         * @brief 初始化推理引擎
         * @param modelPath 模型文件路径
         * @param classNamesPath 类别名称文件路径
         * @return 初始化是否成功
         */
        virtual bool initialize(const QString &modelPath, const QString &classNamesPath) = 0;

        /**
         * @brief 执行推理
         * @param image 输入图像
         * @param confidenceThreshold 置信度阈值
         * @return 推理结果列表
         */
        virtual QVector<InferenceResult> infer(const QImage &image, float confidenceThreshold = 0.5f) = 0;

        /**
         * @brief 获取模型信息
         */
        virtual QString getModelInfo() const = 0;

        /**
         * @brief 检查引擎是否已初始化
         */
        virtual bool isInitialized() const = 0;

        /**
         * @brief 获取最后错误信息
         */
        virtual QString getLastError() const = 0;

    protected:
        QString m_modelPath;
        QString m_classNamesPath;
        QString m_lastError;
        bool m_initialized = false;

        /**
         * @brief 加载类别名称
         */
        std::vector<QString> loadClassNames(const QString &filePath);
    };

    /**
     * @brief ONNX Runtime推理引擎实现
     */
    class OnnxInferenceEngine : public InferenceEngine
    {
    public:
        OnnxInferenceEngine();
        ~OnnxInferenceEngine() override;

        bool initialize(const QString &modelPath, const QString &classNamesPath) override;
        QVector<InferenceResult> infer(const QImage &image, float confidenceThreshold = 0.5f) override;
        QString getModelInfo() const override;
        bool isInitialized() const override;
        QString getLastError() const override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;

        /**
         * @brief 预处理图像
         */
        std::vector<float> preprocessImage(const QImage &image, int inputWidth, int inputHeight);

        /**
         * @brief 后处理输出
         */
        QVector<InferenceResult> postprocessOutput(const std::vector<float> &output,
                                                   const std::vector<QString> &classNames,
                                                   float confidenceThreshold);
    };

} // namespace medyolo11qt::ai