#include "ui.h"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QTextStream>

// A visual review utility, deliberately separate from geometry assertions.
// It draws the icons used by the application at their real toolbar sizes.
int main(int argc, char **argv) {
    QApplication application(argc, argv);
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        if (QFontDatabase::addApplicationFont(fonts + "segoeui.ttf") < 0 ||
            QFontDatabase::addApplicationFont(fonts + "msyh.ttc") < 0) {
            QTextStream(stderr) << "Could not load the Windows UI fonts.\n";
            return 5;
        }
    }
#endif
    const QDir output(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("icon-gallery"));
    if (!QDir().mkpath(output.absolutePath())) return 1;
    const QStringList names = h2d::glyphNames();
    constexpr int columns = 8, cellWidth = 148, cellHeight = 106, top = 48;
    const int rows = (names.size() + columns - 1) / columns;
    for (const auto &theme : {h2d::ThemeMode::Light, h2d::ThemeMode::Dark}) {
        h2d::applyTheme(theme);
        const bool dark = theme == h2d::ThemeMode::Dark;
        const QColor face(dark ? "#202126" : "#fbfbfd");
        const QColor ink(dark ? "#ededf2" : "#232326");
        const QColor muted(dark ? "#a0a2ae" : "#898990");
        QImage sheet(columns * cellWidth, top + rows * cellHeight, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(face);
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(ink);
        painter.drawText(QRect(16, 8, sheet.width() - 32, 32), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("EditHere icons  |  20 px / 24 px / 2x detail  |  %1")
                             .arg(dark ? "Dark" : "Light"));
        for (int index = 0; index < names.size(); ++index) {
            const QPoint origin((index % columns) * cellWidth, top + (index / columns) * cellHeight);
            auto icon = h2d::glyph(names[index]);
            const auto image = icon.pixmap(24, 24).toImage();
            bool visible = false;
            for (int y = 0; y < image.height() && !visible; ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (image.pixelColor(x, y).alpha() > 0) {
                        visible = true;
                        break;
                    }
            if (!visible) {
                QTextStream(stderr) << "Empty glyph: " << names[index] << '\n';
                return 4;
            }
            for (const auto [x, size] : {std::pair{15, 20}, {48, 24}, {82, 48}}) {
                icon.paint(&painter, QRect(origin + QPoint(x, 14), QSize(size, size)));
            }
            painter.setPen(ink);
            painter.drawText(QRect(origin + QPoint(0, 70), QSize(cellWidth, 24)), Qt::AlignCenter, names[index]);
            painter.setPen(muted);
            painter.drawLine(origin + QPoint(8, 100), origin + QPoint(cellWidth - 8, 100));
        }
        painter.end();
        const auto suffix = dark ? QStringLiteral("dark") : QStringLiteral("light");
        if (!sheet.save(output.filePath(QStringLiteral("icons-%1.png").arg(suffix)))) return 2;
        const QStringList primaryNames{"point", "image-save", "settings", "open", "explode"};
        const QStringList primaryLabels{QStringLiteral("点批注"), QStringLiteral("保存图片"),
                                        QStringLiteral("设置"), QStringLiteral("打开文件"),
                                        QStringLiteral("布局调整")};
        QImage primary(5 * cellWidth, cellHeight, QImage::Format_ARGB32_Premultiplied);
        primary.fill(face);
        QPainter primaryPainter(&primary);
        for (int index = 0; index < primaryNames.size(); ++index) {
            const int x = index * cellWidth;
            const auto icon = h2d::glyph(primaryNames[index]);
            icon.paint(&primaryPainter, QRect(x + 28, 18, 20, 20));
            icon.paint(&primaryPainter, QRect(x + 59, 16, 24, 24));
            primaryPainter.setPen(ink);
            primaryPainter.drawText(QRect(x, 53, cellWidth, 24), Qt::AlignCenter, primaryLabels[index]);
            primaryPainter.setPen(muted);
            primaryPainter.drawText(QRect(x, 78, cellWidth, 20), Qt::AlignCenter, QStringLiteral("20 px / 24 px"));
        }
        primaryPainter.end();
        if (!primary.save(output.filePath(QStringLiteral("icons-main-%1.png").arg(suffix)))) return 2;


    }
    QTextStream(stdout) << output.absolutePath() << '\n';
    return 0;
}
