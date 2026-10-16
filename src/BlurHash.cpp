#include "BlurHash.h"
#include <array>
#include <cmath>
#include <vector>

namespace {
int base83(QStringView value) {
    static const QString alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";
    int result = 0;
    for (const auto c : value) {
        const int digit = alphabet.indexOf(c);
        if (digit < 0) return -1;
        result = result * 83 + digit;
    }
    return result;
}
double linear(int v) { const double s = v / 255.0; return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); }
int srgb(double v) {
    v = qBound(0.0, v, 1.0);
    return qRound(255 * (v <= 0.0031308 ? v * 12.92 : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055));
}
}
QImage BlurHashProvider::decode(const QString &hash, QSize size) {
    if (hash.size() < 6 || hash.size() > 166) return {};
    const QStringView view(hash);
    const int flag = base83(view.first(1));
    if (flag < 0 || flag > 80) return {};
    const int nx = flag % 9 + 1, ny = flag / 9 + 1;
    if (hash.size() != 4 + 2 * nx * ny) return {};
    const int quantized = base83(view.mid(1, 1)), dc = base83(view.mid(2, 4));
    if (quantized < 0 || dc < 0 || dc > 0xffffff) return {};
    const double maximum = (quantized + 1) / 166.0;
    std::vector<std::array<double, 3>> coefficients{{linear(dc >> 16), linear((dc >> 8) & 255), linear(dc & 255)}};
    for (int i = 1; i < nx * ny; ++i) {
        const int value = base83(view.mid(4 + i * 2, 2));
        if (value < 0 || value >= 19 * 19 * 19) return {};
        std::array<double, 3> c;
        const int channels[] = {value / (19 * 19), value / 19 % 19, value % 19};
        for (int j = 0; j < 3; ++j) { const double v = (channels[j] - 9) / 9.0; c[j] = std::copysign(v * v, v) * maximum; }
        coefficients.push_back(c);
    }
    size = QSize(qBound(1, size.width(), 64), qBound(1, size.height(), 64));
    QImage image(size, QImage::Format_RGB32);
    for (int y = 0; y < size.height(); ++y) for (int x = 0; x < size.width(); ++x) {
        std::array<double, 3> color{};
        for (int j = 0; j < ny; ++j) for (int i = 0; i < nx; ++i) {
            const double basis = std::cos(M_PI * x * i / size.width()) * std::cos(M_PI * y * j / size.height());
            for (int c = 0; c < 3; ++c) color[c] += coefficients[size_t(i + j * nx)][c] * basis;
        }
        image.setPixel(x, y, qRgb(srgb(color[0]), srgb(color[1]), srgb(color[2])));
    }
    return image;
}
QImage BlurHashProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const auto image = decode(QString::fromLatin1(QByteArray::fromHex(id.toLatin1())), requestedSize.isValid() ? requestedSize : QSize(32, 24));
    if (size) *size = image.size();
    return image;
}
