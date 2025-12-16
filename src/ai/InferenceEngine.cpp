#include "InferenceEngine.h"
#include <QPainter>
#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <cfloat>
#include <climits>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QVector>
#include "core/AppConfig.h"
#include "core/ErrorHandler.h"

#ifdef HAVE_ORT
#include <onnxruntime_cxx_api.h>
#endif

namespace
{

    inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

    struct Box
    {
        float x1, y1, x2, y2, score;
        int cls;
        std::vector<float> maskCoeffs;
        double maskAreaPixels{0.0};
        bool hasMask{false};
    };

    inline float IoU(const Box &a, const Box &b)
    {
        const float xx1 = std::max(a.x1, b.x1);
        const float yy1 = std::max(a.y1, b.y1);
        const float xx2 = std::min(a.x2, b.x2);
        const float yy2 = std::min(a.y2, b.y2);
        const float w = std::max(0.f, xx2 - xx1), h = std::max(0.f, yy2 - yy1);
        const float inter = w * h;
        const float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
        const float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);
        return inter / std::max(1e-6f, areaA + areaB - inter);
    }

    // 类内 NMS（与 Ultralytics 同思路：给不同类别一个大偏移）
    static std::vector<Box> nms_classwise(std::vector<Box> dets, float iouThr)
    {
        const float max_wh = 4096.f;
        for (auto &d : dets)
        {
            float off = d.cls * max_wh;
            d.x1 += off;
            d.x2 += off;
        }
        std::sort(dets.begin(), dets.end(), [](auto &a, auto &b)
                  { return a.score > b.score; });
        std::vector<Box> keep;
        keep.reserve(dets.size());
        for (size_t i = 0; i < dets.size(); ++i)
        {
            bool drop = false;
            for (const auto &k : keep)
                if (IoU(dets[i], k) > iouThr)
                {
                    drop = true;
                    break;
                }
            if (!drop)
                keep.push_back(dets[i]);
            if (keep.size() >= 300)
                break;
        }
        for (auto &d : keep)
        {
            float off = d.cls * max_wh;
            d.x1 -= off;
            d.x2 -= off;
        }
        return keep;
    }

    // 画框：加粗 + 文字底色
    static void drawBox(QImage &img, const QRectF &r, const QString &text, const QColor &col)
    {
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
        p.setPen(QPen(col, 3.5));
        p.drawRect(r);

        QFont f("Sans", 12, QFont::Bold);
        p.setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(text) + 10, th = fm.height() + 6;
        QRect tr((int)r.x(), std::max(0, (int)r.y() - th), tw, th);
        p.fillRect(tr, QColor(0, 0, 0, 170));
        p.setPen(Qt::yellow);
        p.drawText(tr.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }

    // Ultralytics letterbox（pad=114）
    static QImage letterbox(const QImage &src, int newW, int newH, float &r, int &dw, int &dh,
                            bool scaleup = true, int /*stride*/ = 32, int padv = 114)
    {
        // 计算缩放比例
        float gain = std::min((float)newH / src.height(), (float)newW / src.width());
        if (!scaleup)
            gain = std::min(gain, 1.0f);
        const int unpadW = static_cast<int>(std::round(src.width() * gain));
        const int unpadH = static_cast<int>(std::round(src.height() * gain));
        r = gain;

        dw = (newW - unpadW) / 2;
        dh = (newH - unpadH) / 2;

        QImage res(newW, newH, src.format());
        res.fill(padv);
        QPainter p(&res);
        p.drawImage(dw, dh, src.scaled(unpadW, unpadH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        p.end();

        // 确保 dw, dh 是 stride 的倍数
        if (dw % 2 != 0)
            dw += 1; // 使其为偶数，以便后续处理
        if (dh % 2 != 0)
            dh += 1;

        return res;
    }

    // 缩放框回到原始图像尺寸
    static void scale_boxes_back(std::vector<Box> &boxes, float r, int dw, int dh, int imgW, int imgH)
    {
        for (auto &b : boxes)
        {
            b.x1 = (b.x1 - dw) / r;
            b.y1 = (b.y1 - dh) / r;
            b.x2 = (b.x2 - dw) / r;
            b.y2 = (b.y2 - dh) / r;
            // 限制在图像范围内
            b.x1 = std::max(0.f, std::min((float)imgW - 1, b.x1));
            b.y1 = std::max(0.f, std::min((float)imgH - 1, b.y1));
            b.x2 = std::max(0.f, std::min((float)imgW - 1, b.x2));
            b.y2 = std::max(0.f, std::min((float)imgH - 1, b.y2));
        }
    }

    // XOR解密（简单保护）
    static QByteArray xorDecrypt(const QByteArray &data, const QByteArray &key)
    {
        if (key.isEmpty())
        {
            LOG_WARNING("XOR解密密钥为空", "Inference", 5004);
            return data; // 密钥为空时返回原始数据
        }

        QByteArray result = data;
        const int keyLen = key.size();
        for (int i = 0; i < data.size(); ++i)
            result[i] = data[i] ^ key[i % keyLen];
        return result;
    }
    constexpr quint32 kModelMagic = 0x4D594F4C; // "MYOL"
    constexpr int kModelHeaderSize = 8;

    struct DetectionPreprocessResult
    {
        std::vector<float> tensor;
        float scale{1.f};
        int padW{0};
        int padH{0};
        int width{0};
        int height{0};
    };

    static std::vector<float> imageToCHW(const QImage &src)
    {
        QImage img = (src.format() == QImage::Format_RGB888) ? src : src.convertToFormat(QImage::Format_RGB888);
        const int h = img.height();
        const int w = img.width();
        const int plane = h * w;
        std::vector<float> tensor(3 * plane);
        for (int y = 0; y < h; ++y)
        {
            const uchar *line = img.constScanLine(y);
            for (int x = 0; x < w; ++x)
            {
                const int idx = y * w + x;
                tensor[0 * plane + idx] = line[3 * x + 0] / 255.f;
                tensor[1 * plane + idx] = line[3 * x + 1] / 255.f;
                tensor[2 * plane + idx] = line[3 * x + 2] / 255.f;
            }
        }
        return tensor;
    }

    static DetectionPreprocessResult preprocessDetectionInput(const QImage &image, int targetW, int targetH)
    {
        DetectionPreprocessResult result;
        result.width = targetW;
        result.height = targetH;

        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        float ratio = 1.f;
        int dw = 0;
        int dh = 0;
        QImage boxed = letterbox(rgb, targetW, targetH, ratio, dw, dh, true, 32, 114);

        result.scale = ratio;
        result.padW = dw;
        result.padH = dh;
        result.tensor = imageToCHW(boxed);
        return result;
    }

    static std::vector<float> preprocessSegmentationInput(const QImage &image, int targetW, int targetH)
    {
        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        QImage resized = rgb.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return imageToCHW(resized);
    }

#ifdef HAVE_ORT
    static Ort::Value createInputTensor(std::vector<float> &tensor, int height, int width)
    {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 4> shape{1, 3, height, width};
        return Ort::Value::CreateTensor<float>(mem, tensor.data(), tensor.size(), shape.data(), shape.size());
    }

    static bool runSingleOutput(Ort::Session &session,
                                const std::vector<const char *> &inputNames,
                                Ort::Value &input,
                                const char *outputName,
                                Ort::Value &output,
                                QString *errorMessage = nullptr)
    {
        try
        {
            auto outs = session.Run(Ort::RunOptions{nullptr},
                                    inputNames.data(), &input, 1,
                                    &outputName, 1);
            if (outs.empty())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("ONNX Runtime 没有返回任何输出");
                return false;
            }
            if (!outs[0].IsTensor())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("ONNX Runtime 输出不是张量");
                return false;
            }

            output = std::move(outs[0]);
            return true;
        }
        catch (const Ort::Exception &e)
        {
            if (errorMessage)
                *errorMessage = QString::fromUtf8(e.what());
            return false;
        }
    }
#endif

    class EncryptedModelLoader
    {
    public:
        struct LoadInfo
        {
            QByteArray data;
            quint32 version{0};
        };

        // 修改 EncryptedModelLoader::load 方法，增加更详细的错误信息
        static std::optional<LoadInfo> load(const QString &path, const QByteArray &key, QString *errorMessage = nullptr)
        {
            if (path.isEmpty())
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("模型路径为空");
                return std::nullopt;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("无法打开模型文件: %1，错误: %2").arg(path).arg(file.errorString());
                return std::nullopt;
            }

            QByteArray fileData = file.readAll();
            file.close();

            if (fileData.size() < kModelHeaderSize)
            {
                // 增加对未加密文件的检测
                QFileInfo info(path);
                if (info.suffix().toLower() == "onnx")
                {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("检测到未加密的ONNX文件，请先使用encrypt_model工具加密: %1").arg(path);
                }
                else
                {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("模型文件过小或损坏: %1，大小: %2字节").arg(path).arg(fileData.size());
                }
                return std::nullopt;
            }

            QDataStream headerStream(fileData);
            quint32 magic = 0;
            quint32 version = 0;
            headerStream >> magic >> version;

            if (magic != kModelMagic)
            {
                if (errorMessage)
                    *errorMessage = QStringLiteral("模型文件非法或未加密: %1").arg(path);
                return std::nullopt;
            }

            // 添加更多调试信息
            if (AppConfig::instance().isDebugModeEnabled())
            {
                qDebug() << "模型版本:" << version;
                qDebug() << "加密数据大小:" << fileData.size() - kModelHeaderSize;
            }

            QByteArray encryptedPayload = fileData.mid(kModelHeaderSize);
            LoadInfo info;
            info.version = version;
            info.data = xorDecrypt(encryptedPayload, key);
            return info;
        }
    };
}

