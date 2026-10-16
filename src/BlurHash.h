#pragma once
#include <QQuickImageProvider>

class BlurHashProvider : public QQuickImageProvider {
public:
    BlurHashProvider() : QQuickImageProvider(Image, ForceAsynchronousImageLoading) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    static QImage decode(const QString &hash, QSize size = QSize(32, 24));
};
