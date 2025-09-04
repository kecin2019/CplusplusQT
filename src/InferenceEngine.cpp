#include "InferenceEngine.h"

#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cmath>

#ifdef HAVE_ORT
#include <onnxruntime_cxx_api.h>
#endif

// ====================== 辅助函数（匿名命名空间） ======================
namespace
{

    inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

    struct Det
    {
        float x1, y1, x2, y2, score;
        int cls;
    };

    inline float iou(const Det &a, const Det &b)
    {
        const float xx1 = std::max(a.x1, b.x1);
        const float yy1 = std::max(a.y1, b.y1);
        const float xx2 = std::min(a.x2, b.x2);
        const float yy2 = std::min(a.y2, b.y2);
        const float w = std::max(0.f, xx2 - xx1);
        const float h = std::max(0.f, yy2 - yy1);
        const float inter = w * h;
        const float areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
        const float areaB = (b.x2 - b.x1) * (b.y2 - b.y1);
        const float u = areaA + areaB - inter + 1e-6f;
        return inter / u;
    }

    // Ultralytics 风格：class-wise NMS（offset 技巧）
    static std::vector<Det> nms_classwise(std::vector<Det> dets, float iouThr)
    {
        const float max_wh = 4096.0f; // 与 Ultralytics 保持一致
        // 按类别偏移
        for (auto &d : dets)
        {
            float off = d.cls * max_wh;
            d.x1 += off;
            d.x2 += off;
        }
        std::sort(dets.begin(), dets.end(), [](const Det &a, const Det &b)
                  { return a.score > b.score; });
        std::vector<Det> keep;
        keep.reserve(dets.size());
        for (size_t i = 0; i < dets.size(); ++i)
        {
            bool drop = false;
            for (const auto &k : keep)
            {
                if (iou(dets[i], k) > iouThr)
                {
                    drop = true;
                    break;
                }
            }
            if (!drop)
                keep.push_back(dets[i]);
            if (keep.size() >= 300)
                break;
        }
        // 去掉偏移
        for (auto &d : keep)
        {
            float off = d.cls * max_wh;
            d.x1 -= off;
            d.x2 -= off;
        }
        return keep;
    }