#ifdef HAVE_ORT
struct InferenceEngine::OrtPack
{
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "MedYOLO11Qt"};
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;
    std::vector<Ort::AllocatedStringPtr> inPtrs;
    std::vector<Ort::AllocatedStringPtr> outPtrs;
    std::vector<const char *> inputNames;
    std::vector<const char *> outputNames;
};
#endif

InferenceEngine::InferenceEngine() = default;
InferenceEngine::~InferenceEngine() = default;

void InferenceEngine::unload()
{
#ifdef HAVE_ORT
    m_ort.reset();
#endif
    m_modelPath.clear();
    m_isYoloDetect = false;
    m_hasObj = false;
    m_numClasses = 0;
    m_channels = 0;
    m_detOutputIndex = -1;
    m_segOutputIndex = -1;
    m_maskChannels = 0;
    m_maskWidth = 0;
    m_maskHeight = 0;
}

bool InferenceEngine::isLoaded() const
{
#ifdef HAVE_ORT
    return m_ort && m_ort->session;
#else
    return false;
#endif
}

bool InferenceEngine::loadModel(const QString &path)
{
#ifndef HAVE_ORT
    Q_UNUSED(path);
    LOG_WARNING(QStringLiteral("编译时未启用 ONNXRuntime，无法加载模型"), "Inference", 5005);
    return false;
#else
    try
    {
        if (path.isEmpty())
        {
            LOG_WARNING(QStringLiteral("模型路径为空"), "Inference", 5006);
            return false;
        }

        if (m_ort && m_ort->session && !m_modelPath.isEmpty())
        {
            if (QFileInfo(m_modelPath) == QFileInfo(path))
            {
                LOG_INFO(QStringLiteral("模型已加载且路径未变化，跳过重复初始化"), "Inference");
                return true;
            }
        }
        unload();

        AppConfig &config = AppConfig::instance();
        QString keyStr = config.getModelProtectionKey();
        if (keyStr.isEmpty())
        {
            keyStr = QStringLiteral("MedYOLO11Qt_Model_Protection_Key_2024");
            LOG_WARNING(QStringLiteral("配置中未找到模型保护密钥，使用默认密钥"), "Inference", 5010);
        }

        const QByteArray key = QCryptographicHash::hash(keyStr.toUtf8(), QCryptographicHash::Sha256);

        QString loadError;
        auto loadInfo = EncryptedModelLoader::load(path, key, &loadError);
        if (!loadInfo.has_value())
        {
            const QString msg = loadError.isEmpty()
                                    ? QStringLiteral("模型加载失败: %1").arg(path)
                                    : loadError;
            LOG_ERROR(msg, "Inference", 5007);
            return false;
        }

        const QByteArray modelData = loadInfo->data;

        m_ort = std::make_unique<OrtPack>();
        m_ort->opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const OrtLoggingLevel logLevel = config.isDebugModeEnabled() ? ORT_LOGGING_LEVEL_INFO : ORT_LOGGING_LEVEL_WARNING;
        m_ort->env = Ort::Env(logLevel, "MedYOLO11Qt");

        m_ort->session = std::make_unique<Ort::Session>(
            m_ort->env,
            modelData.constData(),
            static_cast<size_t>(modelData.size()),
            m_ort->opts);

        Ort::AllocatorWithDefaultOptions alloc;
        const size_t ni = m_ort->session->GetInputCount();
        const size_t no = m_ort->session->GetOutputCount();
        m_ort->inPtrs.clear();
        m_ort->outPtrs.clear();
        m_ort->inputNames.clear();
        m_ort->outputNames.clear();
        m_ort->inPtrs.reserve(ni);
        m_ort->outPtrs.reserve(no);
        m_ort->inputNames.reserve(ni);
        m_ort->outputNames.reserve(no);
        for (size_t i = 0; i < ni; ++i)
        {
            m_ort->inPtrs.emplace_back(m_ort->session->GetInputNameAllocated(i, alloc));
            m_ort->inputNames.push_back(m_ort->inPtrs.back().get());
        }
        for (size_t i = 0; i < no; ++i)
        {
            m_ort->outPtrs.emplace_back(m_ort->session->GetOutputNameAllocated(i, alloc));
            m_ort->outputNames.push_back(m_ort->outPtrs.back().get());
        }

        try
        {
            auto ti = m_ort->session->GetInputTypeInfo(0);
            if (ti.GetONNXType() == ONNX_TYPE_TENSOR)
            {
                auto inf = ti.GetTensorTypeAndShapeInfo();
                auto sh = inf.GetShape();
                if (sh.size() >= 4 && std::abs(static_cast<int>(sh[2])) > 0 && std::abs(static_cast<int>(sh[3])) > 0)
                {
                    m_inH = std::abs(static_cast<int>(sh[2]));
                    m_inW = std::abs(static_cast<int>(sh[3]));
                }
            }
        }
        catch (...)
        {
            m_inH = m_inW = 640;
        }

        m_isYoloDetect = false;
        m_hasObj = false;
        m_numClasses = 0;
        m_channels = 0;
        m_detOutputIndex = -1;
        m_segOutputIndex = -1;
        m_maskChannels = 0;
        m_maskWidth = 0;
        m_maskHeight = 0;

        int expectedMaskChannels = 0;
        for (size_t i = 0; i < no; ++i)
        {
            auto to = m_ort->session->GetOutputTypeInfo(i);
            if (to.GetONNXType() != ONNX_TYPE_TENSOR)
                continue;
            auto info = to.GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                continue;
            auto sh = info.GetShape();
            if (sh.size() == 3)
            {
                const int d1 = std::abs(static_cast<int>(sh[1]));
                const int d2 = std::abs(static_cast<int>(sh[2]));
                const int C = std::min(d1, d2);
                const int N = std::max(d1, d2);
                if (C >= 6 && N >= 10)
                {
                    m_isYoloDetect = true;
                    const int nc_v8 = C - 4;
                    const int nc_v5 = C - 5;
                    if (nc_v8 >= 1)
                    {
                        m_hasObj = false;
                        m_numClasses = nc_v8;
                    }
                    else
                    {
                        m_hasObj = true;
                        m_numClasses = std::max(1, nc_v5);
                    }
                    m_channels = C;
                    m_detOutputIndex = static_cast<int>(i);
                    const int clsOffset = m_hasObj ? 5 : 4;
                    int maskCandidate = m_channels - (clsOffset + m_numClasses);
                    if (maskCandidate > 0)
                        expectedMaskChannels = maskCandidate;
                    break;
                }
            }
        }

        for (size_t i = 0; i < no; ++i)
        {
            auto to = m_ort->session->GetOutputTypeInfo(i);
            if (to.GetONNXType() != ONNX_TYPE_TENSOR)
                continue;
            auto info = to.GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                continue;
            auto sh = info.GetShape();
            if (sh.size() != 4)
                continue;
            const int protoBatch = std::abs(static_cast<int>(sh[0]));
            const int protoC = std::abs(static_cast<int>(sh[1]));
            const int protoH = std::abs(static_cast<int>(sh[2]));
            const int protoW = std::abs(static_cast<int>(sh[3]));
            if (protoBatch != 1 || protoC <= 0 || protoH <= 0 || protoW <= 0)
                continue;
            if (expectedMaskChannels > 0 && protoC != expectedMaskChannels)
                continue;
            m_segOutputIndex = static_cast<int>(i);
            m_maskChannels = protoC;
            m_maskHeight = protoH;
            m_maskWidth = protoW;
            break;
        }

        m_modelPath = path;
        LOG_INFO(QStringLiteral("成功加载模型: %1, 输入尺寸: %2x%3").arg(path).arg(m_inW).arg(m_inH), "Inference");
        return true;
    }
    catch (const Ort::Exception &e)
    {
        LOG_ERROR(QStringLiteral("ONNX Runtime 异常: %1").arg(QString::fromUtf8(e.what())), "Inference", 5012);
        m_ort.reset();
        return false;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(QStringLiteral("模型加载异常: %1").arg(QString::fromUtf8(e.what())), "Inference", 5013);
        m_ort.reset();
        return false;
    }
    catch (...)
    {
        LOG_ERROR(QStringLiteral("模型加载发生未知异常"), "Inference", 5014);
        m_ort.reset();
        return false;
    }
#endif
}

