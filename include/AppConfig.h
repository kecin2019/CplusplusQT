#pragma once
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <memory>

namespace medyolo11qt::core
{

    /**
     * @brief 应用程序配置管理类
     * 负责管理程序的所有配置参数，包括界面设置、AI模型路径、文件关联等
     */
    class AppConfig
    {
    public:
        static AppConfig &getInstance();

        // 界面配置
        QColor getBackgroundColor() const { return m_backgroundColor; }
        QColor getTextColor() const { return m_textColor; }
        int getFontSize() const { return m_fontSize; }

        // AI模型配置
        QString getModelPath(const QString &modelName) const;
        QString getClassNamesPath() const { return m_classNamesPath; }
        double getConfidenceThreshold() const { return m_confidenceThreshold; }

        // 文件关联配置
        QStringList getSupportedImageFormats() const { return m_supportedImageFormats; }
        bool isDicomEnabled() const { return m_dicomEnabled; }

        // 配置持久化
        bool loadFromFile(const QString &configPath);
        bool saveToFile(const QString &configPath) const;

        // 配置修改
        void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }
        void setTextColor(const QColor &color) { m_textColor = color; }
        void setFontSize(int size) { m_fontSize = size; }
        void setConfidenceThreshold(double threshold) { m_confidenceThreshold = threshold; }
        void setDicomEnabled(bool enabled) { m_dicomEnabled = enabled; }

    private:
        AppConfig();
        ~AppConfig() = default;
        AppConfig(const AppConfig &) = delete;
        AppConfig &operator=(const AppConfig &) = delete;

        void loadDefaults();
        QJsonObject toJson() const;
        void fromJson(const QJsonObject &json);

        // 界面配置
        QColor m_backgroundColor;
        QColor m_textColor;
        int m_fontSize;

        // AI模型配置
        QString m_modelPath;
        QString m_classNamesPath;
        double m_confidenceThreshold;

        // 文件关联配置
        QStringList m_supportedImageFormats;
        bool m_dicomEnabled;

        // 模型路径映射
        QMap<QString, QString> m_modelPaths;
    };

} // namespace medyolo11qt::core