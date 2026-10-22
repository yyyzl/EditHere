#pragma once
#include <QByteArray>
#include <QFile>
#include <QtEndian>

namespace h2d::test {
// A small, silent, uncompressed AVI keeps playback regressions independent of
// user media, external encoders and EDITHERE_VIDEO_TEST_SOURCE.
inline bool writeSilentVideoFixture(const QString &path) {
    constexpr quint32 width = 80, height = 60, fps = 10, frames = 100;
    constexpr quint32 frameBytes = width * height * 3;
    const auto put32 = [](QByteArray &data, int offset, quint32 value) {
        qToLittleEndian(value, data.data() + offset);
    };
    const auto chunk = [&](const char *tag, const QByteArray &data) {
        QByteArray result(tag, 4);
        result.append(4, '\0');
        put32(result, 4, quint32(data.size()));
        result.append(data);
        if (data.size() % 2) result.append('\0');
        return result;
    };
    QByteArray header(56, '\0');
    put32(header, 0, 1000000 / fps);
    put32(header, 4, frameBytes * fps);
    put32(header, 16, frames);
    put32(header, 24, 1);
    put32(header, 28, frameBytes);
    put32(header, 32, width);
    put32(header, 36, height);
    QByteArray stream(56, '\0');
    stream.replace(0, 4, "vids");
    stream.replace(4, 4, "DIB ");
    put32(stream, 20, 1);
    put32(stream, 24, fps);
    put32(stream, 32, frames);
    put32(stream, 36, frameBytes);
    put32(stream, 40, 0xffffffff);
    qToLittleEndian<quint16>(width, stream.data() + 52);
    qToLittleEndian<quint16>(height, stream.data() + 54);
    QByteArray bitmap(40, '\0');
    put32(bitmap, 0, 40);
    put32(bitmap, 4, width);
    put32(bitmap, 8, height);
    qToLittleEndian<quint16>(1, bitmap.data() + 12);
    qToLittleEndian<quint16>(24, bitmap.data() + 14);
    put32(bitmap, 20, frameBytes);
    const auto streamList = chunk("LIST", QByteArray("strl", 4) + chunk("strh", stream) + chunk("strf", bitmap));
    const auto headers = chunk("LIST", QByteArray("hdrl", 4) + chunk("avih", header) + streamList);
    QByteArray movie("movi", 4);
    movie.reserve(4 + frames * (frameBytes + 8));
    for (quint32 index = 0; index < frames; ++index)
        movie.append(chunk("00db", QByteArray(frameBytes, char(index * 5 % 256))));
    const auto bytes = chunk("RIFF", QByteArray("AVI ", 4) + headers + chunk("LIST", movie));
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.flush();
}
} // namespace h2d::test
