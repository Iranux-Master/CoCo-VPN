#include "qrCodeUtils.h"

#include <cmath>

#include <QDataStream>
#include <QIODevice>
#include <QList>

QList<QString> qrCodeUtils::generateQrCodeImageSeries(const QByteArray &data)
{
    constexpr int chunkSize = 850;
    constexpr int maxQrChunks = 64;

    const int chunksCount = static_cast<int>(std::ceil(data.size() / static_cast<double>(chunkSize)));
    // Very large admin configurations can otherwise generate hundreds of SVG QR images
    // synchronously on the UI thread. Let the user share the config file instead.
    if (chunksCount > maxQrChunks) {
        return {};
    }
    QList<QString> chunks;
    for (int i = 0; i < data.size(); i += chunkSize) {
        QByteArray chunk;
        QDataStream s(&chunk, QIODevice::WriteOnly);
        s << qrCodeUtils::qrMagicCode << static_cast<quint8>(chunksCount)
          << static_cast<quint8>(i / chunkSize) << data.mid(i, chunkSize);

        QByteArray ba = chunk.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(ba.constData(), qrcodegen::QrCode::Ecc::LOW);
        QString svg = QString::fromStdString(toSvgString(qr, 1));
        chunks.append(svgToBase64(svg));
    }

    return chunks;
}

QString qrCodeUtils::generatePlainQrCodeImage(const QByteArray &data)
{
    try {
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(data.constData(), qrcodegen::QrCode::Ecc::LOW);
        QString svg = QString::fromStdString(toSvgString(qr, 1));
        return svgToBase64(svg);
    } catch (const qrcodegen::data_too_long &) {
        return {};
    }
}

QString qrCodeUtils::svgToBase64(const QString &image)
{
    return "data:image/svg+xml;base64," + QString::fromLatin1(image.toUtf8().toBase64().data());
}

qrcodegen::QrCode qrCodeUtils::generateQrCode(const QByteArray &data)
{
    return qrcodegen::QrCode::encodeText(data.constData(), qrcodegen::QrCode::Ecc::LOW);
}