    static void drawBox(QImage &img, const Det &d, const QString &text)
    {
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
        p.setPen(QPen(Qt::green, 2));
        QRectF r(d.x1, d.y1, d.x2 - d.x1, d.y2 - d.y1);
        p.drawRect(r);

        QFont f("Sans", 12, QFont::Bold);
        p.setFont(f);
        QFontMetrics fm(f);
        const int tw = fm.horizontalAdvance(text) + 8;
        const int th = fm.height() + 4;
        QRect tr((int)r.x(), std::max(0, (int)r.y() - th), tw, th);
        p.fillRect(tr, QColor(0, 0, 0, 160));
        p.setPen(Qt::yellow);
        p.drawText(tr.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }

    // -------- Ultralytics letterbox 预处理 --------
    // 返回 letterboxed 图像、缩放比例 r、左右上下 padding (dw, dh)
    static QImage letterbox(const QImage &src, int newW, int newH, float &r, int &dw, int &dh,
                            bool scaleup = true, int stride = 32, int padv = 114)
    {
        const int w0 = src.width();
        const int h0 = src.height();
        r = std::min((float)newW / (float)w0, (float)newH / (float)h0);
        if (!scaleup)
            r = std::min(r, 1.0f);

        int w = int(std::round(w0 * r));
        int h = int(std::round(h0 * r));

        // 按 stride 对齐（Ultralytics 的 auto=True 行为在批量中使用；这里保持最常见简单形态）
        int dw_total = newW - w;
        int dh_total = newH - h;
        dw = dw_total / 2;
        dh = dh_total / 2;

        // 画布填充 114
        QImage canvas(newW, newH, QImage::Format_RGB888);
        canvas.fill(QColor(padv, padv, padv));

        // 将原图按 r 缩放并粘贴到 (dw, dh)
        QImage scaled = src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        {
            QPainter p(&canvas);
            p.drawImage(QPoint(dw, dh), scaled);
        }
        return canvas;
    }

    // 反变换：从 letterbox 坐标 -> 原图坐标
    static void scale_boxes_back(std::vector<Det> &dets, float r, int dw, int dh, int w0, int h0)
    {
        for (auto &d : dets)
        {
            d.x1 = (d.x1 - dw) / r;
            d.y1 = (d.y1 - dh) / r;
            d.x2 = (d.x2 - dw) / r;
            d.y2 = (d.y2 - dh) / r;
            d.x1 = std::clamp(d.x1, 0.f, (float)w0 - 1.f);
            d.y1 = std::clamp(d.y1, 0.f, (float)h0 - 1.f);
            d.x2 = std::clamp(d.x2, 0.f, (float)w0 - 1.f);
            d.y2 = std::clamp(d.y2, 0.f, (float)h0 - 1.f);
        }
    }

#if ORT_DIAG
    static QString shapeToStr(const std::vector<int64_t> &v)
    {
        QString s = "[";
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (i)
                s += ", ";
            s += QString::number((long long)v[i]);
        }
        s += "]";
        return s;
    }
    static QString onnxTypeToStr(ONNXType t)
    {
        switch (t)
        {
        case ONNX_TYPE_UNKNOWN:
            return "UNKNOWN";
        case ONNX_TYPE_TENSOR:
            return "TENSOR";
        case ONNX_TYPE_SEQUENCE:
            return "SEQUENCE";
        case ONNX_TYPE_MAP:
            return "MAP";
        case ONNX_TYPE_OPAQUE:
            return "OPAQUE";
        case ONNX_TYPE_OPTIONAL:
            return "OPTIONAL";
        default:
            return "OTHER";
        }
    }
    static QString elemTypeToStr(ONNXTensorElementDataType t)
    {
        switch (t)
        {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            return "float32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
            return "uint8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
            return "int8";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
            return "uint16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
            return "int16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
            return "int32";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
            return "int64";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
            return "bool";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            return "float16";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            return "double";
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:
            return "bfloat16";
        default:
            return "other";
        }
    }
    // 安全把 const char* 转 QString（最多读 4096 字节）
    static QString safeName(const char *p)
    {
        if (!p)
            return "<null>";
        const size_t MAX_L = 4096;
        size_t n = 0;
        while (n < MAX_L && p[n] != '\0')
            ++n;
        return QString::fromUtf8(p, (int)n);
    }
#endif // ORT_DIAG

} // namespace

// ====================== ORT 打包结构 ======================
#ifdef HAVE_ORT
struct InferenceEngine::OrtPack
{
#if ORT_DIAG
    Ort::Env env{ORT_LOGGING_LEVEL_VERBOSE, "medapp"};
#else
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "medapp"};
#endif
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;

    std::vector<Ort::AllocatedStringPtr> inNamePtrs, outNamePtrs;
    std::vector<const char *> inputNames, outputNames;
};
#endif // HAVE_ORT

// ====================== InferenceEngine 成员实现 ======================
InferenceEngine::InferenceEngine() = default;
InferenceEngine::~InferenceEngine() = default;

bool InferenceEngine::loadLabelsFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ts.setEncoding(QStringConverter::Utf8);
#endif
    std::vector<QString> tmp;
    while (!ts.atEnd())
    {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith('#'))
            continue;
        tmp.push_back(line);
    }
    if (!tmp.empty())
    {
        m_labels.swap(tmp);
        return true;
    }
    return false;
}

