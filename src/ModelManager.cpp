#include "ModelManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
    QString defaultModelsDirectory()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString bundled = appDir + "/models";
        if (QFileInfo::exists(bundled))
            return bundled;

        const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (!documents.isEmpty())
        {
            const QString fallback = documents + "/MedYOLO11Qt/models";
            if (QFileInfo::exists(fallback))
                return fallback;
        }
        return appDir;
    }
}

ModelManager::ModelManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ModelManager::ModelType>();
    m_rootDir = defaultModelsDirectory();
    m_defaultFiles[ModelType::FAI_XRay] = "fai_xray.onnx";
    m_defaultFiles[ModelType::MRI_Segmentation] = "mri_segmentation.onnx";
}

void ModelManager::setRootDirectory(const QString &dir)
{
    if (dir.isEmpty())
        return;
    m_rootDir = dir;
}

void ModelManager::setModelFileName(ModelType type, const QString &fileName)
{
    if (!fileName.isEmpty())
        m_defaultFiles[type] = fileName;
}

QString ModelManager::modelFileName(ModelType type) const
{
    return m_defaultFiles.value(type);
}

void ModelManager::setModelPath(ModelType type, const QString &absolutePath)
{
    m_customPaths[type] = absolutePath;
}

QString ModelManager::modelPath(ModelType type) const
{
    const QString custom = m_customPaths.value(type);
    if (!custom.isEmpty())
        return custom;
    return resolvePath(type);
}

bool ModelManager::loadModel(ModelType type, InferenceEngine &engine)
{
    const QString path = modelPath(type);
    if (path.isEmpty())
    {
        emit modelLoadFailed(type, path, tr("模型路径为空"));
        return false;
    }

    if (!QFileInfo::exists(path))
    {
        emit modelLoadFailed(type, path, tr("模型文件不存在"));
        return false;
    }

    const bool ok = engine.loadModel(path, toTask(type));
    if (!ok)
    {
        emit modelLoadFailed(type, path, tr("模型加载失败"));
        return false;
    }

    m_loadedTypes.insert(type);
    m_loadedPaths[type] = path;
    emit modelLoaded(type, path);
    return true;
}

void ModelManager::resetModel(ModelType type)
{
    m_loadedTypes.remove(type);
    m_loadedPaths.remove(type);
}

InferenceEngine::Task ModelManager::toTask(ModelType type)
{
    switch (type)
    {
    case ModelType::MRI_Segmentation:
        return InferenceEngine::Task::HipMRI_Seg;
    case ModelType::FAI_XRay:
    default:
        return InferenceEngine::Task::FAI_XRay;
    }
}

QString ModelManager::resolvePath(ModelType type) const
{
    const QString fileName = m_defaultFiles.value(type);
    if (fileName.isEmpty())
        return QString();

    QDir root(m_rootDir);
    return root.absoluteFilePath(fileName);
}
