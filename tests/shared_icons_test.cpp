#include "ui.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QSet>
#include <QTest>

using namespace h2d;

static bool hasInk(const QImage &image) {
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y).alpha() > 0)
                return true;
    return false;
}

static QColor opaqueInk(const QImage &image) {
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y).alpha() == 255)
                return image.pixelColor(x, y);
    return {};
}

class SharedIconsTest : public QObject {
    Q_OBJECT
  private slots:
    void registeredAssetsRenderAtToolbarSizesAndDisplayScales() {
        const auto names = glyphNames(); // Also forces static resource registration.
        QVERIFY(!names.isEmpty());
        QFile manifest(":/ui-icons/manifest.json");
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        QSet<QString> registered;
        const auto icons = QJsonDocument::fromJson(manifest.readAll()).object()["icons"].toArray();
        for (const auto &entry : icons) {
            const auto object = entry.toObject();
            const auto name = object["id"].toString();
            QVERIFY2(!registered.contains(name), qPrintable(name));
            registered.insert(name);
            QVERIFY2(QFile::exists(":/ui-icons/" + object["file"].toString()), qPrintable(name));
        }
        QCOMPARE(registered, QSet<QString>(names.cbegin(), names.cend()));
        for (const auto &name : names) {
            const auto icon = glyph(name);
            QVERIFY2(!icon.isNull(), qPrintable(name));
            QCOMPARE(icon.name(), name);
            for (const int size : {16, 20, 24, 48}) {
                for (const qreal scale : {1., 1.25, 2., 3.}) {
                    const auto pixmap = icon.pixmap(QSize(size, size), scale);
                    QCOMPARE(pixmap.size(), QSize(size, size) * scale);
                    QCOMPARE(pixmap.devicePixelRatio(), scale);
                    QVERIFY2(hasInk(pixmap.toImage()), qPrintable(name));
                }
            }
        }
    }

    void outputActionsHaveDistinctSilhouettes() {
        const QStringList names{"save", "image-save", "feedback-save", "json",
                                "json-copy-file", "json-copy-text", "image-copy"};
        for (int a = 0; a < names.size(); ++a)
            for (int b = a + 1; b < names.size(); ++b)
                QVERIFY2(glyph(names[a]).pixmap(24).toImage() != glyph(names[b]).pixmap(24).toImage(),
                         qPrintable(names[a] + " / " + names[b]));
    }

    void themesCheckedDisabledAndExplicitColors() {
        for (const auto theme : {ThemeMode::Light, ThemeMode::Dark}) {
            applyTheme(theme);
            const auto icon = glyph("save");
            QCOMPARE(opaqueInk(icon.pixmap(48, QIcon::Normal, QIcon::Off).toImage()),
                     QColor(theme == ThemeMode::Dark ? "#d4d5dd" : "#606069"));
            QCOMPARE(opaqueInk(icon.pixmap(48, QIcon::Normal, QIcon::On).toImage()), accent());
            QCOMPARE(opaqueInk(icon.pixmap(48, QIcon::Disabled).toImage()),
                     qApp->palette().color(QPalette::Disabled, QPalette::ButtonText));
            const auto white = glyph("save", Qt::white);
            QCOMPARE(opaqueInk(white.pixmap(48, QIcon::Normal, QIcon::On).toImage()), QColor(Qt::white));
            const auto red = glyph("note-add", QColor("#bd252f"));
            QCOMPARE(opaqueInk(red.pixmap(48).toImage()), QColor("#bd252f"));
        }
    }

    void unknownIdFailsExplicitly() {
        QTest::ignoreMessage(QtWarningMsg, "Unknown shared UI icon: \"missing-action-for-test\"");
        QVERIFY(glyph("missing-action-for-test").isNull());
    }
};

QTEST_MAIN(SharedIconsTest)
#include "shared_icons_test.moc"