bool InferenceEngine::isSegmentationModel() const
{
#ifndef HAVE_ORT
    return false;
#else
    try
    {
        if (!m_ort || !m_ort->session)
            return false;

        // 检查输出形状来判断是否为分割模型
        // 分割模型通常有 [1, num_classes, H, W] 形状的输出
        const size_t no = m_ort->session->GetOutputCount();
        for (size_t i = 0; i < no; ++i)
        {
            auto to = m_ort->session->GetOutputTypeInfo(i);
            if (to.GetONNXType() != ONNX_TYPE_TENSOR)
                continue;
            auto inf = to.GetTensorTypeAndShapeInfo();
            if (inf.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                continue;
            auto sh = inf.GetShape();
            if (sh.size() == 4 && sh[0] == 1 && sh[1] > 1) // [1, C, H, W]
            {
                return true;
            }
        }
        return false;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(QString("检查模型类型异常: %1").arg(e.what()), "Inference", 5015);
        return false;
    }
    catch (...)
    {
        LOG_ERROR("检查模型类型发生未知异常", "Inference", 5016);
        return false;
    }
#endif
}

InferenceEngine::Result InferenceEngine::run(const QImage &input, Task taskHint) const
{
    const bool segmentationRequested = (taskHint == Task::HipMRI_Seg);
#ifndef HAVE_ORT
    Result R;
    R.outputImage = input;
    R.summary = "Built without ONNXRuntime";
    return R;
#else
    return runYolo(input, segmentationRequested && hasSegmentationSupport());
#endif
}

QString InferenceEngine::className(int cls)
{
    static const QStringList names = {"CAM", "PINCER", "MIXED", "NORMAL"};
    return (cls >= 0 && cls < names.size()) ? names[cls] : "UNKNOWN";
}

QColor InferenceEngine::classColor(int cls)
{
    static const QColor colors[] = {QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255), QColor(128, 128, 128)};
    return (cls >= 0 && cls < 4) ? colors[cls] : QColor(128, 128, 128);
}

