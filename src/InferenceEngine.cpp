#include "InferenceEngine.h"
#include <QPainter>
#include <QDebug>
#include <algorithm>
#include <array>
#include <cmath>
#include <QFile>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>

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
        const int w0 = src.width(), h0 = src.height();
        r = std::min((float)newW / w0, (float)newH / h0);
        if (!scaleup)
            r = std::min(r, 1.0f);
        int w = int(std::round(w0 * r)), h = int(std::round(h0 * r));
        int dw_total = newW - w, dh_total = newH - h;
        dw = dw_total / 2;
        dh = dh_total / 2;
        QImage canvas(newW, newH, QImage::Format_RGB888);
        canvas.fill(QColor(padv, padv, padv));
        QImage scaled = src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter p(&canvas);
        p.drawImage(QPoint(dw, dh), scaled);
        return canvas;
    }

    static void scale_boxes_back(std::vector<Box> &dets, float r, int dw, int dh, int w0, int h0)
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

} // namespace

#ifdef HAVE_ORT
struct InferenceEngine::OrtPack
{
#if 1
    Ort::Env env{ORT_LOGGING_LEVEL_INFO, "medapp"};
#else
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "medapp"};
#endif
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;
    std::vector<Ort::AllocatedStringPtr> inPtrs, outPtrs;
    std::vector<const char *> inputNames, outputNames;
};
#endif

// 固定 4 类
QString InferenceEngine::className(int cls)
{
    static const char *n[4] = {"CAM", "PINCER", "MIXED", "NORMAL"};
    if (cls < 0 || cls > 3)
        return QString("C%1").arg(cls);
    return n[cls];
}
QColor InferenceEngine::classColor(int cls)
{
    switch (cls)
    {
    case 0:
        return QColor(255, 69, 0); // CAM    - OrangeRed
    case 1:
        return QColor(30, 144, 255); // PINCER - DodgerBlue
    case 2:
        return QColor(50, 205, 50); // MIXED  - LimeGreen
    case 3:
        return QColor(255, 215, 0); // NORMAL - Gold
    default:
        return QColor(200, 200, 200);
    }
}

InferenceEngine::InferenceEngine() = default;
InferenceEngine::~InferenceEngine() = default;

// 添加XOR解密函数
QByteArray xorDecrypt(const QByteArray &data, const QByteArray &key)
{
    QByteArray result;
    result.resize(data.size());

    for (int i = 0; i < data.size(); ++i)
    {
        result[i] = data[i] ^ key[i % key.size()];
    }

    return result;
}