// ---- 加载模型：含 I/O 摘要与检测头识别（稳健） ----
bool InferenceEngine::loadModel(const QString &weightsPath, Task /*taskHint*/)
{
#ifndef HAVE_ORT
    qWarning() << "Built without ONNXRuntime. Skipping model load.";
    return false;
#else
    m_ort = std::make_unique<OrtPack>();
    m_ort->opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try
    {
        m_ort->session = std::make_unique<Ort::Session>(
            m_ort->env, weightsPath.toUtf8().constData(), m_ort->opts);
    }
    catch (const Ort::Exception &e)
    {
        qWarning() << "[ORT] Load failed:" << e.what();
        m_ort.reset();
        return false;
    }

    Ort::AllocatorWithDefaultOptions alloc;
    const size_t numInputs = m_ort->session->GetInputCount();
    const size_t numOutputs = m_ort->session->GetOutputCount();

    m_ort->inNamePtrs.clear();
    m_ort->outNamePtrs.clear();
    m_ort->inputNames.clear();
    m_ort->outputNames.clear();
    m_ort->inNamePtrs.reserve(numInputs);
    m_ort->outNamePtrs.reserve(numOutputs);
    m_ort->inputNames.reserve(numInputs);
    m_ort->outputNames.reserve(numOutputs);

    for (size_t i = 0; i < numInputs; ++i)
    {
        m_ort->inNamePtrs.emplace_back(m_ort->session->GetInputNameAllocated(i, alloc));
        m_ort->inputNames.push_back(m_ort->inNamePtrs.back().get());
    }
    for (size_t i = 0; i < numOutputs; ++i)
    {
        m_ort->outNamePtrs.emplace_back(m_ort->session->GetOutputNameAllocated(i, alloc));
        m_ort->outputNames.push_back(m_ort->outNamePtrs.back().get());
    }

#if ORT_DIAG
    try
    {
        qDebug() << "----- ONNX IO Summary -----";
        qDebug() << "Inputs:" << (int)numInputs;
        for (size_t i = 0; i < numInputs; ++i)
        {
            auto ti = m_ort->session->GetInputTypeInfo(i);
            ONNXType t = ti.GetONNXType();
            if (t == ONNX_TYPE_TENSOR)
            {
                auto tt = ti.GetTensorTypeAndShapeInfo();
                qDebug() << "  [" << (int)i << "] name=" << safeName(m_ort->inputNames[i])
                         << " onnx=" << onnxTypeToStr(t)
                         << " elem=" << elemTypeToStr(tt.GetElementType())
                         << " shape=" << shapeToStr(tt.GetShape());
            }
            else
            {
                qDebug() << "  [" << (int)i << "] name=" << safeName(m_ort->inputNames[i])
                         << " onnx=" << onnxTypeToStr(t)
                         << " (non-tensor)";
            }
        }
        qDebug() << "Outputs:" << (int)numOutputs;
        for (size_t i = 0; i < numOutputs; ++i)
        {
            auto to = m_ort->session->GetOutputTypeInfo(i);
            ONNXType t = to.GetONNXType();
            if (t == ONNX_TYPE_TENSOR)
            {
                auto tt = to.GetTensorTypeAndShapeInfo();
                qDebug() << "  [" << (int)i << "] name=" << safeName(m_ort->outputNames[i])
                         << " onnx=" << onnxTypeToStr(t)
                         << " elem=" << elemTypeToStr(tt.GetElementType())
                         << " shape=" << shapeToStr(tt.GetShape());
            }
            else
            {
                qDebug() << "  [" << (int)i << "] name=" << safeName(m_ort->outputNames[i])
                         << " onnx=" << onnxTypeToStr(t)
                         << " (non-tensor)";
            }
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "[ORT] IO summary error:" << e.what();
    }
#endif // ORT_DIAG

    // 输入尺寸：仅当 input0 是 Tensor 时读取，否则回退 640
    try
    {
        auto t0 = m_ort->session->GetInputTypeInfo(0);
        if (t0.GetONNXType() == ONNX_TYPE_TENSOR)
        {
            auto inInfo = t0.GetTensorTypeAndShapeInfo();
            auto inShape = inInfo.GetShape(); // [1,3,H,W] 或 [N,3,H,W]
            if (inShape.size() >= 4)
            {
                long long H = inShape[2], W = inShape[3];
                if (H > 0 && W > 0)
                {
                    m_inH = (int)H;
                    m_inW = (int)W;
                }
            }
        }
        else
        {
            m_inH = m_inW = 640;
        }
    }
    catch (...)
    {
        m_inH = m_inW = 640;
    }

    // 检测头识别：在 “tensor & float & rank=3” 的输出里找 [1,C,N] 或 [1,N,C]
    m_isYoloDetect = false;
    m_hasObj = false;
    m_numClasses = 0;
    m_channels = 0;
    try
    {
        for (size_t oi = 0; oi < numOutputs; ++oi)
        {
            auto to = m_ort->session->GetOutputTypeInfo(oi);
            if (to.GetONNXType() != ONNX_TYPE_TENSOR)
                continue;
            auto outInfo = to.GetTensorTypeAndShapeInfo();
            if (outInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                continue;
            auto outShape = outInfo.GetShape();
            if (outShape.size() == 3)
            {
                int d1 = (int)std::llabs(outShape[1]);
                int d2 = (int)std::llabs(outShape[2]);
                int C = std::min(d1, d2);
                int N = std::max(d1, d2);
                if (C >= 6 && N >= 10)
                {
                    m_isYoloDetect = true;
                    int nc_v8 = C - 4, nc_v5 = C - 5;
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
                    break;
                }
            }
        }
    }
    catch (...)
    {
        // 继续，run() 里还有兜底
    }

#if ORT_DIAG
    qDebug() << "[ORT] in=" << m_inW << "x" << m_inH
             << "detect=" << m_isYoloDetect
             << "C=" << m_channels
             << "nc=" << m_numClasses
             << "hasObj=" << m_hasObj;
#endif

    // 若没显式设置标签，给默认名
    if (m_labels.empty() && m_numClasses > 0)
    {
        m_labels.resize(m_numClasses);
        for (int i = 0; i < m_numClasses; ++i)
            m_labels[i] = QString("Class %1").arg(i);
    }

    m_modelPath = weightsPath;
    return true;
#endif // HAVE_ORT
}

// ---- 推理：Ultralytics 等价流程（letterbox -> ORT -> 解析 -> class-wise NMS -> 反变换 -> 绘制） ----
InferenceEngine::Result InferenceEngine::run(const QImage &input, Task /*taskHint*/) const
{
    Result r;
    QImage vis = input.convertToFormat(QImage::Format_ARGB32);

#ifndef HAVE_ORT
    r.outputImage = vis;
    r.summary = "Built without ONNXRuntime";
    return r;
#else
    if (!m_ort || !m_ort->session)
    {
        r.outputImage = vis;
        r.summary = "Model not loaded";
        return r;
    }

    // ---------- 预处理（Ultralytics letterbox） ----------
    const int netW = m_inW, netH = m_inH;
    QImage rgb = input.convertToFormat(QImage::Format_RGB888);
    float rsc = 1.f;
    int dw = 0, dh = 0;
    QImage lbox = letterbox(rgb, netW, netH, rsc, dw, dh, /*scaleup=*/true, /*stride=*/32, /*padv=*/114);

    // to NCHW float32 [0,1]
    std::vector<float> tensor(3 * netH * netW);
    for (int y = 0; y < netH; ++y)
    {
        const uchar *line = lbox.constScanLine(y);
        for (int x = 0; x < netW; ++x)
        {
            const int idx = y * netW + x;
            tensor[0 * netH * netW + idx] = line[3 * x + 0] / 255.0f; // R
            tensor[1 * netH * netW + idx] = line[3 * x + 1] / 255.0f; // G
            tensor[2 * netH * netW + idx] = line[3 * x + 2] / 255.0f; // B
        }
    }

    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
    std::array<int64_t, 4> ishape{1, 3, netH, netW};
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, tensor.data(), tensor.size(), ishape.data(), ishape.size());

    // ---------- 选择一个 rank=3 float 的输出 ----------
    const size_t numOut = m_ort->session->GetOutputCount();
    int chosen = -1;
    std::vector<std::vector<int64_t>> outShapes(numOut);
    std::vector<ONNXTensorElementDataType> outTypes(numOut);
    for (size_t i = 0; i < numOut; ++i)
    {
        auto ti = m_ort->session->GetOutputTypeInfo(i);
        if (ti.GetONNXType() != ONNX_TYPE_TENSOR)
            continue;
        auto tshape = ti.GetTensorTypeAndShapeInfo();
        outShapes[i] = tshape.GetShape();
        outTypes[i] = tshape.GetElementType();
        if (chosen < 0 &&
            outTypes[i] == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT &&
            outShapes[i].size() == 3)
        {
            chosen = (int)i;
        }
    }
    if (chosen < 0)
    {
        r.outputImage = vis;
        r.summary = "No suitable tensor output found (expect rank=3 float).";
        return r;
    }

#if ORT_DIAG
    qDebug() << "[ORT] chosen output index =" << chosen
             << " shape=" << shapeToStr(outShapes[(size_t)chosen])
             << " type=" << elemTypeToStr(outTypes[(size_t)chosen]);
#endif

    // ---------- 推理 ----------
    const char *outName = m_ort->outputNames[(size_t)chosen];
    std::vector<Ort::Value> outs;
    try
    {
        outs = m_ort->session->Run(Ort::RunOptions{nullptr},
                                   m_ort->inputNames.data(), &in, 1,
                                   &outName, 1);
    }
    catch (const Ort::Exception &e)
    {
        r.outputImage = vis;
        r.summary = QString::fromUtf8(e.what());
        return r;
    }
    if (outs.empty() || !outs[0].IsTensor())
    {
        r.outputImage = vis;
        r.summary = "Invalid output (empty or non-tensor)";
        return r;
    }

    const float *data = outs[0].GetTensorData<float>();
    auto info = outs[0].GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape(); // 期望 [1, N, A]，或 [1, A, N]
    if (shape.size() != 3)
    {
        r.outputImage = vis;
        r.summary = QString("Unexpected output rank: %1").arg((int)shape.size());
        return r;
    }

    // ---------- 解析 YOLO（与 Ultralytics 一致） ----------
    const int s1 = (int)std::llabs(shape[1]);
    const int s2 = (int)std::llabs(shape[2]);
    const bool attrLast = (s2 >= 6 && s2 <= 1024); // 更常见： [1, N, A]
    const int A = attrLast ? s2 : s1;              // 属性维：4(+1)+nc
    const int N = attrLast ? s1 : s2;              // 候选数

    // 判定是否含 obj 与 nc（若 load 时未识别）
    int C = A;
    int nc = (m_numClasses > 0) ? m_numClasses : std::max(1, std::max(C - 4, C - 5));
    bool hasObj = (m_numClasses > 0) ? m_hasObj : (C - 5 == nc); // v5: 4+1+nc

    // 采样判断分数是否已经 sigmoid（若已在 0..1 区间，不再二次 sigmoid）
    auto need_sigmoid = [&](int offset, int len) -> bool
    {
        int cnt = 0, over = 0, under = 0;
        for (int i = 0; i < std::min(len, 2000); ++i)
        {
            float v = data[offset + i];
            if (v > 1.0f)
                over++;
            else if (v < 0.0f)
                under++;
            cnt++;
        }
        // 有明显超界才做 sigmoid
        return (over + under) * 1.0f / std::max(1, cnt) > 0.05; // 5% 超界判定为 logits
    };

    // 解析函数
    auto getAttr = [&](int i, int k) -> float
    {
        // i in [0,N), k in [0,A)
        return attrLast ? data[i * A + k] : data[k * N + i];
    };

    // 是否需要对 cls 概率/obj 再做 sigmoid
    bool cls_need_sig = false, obj_need_sig = false;
    {
        // 抽查第 0~min(N,32) 个候选的前 32 个类别 logits
        const int sample_n = std::min(N, 32);
        const int cls_off = hasObj ? 5 : 4;
        if (nc > 0)
        {
            // 取第一行进行判定即可
            const int off0 = attrLast ? (0 * A + cls_off) : (cls_off * N + 0);
            cls_need_sig = need_sigmoid(off0, std::min(nc, 64));
        }
        if (hasObj)
        {
            const int off_obj = attrLast ? (0 * A + 4) : (4 * N + 0);
            obj_need_sig = need_sigmoid(off_obj, sample_n);
        }
    }

    // 候选收集
    std::vector<Det> dets;
    dets.reserve(std::min(N, 3000));
    for (int i = 0; i < N; ++i)
    {
        float cx = getAttr(i, 0);
        float cy = getAttr(i, 1);
        float bw = getAttr(i, 2);
        float bh = getAttr(i, 3);

        // Ultralytics 导出通常已是像素坐标（基于 letterbox 后的输入尺寸）
        // 若发现坐标全部在 [0,2] 内，说明是归一化，放大到像素
        static bool checked_norm = false;
        static bool normalized = false;
        if (!checked_norm)
        {
            float mx = std::max({std::fabs(cx), std::fabs(cy), std::fabs(bw), std::fabs(bh)});
            normalized = (mx <= 2.0f); // 经验阈值
            checked_norm = true;
        }
        if (normalized)
        {
            cx *= netW;
            cy *= netH;
            bw *= netW;
            bh *= netH;
        }

        float obj = hasObj ? getAttr(i, 4) : 1.f;
        if (hasObj && obj_need_sig)
            obj = sigmoid(obj);

        // 取最大类别置信度
        int best = -1;
        float bestp = 0.f;
        const int cls_off = hasObj ? 5 : 4;
        for (int k = 0; k < nc; ++k)
        {
            float p = getAttr(i, cls_off + k);
            if (cls_need_sig)
                p = sigmoid(p);
            float score = hasObj ? (obj * p) : p;
            if (score > bestp)
            {
                bestp = score;
                best = k;
            }
        }
        if (best < 0 || bestp < m_confThr)
            continue;

        // (cx,cy,w,h)->(x1,y1,x2,y2)（仍在 letterbox 坐标系）
        float x1 = cx - bw * 0.5f;
        float y1 = cy - bh * 0.5f;
        float x2 = cx + bw * 0.5f;
        float y2 = cy + bh * 0.5f;

        // clip 到网络尺寸（防止溢出）
        x1 = std::clamp(x1, 0.f, (float)netW - 1.f);
        y1 = std::clamp(y1, 0.f, (float)netH - 1.f);
        x2 = std::clamp(x2, 0.f, (float)netW - 1.f);
        y2 = std::clamp(y2, 0.f, (float)netH - 1.f);

        if (x2 - x1 >= 2 && y2 - y1 >= 2)
            dets.push_back({x1, y1, x2, y2, bestp, best});
    }

    // ---------- NMS（class-wise，与 Ultralytics 一致） ----------
    auto kept = nms_classwise(std::move(dets), m_iouThr);

    // ---------- 坐标反变换回原图尺寸 ----------
    scale_boxes_back(kept, rsc, dw, dh, input.width(), input.height());

    // ---------- 绘制 ----------
    for (const auto &d : kept)
    {
        QString name = (d.cls >= 0 && d.cls < (int)m_labels.size())
                           ? m_labels[d.cls]
                           : QString("cls%1").arg(d.cls);
        QString txt = QString("%1  %2%").arg(name).arg(int(std::round(d.score * 100.f)));
        drawBox(vis, d, txt);
    }

    r.outputImage = vis;
    r.summary = QString("Detections: %1").arg(kept.size());
    return r;
#endif // HAVE_ORT
}
