#include <QCoreApplication>
#include <QFile>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QRandomGenerator>
#include <QDebug>

// 简单的XOR加密函数
QByteArray xorEncrypt(const QByteArray &data, const QByteArray &key)
{
    QByteArray result;
    result.resize(data.size());

    for (int i = 0; i < data.size(); ++i)
    {
        result[i] = data[i] ^ key[i % key.size()];
    }

    return result;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc != 3)
    {
        qDebug() << "Usage: encrypt_model <input.onnx> <output.encrypted>";
        return 1;
    }

    QString inputFile = argv[1];
    QString outputFile = argv[2];

    // 读取原始模型文件
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open input file:" << inputFile;
        return 1;
    }

    QByteArray modelData = file.readAll();
    file.close();

    // 使用程序特定的密钥
    QByteArray key = QCryptographicHash::hash(
        "MedYOLO11Qt_Model_Protection_Key_2024",
        QCryptographicHash::Sha256);

    // 使用XOR加密（简单但有效）
    QByteArray encryptedData = xorEncrypt(modelData, key);

    // 添加文件头标识（4字节魔数 + 4字节版本）
    QByteArray header;
    QDataStream headerStream(&header, QIODevice::WriteOnly);
    headerStream << quint32(0x4D594F4C); // "MYOL" 魔数
    headerStream << quint32(0x00010000); // 版本 1.0.0

    // 保存加密后的文件（包含文件头）
    QFile outFile(outputFile);
    if (!outFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "Failed to create output file:" << outputFile;
        return 1;
    }

    outFile.write(header);
    outFile.write(encryptedData);
    outFile.close();

    qDebug() << "Model encrypted successfully!";
    qDebug() << "Original size:" << modelData.size() << "bytes";
    qDebug() << "Encrypted size:" << encryptedData.size() + 8 << "bytes";

    return 0;
}