QString InferenceEngine::segmentationClassName(int cls)
{
    const QStringList names = AppConfig::instance().getMriClassNames();
    if (cls >= 0 && cls < names.size())
        return names[cls];
    return QStringLiteral("Muscle %1").arg(cls + 1);
}

QColor InferenceEngine::segmentationClassColor(int cls)
{
    static const QVector<QColor> palette = {
        QColor(252, 90, 141),
        QColor(88, 205, 237),
        QColor(130, 202, 157),
        QColor(255, 196, 87),
        QColor(181, 148, 226),
        QColor(255, 147, 74),
        QColor(120, 144, 156),
        QColor(255, 99, 71)};
    if (cls >= 0 && cls < palette.size())
        return palette[cls];
    const int hue = (cls * 57) % 360;
    return QColor::fromHsv(hue, 180, 240);
}
#ifdef HAVE_ORT
InferenceEngine::Result InferenceEngine::runYolo(const QImage &input, bool segmentationMode) const
{
    Result R;
    QImage vis = input.convertToFormat(QImage::Format_ARGB32);

    try
    {
        if (!m_ort || !m_ort->session)
        {
            R.outputImage = vis;
            R.summary = "Model not loaded";
            LOG_WARNING("推理请求但模型未加载", "Inference", 5022);
            return R;
        }

        const int netW = m_inW, netH = m_inH;
        DetectionPreprocessResult prep = preprocessDetectionInput(input, netW, netH);
        Ort::Value in = createInputTensor(prep.tensor, prep.height, prep.width);

        const size_t no = m_ort->session->GetOutputCount();
        int detIndex = (m_detOutputIndex >= 0 && m_detOutputIndex < (int)no) ? m_detOutputIndex : -1;
        auto isValidDetOutput = [&](int idx) -> bool {
            if (idx < 0 || idx >= (int)no)
                return false;
            auto to = m_ort->session->GetOutputTypeInfo((size_t)idx);
            if (to.GetONNXType() != ONNX_TYPE_TENSOR)
                return false;
            auto info = to.GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                return false;
            auto sh = info.GetShape();
            return sh.size() == 3;
        };
        if (!isValidDetOutput(detIndex))
            detIndex = -1;
        if (detIndex < 0)
        {
            for (size_t i = 0; i < no; ++i)
            {
                if (isValidDetOutput((int)i))
                {
                    detIndex = (int)i;
                    break;
                }
            }
        }
        if (detIndex < 0)
        {
            R.outputImage = vis;
            R.summary = "Detection output missing";
            LOG_WARNING("模型输出无效: 未找到检测张量", "Inference", 5001);
            return R;
        }

        bool wantSegmentation = segmentationMode && hasSegmentationSupport();
        std::vector<int> requestIndices;
        requestIndices.push_back(detIndex);
        if (wantSegmentation && m_segOutputIndex >= 0 &&
            m_segOutputIndex < static_cast<int>(no))
            requestIndices.push_back(m_segOutputIndex);

        std::vector<const char *> requestNames;
        requestNames.reserve(requestIndices.size());
        for (int idx : requestIndices)
            requestNames.push_back(m_ort->outputNames[static_cast<size_t>(idx)]);

        auto outputs = m_ort->session->Run(Ort::RunOptions{nullptr},
                                           m_ort->inputNames.data(), &in, 1,
                                           requestNames.data(), requestNames.size());
        if (outputs.empty() || !outputs[0].IsTensor())
        {
            R.outputImage = vis;
            R.summary = "Invalid output";
            LOG_WARNING("模型输出无效", "Inference", 5003);
            return R;
        }

        const float *data = outputs[0].GetTensorData<float>();
        auto info = outputs[0].GetTensorTypeAndShapeInfo();
        auto sh = info.GetShape();
        const int d1 = std::abs((int)sh[1]);
        const int d2 = std::abs((int)sh[2]);
        const int attrCount = std::min(d1, d2);
        const int detCount = std::max(d1, d2);
        const bool attrLast = (d2 == attrCount);

        int nc = (m_numClasses > 0 ? m_numClasses : std::max(1, std::max(attrCount - 4, attrCount - 5)));
        bool hasObj = (m_numClasses > 0 ? m_hasObj : (attrCount - 5 == nc));

        auto getAttr = [&](int detIdx, int attrIdx) -> float
        {
            return attrLast ? data[detIdx * attrCount + attrIdx]
                            : data[attrIdx * detCount + detIdx];
        };

        const int clsOffset = hasObj ? 5 : 4;

        bool segReady = wantSegmentation && outputs.size() > 1 && outputs[1].IsTensor() && m_maskChannels > 0;
        int maskOffset = -1;
        int maskChannels = segReady ? m_maskChannels : 0;
        if (segReady)
        {
            maskOffset = clsOffset + nc;
            if (maskOffset + maskChannels > attrCount)
                segReady = false;
        }

        std::vector<Box> candidates;
        candidates.reserve(std::min(detCount, 3000));
        for (int idx = 0; idx < detCount; ++idx)
        {
            float cx = getAttr(idx, 0);
            float cy = getAttr(idx, 1);
            float bw = getAttr(idx, 2);
            float bh = getAttr(idx, 3);

            const float maxAbs = std::max({std::fabs(cx), std::fabs(cy), std::fabs(bw), std::fabs(bh)});
            const bool normalized = (maxAbs <= 1.5f);
            if (normalized)
            {
                cx *= netW;
                cy *= netH;
                bw *= netW;
                bh *= netH;
            }

            float obj = hasObj ? sigmoid(getAttr(idx, 4)) : 1.f;

            int bestClass = -1;
            float bestScore = 0.f;
            for (int k = 0; k < nc; ++k)
            {
                float clsProb = sigmoid(getAttr(idx, clsOffset + k));
                float score = hasObj ? (obj * clsProb) : clsProb;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestClass = k;
                }
            }
            if (bestClass < 0 || bestScore < m_confThr)
                continue;

            float x1 = cx - bw * 0.5f;
            float y1 = cy - bh * 0.5f;
            float x2 = cx + bw * 0.5f;
            float y2 = cy + bh * 0.5f;
            x1 = std::clamp(x1, 0.f, (float)netW - 1.f);
            y1 = std::clamp(y1, 0.f, (float)netH - 1.f);
            x2 = std::clamp(x2, 0.f, (float)netW - 1.f);
            y2 = std::clamp(y2, 0.f, (float)netH - 1.f);

            Box box;
            box.x1 = x1;
            box.y1 = y1;
            box.x2 = x2;
            box.y2 = y2;
            box.score = bestScore;
            box.cls = segmentationMode ? bestClass : std::clamp(bestClass, 0, 3);

            if (segReady && maskOffset >= 0)
            {
                box.maskCoeffs.resize(static_cast<size_t>(maskChannels));
                for (int m = 0; m < maskChannels; ++m)
                    box.maskCoeffs[(size_t)m] = getAttr(idx, maskOffset + m);
            }

            candidates.push_back(std::move(box));
        }

        auto kept = nms_classwise(std::move(candidates), m_iouThr);
        scale_boxes_back(kept, prep.scale, prep.padW, prep.padH, input.width(), input.height());

        QImage overlay;
        if (segReady && !kept.empty())
        {
            const Ort::Value &protoTensor = outputs.back();
            auto protoInfo = protoTensor.GetTensorTypeAndShapeInfo();
            auto protoShape = protoInfo.GetShape();
            if (protoShape.size() == 4)
            {
                const int protoBatch = std::abs((int)protoShape[0]);
                const int protoC = std::abs((int)protoShape[1]);
                const int protoH = std::abs((int)protoShape[2]);
                const int protoW = std::abs((int)protoShape[3]);
                if (protoBatch == 1 && protoC == maskChannels && protoH > 0 && protoW > 0)
                {
                    const float *protoData = protoTensor.GetTensorData<float>();
                    const size_t protoPlane = (size_t)protoH * (size_t)protoW;
                    std::vector<float> buffer(protoPlane);
                    QImage maskComposite(input.size(), QImage::Format_ARGB32_Premultiplied);
                    maskComposite.fill(Qt::transparent);
                    const QRect canvas(0, 0, input.width(), input.height());
                    const float maskThreshold = 0.45f;

                    for (auto &box : kept)
                    {
                        if (box.maskCoeffs.size() != (size_t)maskChannels)
                            continue;

                        std::fill(buffer.begin(), buffer.end(), 0.f);
                        for (int c = 0; c < maskChannels; ++c)
                        {
                            const float coeff = box.maskCoeffs[(size_t)c];
                            const float *plane = protoData + (size_t)c * protoPlane;
                            for (size_t idx = 0; idx < protoPlane; ++idx)
                                buffer[idx] += coeff * plane[idx];
                        }
                        for (float &v : buffer)
                            v = sigmoid(v);

                        QImage mask(protoW, protoH, QImage::Format_Grayscale8);
                        for (int y = 0; y < protoH; ++y)
                        {
                            uchar *row = mask.scanLine(y);
                            for (int x = 0; x < protoW; ++x)
                            {
                                float val = buffer[(size_t)y * protoW + x];
                                row[x] = static_cast<uchar>(std::clamp<int>(std::lround(val * 255.f), 0, 255));
                            }
                        }

                        QImage resized = mask.scaled(prep.width, prep.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                        int cropX = std::clamp(prep.padW, 0, resized.width());
                        int cropY = std::clamp(prep.padH, 0, resized.height());
                        int cropW = std::clamp((int)std::round(input.width() * prep.scale), 0, resized.width() - cropX);
                        int cropH = std::clamp((int)std::round(input.height() * prep.scale), 0, resized.height() - cropY);
                        if (cropW <= 0 || cropH <= 0)
                            continue;

                        QImage cropped = resized.copy(cropX, cropY, cropW, cropH);
                        QImage restored = cropped.scaled(input.width(), input.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

                        QRect roi(std::max(0, (int)std::floor(box.x1)),
                                  std::max(0, (int)std::floor(box.y1)),
                                  std::max(1, (int)std::ceil(box.x2 - box.x1)),
                                  std::max(1, (int)std::ceil(box.y2 - box.y1)));
                        roi = roi.intersected(canvas);
                        if (roi.isEmpty())
                            continue;

                        double covered = 0.0;
                        for (int y = roi.top(); y <= roi.bottom(); ++y)
                        {
                            const uchar *maskRow = restored.constScanLine(y);
                            QRgb *overlayRow = reinterpret_cast<QRgb *>(maskComposite.scanLine(y));
                            for (int x = roi.left(); x <= roi.right(); ++x)
                            {
                                float alpha = maskRow[x] / 255.f;
                                if (alpha < maskThreshold)
                                    continue;
                                covered += 1.0;
                                int a = std::clamp<int>(std::lround(alpha * 200.f), 0, 255);
                                QColor color = segmentationClassColor(box.cls);
                                QRgb current = overlayRow[x];
                                if (qAlpha(current) < a)
                                    overlayRow[x] = qRgba(color.red(), color.green(), color.blue(), a);
                            }
                        }

                        box.maskAreaPixels = covered;
                        box.hasMask = covered > 0.0;
                    }

                    overlay = maskComposite.convertToFormat(QImage::Format_ARGB32);
                    if (!overlay.isNull())
                    {
                        QPainter painter(&vis);
                        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                        painter.drawImage(QPoint(), overlay);
                    }
                }
            }
        }

        for (const auto &box : kept)
        {
            QRectF rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            QString label = segmentationMode ? segmentationClassName(box.cls) : className(box.cls);
            QString text = segmentationMode
                               ? label
                               : QString("%1  %2%").arg(label).arg(int(std::round(box.score * 100)));
            QColor color = segmentationMode ? segmentationClassColor(box.cls) : classColor(box.cls);
            drawBox(vis, rect, text, color);
        }

        R.outputImage = vis;
        R.segmentationMask = (segmentationMode ? overlay : QImage());
        R.dets.reserve(kept.size());
        for (const auto &box : kept)
        {
            Detection det;
            det.x1 = box.x1;
            det.y1 = box.y1;
            det.x2 = box.x2;
            det.y2 = box.y2;
            det.score = box.score;
            det.cls = box.cls;
            det.maskAreaPixels = box.maskAreaPixels;
            det.hasMask = box.hasMask;
            R.dets.push_back(det);
        }

        if (segmentationMode)
        {
            R.summary = kept.empty() ? QStringLiteral("No segmentation detected")
                                     : QStringLiteral("Segments: %1").arg((int)kept.size());
        }
        else
        {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
            for (const auto &b : kept)
            {
                if (b.cls == 0)
                    ++c0;
                else if (b.cls == 1)
                    ++c1;
                else if (b.cls == 2)
                    ++c2;
                else
                    ++c3;
            }
            R.summary = QStringLiteral("Detections: %1  [CAM:%2  PINCER:%3  MIXED:%4  NORMAL:%5]")
                            .arg((int)kept.size())
                            .arg(c0)
                            .arg(c1)
                            .arg(c2)
                            .arg(c3);
        }

        return R;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(QString("推理异常: %1").arg(e.what()), "Inference", 5023);
        Result err;
        err.outputImage = vis;
        err.summary = QString("推理异常: %1").arg(e.what());
        return err;
    }
    catch (...)
    {
        LOG_ERROR("推理发生未知异常", "Inference", 5024);
        Result err;
        err.outputImage = vis;
        err.summary = "推理发生未知异常";
        return err;
    }
}
#endif

bool InferenceEngine::hasSegmentationSupport() const
{
#ifdef HAVE_ORT
    return m_ort && m_ort->session &&
           m_segOutputIndex >= 0 &&
           m_maskChannels > 0 &&
           m_maskWidth > 0 &&
           m_maskHeight > 0;
#else
    return false;
#endif
}


