#pragma once

#include <QObject>
#include <QMap>
#include <QSet>

#include "InferenceEngine.h"

class ModelManager : public QObject
{
    Q_OBJECT
public:
    enum class ModelType
    {
        FAI_XRay,
        MRI_Segmentation
    };
    Q_ENUM(ModelType)

    explicit ModelManager(QObject *parent = nullptr);

    void setRootDirectory(const QString &dir);
    QString rootDirectory() const { return m_rootDir; }

    void setModelFileName(ModelType type, const QString &fileName);
    QString modelFileName(ModelType type) const;

    void setModelPath(ModelType type, const QString &absolutePath);
    QString modelPath(ModelType type) const;

    bool loadModel(ModelType type, InferenceEngine &engine);
    bool isModelLoaded(ModelType type) const { return m_loadedTypes.contains(type); }
    QString loadedModelPath(ModelType type) const { return m_loadedPaths.value(type); }

    void resetModel(ModelType type);

signals:
    void modelLoaded(ModelType type, const QString &path);
    void modelLoadFailed(ModelType type, const QString &path, const QString &reason);

private:
    static InferenceEngine::Task toTask(ModelType type);
    QString resolvePath(ModelType type) const;

    QString m_rootDir;
    QMap<ModelType, QString> m_defaultFiles;
    QMap<ModelType, QString> m_customPaths;
    QSet<ModelType> m_loadedTypes;
    QMap<ModelType, QString> m_loadedPaths;
};

Q_DECLARE_METATYPE(ModelManager::ModelType)
