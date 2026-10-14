#include "controller.h"
#include <QApplication>
#include <QMenu>
#include <QTest>
#import <AppKit/AppKit.h>

using namespace h2d;

class MacOSDockReopenTests : public QObject {
    Q_OBJECT
    static AppSettings quietSettings() {
        auto settings = defaultSettings();
        settings.checkUpdatesOnStartup = false;
        return settings;
    }
    static Editor *editorOf(Controller &controller) {
        auto tray = controller.findChild<QSystemTrayIcon *>("edithereTray");
        return tray && tray->contextMenu() ? qobject_cast<Editor *>(tray->contextMenu()->parentWidget()) : nullptr;
    }
  private slots:
    void initTestCase() {
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("cocoa"));
        QVERIFY(NSApp.delegate);
        QVERIFY([NSApp.delegate respondsToSelector:@selector(applicationShouldHandleReopen:hasVisibleWindows:)]);
        qApp->setQuitOnLastWindowClosed(false);
    }
    void dockReopenRestoresWindowWhileApplicationIsAlreadyActive() {
        Controller controller(nullptr, quietSettings());
        controller.start(false);
        auto editor = editorOf(controller);
        QVERIFY(editor && editor->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(editor));
        for (int attempt = 0; attempt < 3; ++attempt) {
            editor->close();
            QCoreApplication::processEvents();
            QVERIFY(!editor->isVisible());
            // 直接调用 Dock 使用的 AppKit 入口，覆盖 Cocoa → Qt → 白板恢复的完整路径。
            [NSApp.delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
            QTRY_VERIFY_WITH_TIMEOUT(editor->isVisible(), 3000);
            QVERIFY(!editor->hasDocument());
            QVERIFY(!editor->guideActive());
        }
    }
    void initialNativeActivationKeepsBackgroundLaunchHidden() {
        Controller controller(nullptr, quietSettings());
        controller.start(false, {}, true);
        auto editor = editorOf(controller);
        QVERIFY(editor && !editor->isVisible());
        auto notification = [NSNotification notificationWithName:NSApplicationDidBecomeActiveNotification object:NSApp];
        [NSApp.delegate applicationDidBecomeActive:notification];
        QCoreApplication::processEvents();
        QVERIFY(!editor->isVisible());
    }
};

QTEST_MAIN(MacOSDockReopenTests)
#include "macos_dock_reopen_test.moc"