bool InferenceEngine::loadModel(const QString &path, Task /*taskHint*/)
{
#ifndef HAVE_ORT
    qWarning() << "Built without ONNXRuntime";
    return false;
#else
    // 读取加密的模型文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to open model file:" << path;
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    // 检查文件头（至少8字节）
    if (fileData.size() < 8)
    {
        qWarning() << "Model file too small or corrupted";
        return false;
    }

    // 读取文件头
    QDataStream headerStream(fileData);
    quint32 magic, version;
    headerStream >> magic >> version;

    // 验证魔数
    if (magic != 0x4D594F4C)
    { // "MYOL"
        qWarning() << "Invalid model file format or not encrypted";
        return false;
    }

    // 解密模型数据
    QByteArray encryptedData = fileData.mid(8); // 跳过8字节文件头

    QByteArray key = QCryptographicHash::hash(
        "MedYOLO11Qt_Model_Protection_Key_2024",
        QCryptographicHash::Sha256);

    QByteArray modelData = xorDecrypt(encryptedData, key);

    m_ort = std::make_unique<OrtPack>();
    m_ort->opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try
    {
        // 使用内存中的模型数据创建session
        m_ort->session = std::make_unique<Ort::Session>(
            m_ort->env,
            modelData.constData(),
            modelData.size(),
            m_ort->opts);
    }
    catch (const Ort::Exception &e)
    {
        qWarning() << "[ORT]" << e.what();
        m_ort.reset();
        return false;
    }

    Ort::AllocatorWithDefaultOptions alloc;
    size_t ni = m_ort->session->GetInputCount(), no = m_ort->session->GetOutputCount();
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

    // 输入尺寸
    try
    {
        auto ti = m_ort->session->GetInputTypeInfo(0);
        if (ti.GetONNXType() == ONNX_TYPE_TENSOR)
        {
            auto inf = ti.GetTensorTypeAndShapeInfo();
            auto sh = inf.GetShape(); // [1,3,H,W]
            if (sh.size() >= 4 && sh[2] > 0 && sh[3] > 0)
            {
                m_inH = (int)sh[2];
                m_inW = (int)sh[3];
            }
        }
    }
    catch (...)
    {
        m_inH = m_inW = 640;
    }

    // 输出结构判定：挑一个 rank=3 的 float 输出
    m_isYoloDetect = false;
    m_hasObj = false;
    m_numClasses = 0;
    m_channels = 0;
    for (size_t i = 0; i < no; ++i)
    {
        auto to = m_ort->session->GetOutputTypeInfo(i);
        if (to.GetONNXType() != ONNX_TYPE_TENSOR)
            continue;
        auto inf = to.GetTensorTypeAndShapeInfo();
        if (inf.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            continue;
        auto sh = inf.GetShape(); // [1,C,N] 或 [1,N,C]
        if (sh.size() == 3)
        {
            int d1 = std::abs((int)sh[1]), d2 = std::abs((int)sh[2]);
            int C = std::min(d1, d2), N = std::max(d1, d2);
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
    m_modelPath = path;
    return true;
#endif
}

bool InferenceEngine::isSegmentationModel() const
{
#ifndef HAVE_ORT
    return false;
#else
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
#endif
}

QImage InferenceEngine::processSegmentation(const QImage &input) const
{
#ifndef HAVE_ORT
    return QImage();
#else
    if (!m_ort || !m_ort->session || !isSegmentationModel())
        return QImage();

    // 预处理：调整到模型输入尺寸
    QImage rgb = input.convertToFormat(QImage::Format_RGB888);
    QImage resized = rgb.scaled(m_inW, m_inH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    std::vector<float> tensor(3 * m_inH * m_inW);
    for (int y = 0; y < m_inH; ++y)
    {
        const uchar *line = resized.constScanLine(y);
        for (int x = 0; x < m_inW; ++x)
        {
            int idx = y * m_inW + x;
            tensor[0 * m_inH * m_inW + idx] = line[3 * x + 0] / 255.f;
            tensor[1 * m_inH * m_inW + idx] = line[3 * x + 1] / 255.f;
            tensor[2 * m_inH * m_inW + idx] = line[3 * x + 2] / 255.f;
        }
    }

    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
    std::array<int64_t, 4> ishape{1, 3, m_inH, m_inW};
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, tensor.data(), tensor.size(), ishape.data(), ishape.size());

    // 找到分割输出（形状为 [1, C, H, W]）
    const size_t no = m_ort->session->GetOutputCount();
    int segOutputIndex = -1;
    for (size_t i = 0; i < no; ++i)
    {
        auto to = m_ort->session->GetOutputTypeInfo(i);
        if (to.GetONNXType() != ONNX_TYPE_TENSOR)
            continue;
        auto inf = to.GetTensorTypeAndShapeInfo();
        if (inf.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            continue;
        auto sh = inf.GetShape();
        if (sh.size() == 4 && sh[0] == 1 && sh[1] > 1)
        {
            segOutputIndex = i;
            break;
        }
    }

    if (segOutputIndex == -1)
        return QImage();

    const char *outName = m_ort->outputNames[segOutputIndex];
    std::vector<Ort::Value> outs;
    try
    {
        outs = m_ort->session->Run(Ort::RunOptions{nullptr},
                                   m_ort->inputNames.data(), &in, 1,
                                   &outName, 1);
    }
    catch (const Ort::Exception &e)
    {
        qWarning() << "Segmentation inference failed:" << e.what();
        return QImage();
    }

    if (outs.empty() || !outs[0].IsTensor())
        return QImage();

    const float *data = outs[0].GetTensorData<float>();
    auto info = outs[0].GetTensorTypeAndShapeInfo();
    auto sh = info.GetShape(); // [1, C, H, W]

    const int num_classes = sh[1];
    const int seg_h = sh[2];
    const int seg_w = sh[3];

    // 创建分割掩码图像
    QImage mask(seg_w, seg_h, QImage::Format_ARGB32);

    // 简单的argmax处理：取每个位置概率最大的类别
    for (int y = 0; y < seg_h; ++y)
    {
        QRgb *scanline = reinterpret_cast<QRgb *>(mask.scanLine(y));
        for (int x = 0; x < seg_w; ++x)
        {
            int max_class = 0;
            float max_val = -FLT_MAX;

            for (int c = 0; c < num_classes; ++c)
            {
                float val = data[c * seg_h * seg_w + y * seg_w + x];
                if (val > max_val)
                {
                    max_val = val;
                    max_class = c;
                }
            }

            // 为不同类别分配不同颜色
            QColor color;
            switch (max_class)
            {
            case 0:
                color = QColor(255, 0, 0, 128);
                break; // 红色 - 背景
            case 1:
                color = QColor(0, 255, 0, 128);
                break; // 绿色 - 骨骼
            case 2:
                color = QColor(0, 0, 255, 128);
                break; // 蓝色 - 软组织
            case 3:
                color = QColor(255, 255, 0, 128);
                break; // 黄色 - 其他
            default:
                color = QColor(128, 128, 128, 128);
            }

            scanline[x] = color.rgba();
        }
    }

    return mask.scaled(input.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
}

InferenceEngine::Result InferenceEngine::run(const QImage &input, Task taskHint) const
{
    Result R;
    QImage vis = input.convertToFormat(QImage::Format_ARGB32);

    // 根据任务类型选择处理方式
    if (taskHint == Task::HipMRI_Seg && isSegmentationModel())
    {
        // MRI分割任务
        R.segmentationMask = processSegmentation(input);
        R.outputImage = input;
        R.summary = "MRI segmentation completed";
        return R;
    }
#ifndef HAVE_ORT
    R.outputImage = vis;
    R.summary = "Built without ONNXRuntime";
    return R;
#else
    if (!m_ort || !m_ort->session)
    {
        R.outputImage = vis;
        R.summary = "Model not loaded";
        return R;
    }

    // 预处理（Ultralytics letterbox + CHW + [0,1]）
    const int netW = m_inW, netH = m_inH;
    QImage rgb = input.convertToFormat(QImage::Format_RGB888);
    float rs = 1.f;
    int dw = 0, dh = 0;
    QImage lbox = letterbox(rgb, netW, netH, rs, dw, dh, true, 32, 114);

    std::vector<float> tensor(3 * netH * netW);
    for (int y = 0; y < netH; ++y)
    {
        const uchar *line = lbox.constScanLine(y);
        for (int x = 0; x < netW; ++x)
        {
            int idx = y * netW + x;
            tensor[0 * netH * netW + idx] = line[3 * x + 0] / 255.f;
            tensor[1 * netH * netW + idx] = line[3 * x + 1] / 255.f;
            tensor[2 * netH * netW + idx] = line[3 * x + 2] / 255.f;
        }
    }
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
    std::array<int64_t, 4> ishape{1, 3, netH, netW};
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, tensor.data(), tensor.size(), ishape.data(), ishape.size());

    // 选择一个 rank=3 的 float 输出
    const size_t no = m_ort->session->GetOutputCount();
    int chosen = -1;
    std::vector<std::vector<int64_t>> osh(no);
    for (size_t i = 0; i < no; ++i)
    {
        auto t = m_ort->session->GetOutputTypeInfo(i);
        if (t.GetONNXType() != ONNX_TYPE_TENSOR)
            continue;
        auto info = t.GetTensorTypeAndShapeInfo();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            continue;
        osh[i] = info.GetShape();
        if (chosen < 0 && osh[i].size() == 3)
            chosen = (int)i;
    }
    if (chosen < 0)
    {
        R.outputImage = vis;
        R.summary = "No rank-3 float output";
        return R;
    }

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
        R.outputImage = vis;
        R.summary = QString::fromUtf8(e.what());
        return R;
    }
    if (outs.empty() || !outs[0].IsTensor())
    {
        R.outputImage = vis;
        R.summary = "Invalid output";
        return R;
    }

    const float *data = outs[0].GetTensorData<float>();
    auto info = outs[0].GetTensorTypeAndShapeInfo();

    auto sh = info.GetShape(); // [1,N,A] 或 [1,A,N]
    const int d1 = std::abs((int)sh[1]);
    const int d2 = std::abs((int)sh[2]);

    // 属性维（A）应该是两者中较小的那个（通常 >=6 且远小于候选框数）
    const int A = std::min(d1, d2);
    const int N = std::max(d1, d2);

    // 如果第二维等于属性维，则说明布局是 [1, N, A]（属性在最后一维）；否则为 [1, A, N]
    const bool attrLast = (d2 == A);

    int nc = (m_numClasses > 0 ? m_numClasses : std::max(1, std::max(A - 4, A - 5)));
    bool hasObj = (m_numClasses > 0 ? m_hasObj : (A - 5 == nc));

    // 统一的取值函数：i 为第 i 个候选框，k 为属性下标（0..A-1）
    auto getAttr = [&](int i, int k) -> float
    {
        return attrLast ? data[i * A + k] : data[k * N + i];
    };

    auto need_sigmoid = [&](int offset, int len) -> bool
    {
        // 粗略判断是否需要 sigmoid（已是 0..1 就不再做）
        int cnt = 0, out = 0;
        for (int i = 0; i < std::min(len, 512); ++i)
        {
            float v = data[offset + i];
            if (v < 0.f || v > 1.f)
                out++;
            cnt++;
        }
        return out > cnt * 0.05;
    };
    bool cls_need_sig = false, obj_need_sig = false;
    if (nc > 0)
    {
        int cls_off = hasObj ? 5 : 4;
        int off0 = attrLast ? (0 * A + cls_off) : (cls_off * N + 0);
        cls_need_sig = need_sigmoid(off0, std::min(nc, 64));
    }
    if (hasObj)
    {
        int off = attrLast ? (0 * A + 4) : (4 * N + 0);
        obj_need_sig = need_sigmoid(off, std::min(N, 64));
    }

    std::vector<Box> dets;
    dets.reserve(std::min(N, 3000));
    for (int i = 0; i < N; ++i)
    {
        float cx = getAttr(i, 0), cy = getAttr(i, 1), bw = getAttr(i, 2), bh = getAttr(i, 3);

        float mx = std::max({std::fabs(cx), std::fabs(cy), std::fabs(bw), std::fabs(bh)});
        bool normalized = (mx <= 1.5f); // 认为是 0..1 坐标
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

        int best = -1;
        float bestp = 0.f;
        int cls_off = hasObj ? 5 : 4;
        for (int k = 0; k < nc; ++k)
        {
            float p = getAttr(i, cls_off + k);
            if (cls_need_sig)
                p = sigmoid(p);
            float s = hasObj ? (obj * p) : p;
            if (s > bestp)
            {
                bestp = s;
                best = k;
            }
        }
        if (best < 0 || bestp < m_confThr)
            continue;

        float x1 = cx - bw * 0.5f, y1 = cy - bh * 0.5f, x2 = cx + bw * 0.5f, y2 = cy + bh * 0.5f;
        x1 = std::clamp(x1, 0.f, (float)netW - 1.f);
        y1 = std::clamp(y1, 0.f, (float)netH - 1.f);
        x2 = std::clamp(x2, 0.f, (float)netW - 1.f);
        y2 = std::clamp(y2, 0.f, (float)netH - 1.f);

        // 压到 4 类范围（若模型有更多类别）
        int mapped = std::clamp(best, 0, 3);
        dets.push_back({x1, y1, x2, y2, bestp, mapped});
    }

    auto kept = nms_classwise(std::move(dets), m_iouThr);
    scale_boxes_back(kept, rs, dw, dh, input.width(), input.height());

    for (const auto &d : kept)
    {
        QRectF r(d.x1, d.y1, d.x2 - d.x1, d.y2 - d.y1);
        QString txt = QString("%1  %2%").arg(InferenceEngine::className(d.cls)).arg(int(std::round(d.score * 100)));
        drawBox(vis, r, txt, InferenceEngine::classColor(d.cls));
    }

    R.outputImage = vis;
    R.dets.reserve(kept.size());
    for (auto &b : kept)
        R.dets.push_back({b.x1, b.y1, b.x2, b.y2, b.score, b.cls});

    int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    for (auto &b : kept)
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
    R.summary = QString("Detections: %1  [CAM:%2  PINCER:%3  MIXED:%4  NORMAL:%5]")
                    .arg((int)kept.size())
                    .arg(c0)
                    .arg(c1)
                    .arg(c2)
                    .arg(c3);
    return R;
#endif
}