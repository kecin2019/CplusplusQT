#pragma once
#include <QString>
#include <QImage>
#include <QMap>
#include <QDateTime>
#include <memory>

namespace medyolo11qt::medical
{

    /**
     * @brief DICOM标签信息结构
     */
    struct DicomTag
    {
        QString tagId; // 标签ID (如: "0010,0010")
        QString name;  // 标签名称
        QString value; // 标签值
        QString vr;    // 值表示 (Value Representation)

        DicomTag() = default;
        DicomTag(const QString &id, const QString &n, const QString &v, const QString &vrType = "")
            : tagId(id), name(n), value(v), vr(vrType) {}
    };

    /**
     * @brief DICOM元数据结构
     */
    struct DicomMetadata
    {
        QString patientName;
        QString patientId;
        QString studyId;
        QString seriesId;
        QString instanceId;
        QString modality;
        QString bodyPart;
        QDateTime studyDate;
        QDateTime seriesDate;
        QString institution;
        QString manufacturer;
        QString model;
        QString description;
        QMap<QString, DicomTag> customTags;

        bool isValid() const
        {
            return !patientName.isEmpty() && !studyId.isEmpty();
        }

        void clear()
        {
            patientName.clear();
            patientId.clear();
            studyId.clear();
            seriesId.clear();
            instanceId.clear();
            modality.clear();
            bodyPart.clear();
            studyDate = QDateTime();
            seriesDate = QDateTime();
            institution.clear();
            manufacturer.clear();
            model.clear();
            description.clear();
            customTags.clear();
        }
    };

    /**
     * @brief DICOM工具类
     * 提供DICOM文件的读取、解析和转换功能
     */
    class DicomUtils
    {
    public:
        /**
         * @brief 检查文件是否为有效的DICOM文件
         * @param filePath 文件路径
         * @return 是否为DICOM文件
         */
        static bool isDicomFile(const QString &filePath);

        /**
         * @brief 读取DICOM图像
         * @param filePath 文件路径
         * @param metadata 输出的元数据
         * @return 图像数据，失败时返回空图像
         */
        static QImage readDicomImage(const QString &filePath, DicomMetadata *metadata = nullptr);

        /**
         * @brief 读取DICOM元数据
         * @param filePath 文件路径
         * @return 元数据结构
         */
        static DicomMetadata readDicomMetadata(const QString &filePath);

        /**
         * @brief 获取DICOM标签信息
         * @param filePath 文件路径
         * @param tagId 标签ID (如: "0010,0010")
         * @return 标签信息
         */
        static DicomTag getDicomTag(const QString &filePath, const QString &tagId);

        /**
         * @brief 获取所有DICOM标签
         * @param filePath 文件路径
         * @return 标签映射
         */
        static QMap<QString, DicomTag> getAllDicomTags(const QString &filePath);

        /**
         * @brief 转换DICOM窗口参数
         * @param intercept 截距
         * @param slope 斜率
         * @param windowCenter 窗位
         * @param windowWidth 窗宽
         * @return 转换参数
         */
        static QImage convertDicomWindow(const QImage &image,
                                         double intercept, double slope,
                                         double windowCenter, double windowWidth);

        /**
         * @brief 获取支持的DICOM格式
         * @return 文件扩展名列表
         */
        static QStringList getSupportedDicomFormats();

        /**
         * @brief 获取DICOM文件信息
         * @param filePath 文件路径
         * @return 文件信息字符串
         */
        static QString getDicomFileInfo(const QString &filePath);

    private:
        class Impl;
        static std::unique_ptr<Impl> m_impl;

        /**
         * @brief 初始化GDCM库
         */
        static bool initializeGDCM();

        /**
         * @brief 清理GDCM资源
         */
        static void cleanupGDCM();

        /**
         * @brief 解析DICOM标签
         */
        static DicomTag parseDicomTag(const QString &tagId, const QString &value, const QString &vr);

        /**
         * @brief 标准化标签ID
         */
        static QString normalizeTagId(const QString &tagId);
    };

} // namespace medyolo11qt::medical