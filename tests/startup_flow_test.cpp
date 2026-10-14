#include "controller.h"
#include "autostart.h"
#include "guide.h"
#include "settingsdialog.h"
#include "ui.h"
#include <QApplication>
#include <QDir>
#include <QCheckBox>
#include <QDialog>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QFontDatabase>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace h2d;
namespace h2d {
class StartupFlowTestAccess {
  public:
    static void deliver(Controller &controller, const UpdateChecker::Result &result) {
        controller.startupUpdateFinished(result);
    }
    static bool pending(const Controller &controller) { return controller.pendingStartupUpdate_.has_value(); }
    static void present(Controller &controller) { controller.presentStartupUpdate(); }
    static void setAgentSession(Controller &controller, bool value) {
        controller.agentSessionId_ = value ? QStringLiteral("test-session") : QString();
    }
    static void receipt(Controller &controller) { controller.readInstallationReceipt(); }
    static void presentReceipt(Controller &controller) { controller.presentInstallationReceipt(); }
    static bool pendingReceipt(Controller &controller) { return !controller.pendingInstallationReceipt_.isEmpty(); }
};
}

class StartupFlowTests : public QObject {
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
    static UpdateChecker::Result offeredRelease(UpdateChecker::Status status = UpdateChecker::Available) {
        const QString name = QStringLiteral("EditHere-win-x64-setup.exe");
        const QUrl url(QStringLiteral("https://github.com/yyyzl/EditHere/releases/download/v0.11.0/") + name);
        return {status, QStringLiteral("发现新版本 v0.11.0。"),
                QUrl("https://github.com/yyyzl/EditHere/releases/tag/v0.11.0"), QStringLiteral("v0.11.0"),
                {name, url, 100}, {name + ".sha256", QUrl(url.toString() + ".sha256"), 64}, {}, {}, true};
    }
  private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == "offscreen") {
            const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
            QFontDatabase::addApplicationFont(fonts + "segoeui.ttf");
            QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
        }
#endif
        applyTheme(ThemeMode::Light);
    }
    void startupUpdateDefaultPreservesExplicitOptOut() {
        QTemporaryDir directory;
        const auto settingsFile = directory.filePath("settings.ini");
        QVERIFY(defaultSettings().checkUpdatesOnStartup);
        QVERIFY(loadSettings(settingsFile).checkUpdatesOnStartup);
        auto settings = quietSettings();
        QVERIFY(saveSettings(settings, nullptr, settingsFile));
        QVERIFY(!loadSettings(settingsFile).checkUpdatesOnStartup);
        Controller controller(nullptr, loadSettings(settingsFile), settingsFile);
        controller.start(false);
        QVERIFY(!controller.findChild<UpdateChecker *>("startupUpdateChecker"));
    }
    void failedInstallationReceiptIsShownOnceWithoutDiscardingWorkOrEnablingUpdates() {
        QTemporaryDir directory;
        const auto settingsFile = directory.filePath("settings.ini");
        const auto resultsPath = directory.filePath("installer-results");
        QVERIFY(QDir().mkpath(resultsPath));
        const QJsonObject receipt{{"id", "isolated-failed-installation"}, {"status", "failed"},
                                  {"version", "0.11.0"}, {"target", QCoreApplication::applicationDirPath()},
                                  {"message", "Update rolled back; recoverable files remain in the backup directory."},
                                  {"log", directory.filePath("maintenance.log")},
                                  {"journal", directory.filePath("journal.json")},
                                  {"recovery", directory.filePath("backup")}};
        QFile file(QDir(resultsPath).filePath("last.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QJsonDocument(receipt).toJson()) > 0);
        file.close();
        auto settings = quietSettings();
        QVERIFY(saveSettings(settings, nullptr, settingsFile));
        Controller controller(nullptr, settings, settingsFile);
        auto editor = editorOf(controller);
        QImage picture(120, 80, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        auto document = fromImage(picture, "file", "安装提醒时保留的工作");
        document.dirty = true;
        editor->setDocument(document);
        controller.start(false, {}, true);
        QVERIFY(!QApplication::activeModalWidget());
        StartupFlowTestAccess::setAgentSession(controller, true);
        StartupFlowTestAccess::receipt(controller);
        QVERIFY(StartupFlowTestAccess::pendingReceipt(controller));
        QVERIFY(!QApplication::activeModalWidget());
        StartupFlowTestAccess::setAgentSession(controller, false);
        bool prompted = false;
        QTimer::singleShot(0, [&] {
            auto prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(prompt && prompt->objectName() == "installationResultPrompt");
            QCOMPARE(prompt->text(), receipt.value("message").toString());
            QVERIFY(prompt->detailedText().contains(receipt.value("recovery").toString()));
            prompted = true;
            prompt->accept();
        });
        StartupFlowTestAccess::presentReceipt(controller);
        QVERIFY(prompted);
        QVERIFY(!StartupFlowTestAccess::pendingReceipt(controller));
        QCOMPARE(editor->document().id, document.id);
        QVERIFY(editor->document().dirty);
        QVERIFY(!controller.findChild<UpdateChecker *>("startupUpdateChecker"));
        QVERIFY(!loadSettings(settingsFile).checkUpdatesOnStartup);
        QSettings persisted(settingsFile, QSettings::IniFormat);
        QCOMPARE(persisted.value("updates/seenInstallerResult").toString(), receipt.value("id").toString());
        StartupFlowTestAccess::receipt(controller);
        QVERIFY(!StartupFlowTestAccess::pendingReceipt(controller));
        Controller nextLaunch(nullptr, settings, settingsFile);
        StartupFlowTestAccess::receipt(nextLaunch);
        QVERIFY(!StartupFlowTestAccess::pendingReceipt(nextLaunch));
        QVERIFY(!QApplication::activeModalWidget());
        editor->hide();
    }
    void startupUpdateDoesNotPromptWithoutANewerRelease_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("current") << int(UpdateChecker::Current);
        QTest::newRow("failed") << int(UpdateChecker::Failed);
        QTest::newRow("newer-local") << int(UpdateChecker::NewerLocal);
    }
    void startupUpdateDoesNotPromptWithoutANewerRelease() {
        QFETCH(int, status);
        QTemporaryDir directory;
        auto settings = quietSettings();
        settings.checkUpdatesOnStartup = true;
        Controller controller(nullptr, settings, directory.filePath("settings.ini"));
        StartupFlowTestAccess::deliver(controller, offeredRelease(static_cast<UpdateChecker::Status>(status)));
        QVERIFY(!StartupFlowTestAccess::pending(controller));
        QVERIFY(!QApplication::activeModalWidget());
    }
    void skippingStartupUpdatePreservesUnsavedWork_data() {
        QTest::addColumn<bool>("closeWindow");
        QTest::newRow("skip") << false;
        QTest::newRow("close") << true;
    }
    void skippingStartupUpdatePreservesUnsavedWork() {
        QFETCH(bool, closeWindow);
        QTemporaryDir directory;
        auto settings = quietSettings();
        settings.checkUpdatesOnStartup = true;
        Controller controller(nullptr, settings, directory.filePath("settings.ini"));
        auto editor = editorOf(controller);
        QVERIFY(editor);
        QImage picture(120, 80, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        auto document = fromImage(picture, "file", "尚未保存的批注");
        Note note;
        note.isGlobal = true;
        note.comment = "保留这条批注";
        document.notes.append(note);
        document.dirty = true;
        editor->setDocument(document);
        controller.start(false);
        bool prompted = false;
        QTimer::singleShot(0, [&] {
            auto prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(prompt && prompt->objectName() == "startupUpdatePrompt");
            prompted = true;
            const auto artifacts = qEnvironmentVariable("H2D_TEST_ARTIFACTS");
            if (!artifacts.isEmpty() && !closeWindow) {
                QDir().mkpath(artifacts);
                QVERIFY(prompt->grab().save(QDir(artifacts).filePath("startup-update-prompt.png")));
            }
            if (closeWindow)
                prompt->close();
            else
                for (auto button : prompt->buttons())
                    if (prompt->buttonRole(button) == QMessageBox::RejectRole) button->click();
        });
        StartupFlowTestAccess::deliver(controller, offeredRelease());
        QVERIFY(prompted);
        QVERIFY(!StartupFlowTestAccess::pending(controller));
        QCOMPARE(editor->document().id, document.id);
        QVERIFY(editor->document().notes == document.notes);
        QVERIFY(editor->document().dirty);
        controller.start(false);
        StartupFlowTestAccess::present(controller);
        QCOMPARE(controller.findChildren<UpdateChecker *>("startupUpdateChecker").size(), 1);
        QVERIFY(!QApplication::activeModalWidget());
        editor->hide();
    }
    void acceptingStartupUpdateUsesCheckedReleaseAndCanCancel() {
        QTemporaryDir directory;
        auto settings = quietSettings();
        settings.checkUpdatesOnStartup = true;
        Controller controller(nullptr, settings, directory.filePath("settings.ini"));
        auto editor = editorOf(controller);
        QVERIFY(editor);
        QImage picture(120, 80, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        auto document = fromImage(picture, "file", "仍需保留的工作");
        document.dirty = true;
        editor->setDocument(document);
        bool offered = false;
        QTimer::singleShot(0, [&] {
            auto prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(prompt);
            // Inspect and cancel before the queued download starts: no network or
            // real installer is needed to verify the startup-to-settings handoff.
            QTimer::singleShot(0, [&] {
                auto dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                QCOMPARE(dialog->findChild<QLabel *>("updateStatus")->text(), offeredRelease().message);
                QVERIFY(dialog->findChild<QPushButton *>("openReleases")->property("updateAvailable").toBool());
                auto updater = dialog->findChild<UpdateChecker *>();
                QVERIFY(updater && !updater->busy() && !updater->downloading());
                offered = true;
                dialog->findChild<QPushButton *>("settingsCancel")->click();
            });
            for (auto button : prompt->buttons())
                if (prompt->buttonRole(button) == QMessageBox::AcceptRole) button->click();
        });
        StartupFlowTestAccess::deliver(controller, offeredRelease());
        QVERIFY(offered);
        QCOMPARE(editor->document().id, document.id);
        QVERIFY(editor->document().dirty);
        editor->hide();
    }
    void settingsGuideEntryKeepsDocumentAndDiscardsDraft() {
        QTemporaryDir directory;
        const auto stateFile = directory.filePath("settings.ini");
        auto settings = quietSettings();
        QVERIFY(saveSettings(settings, nullptr, stateFile));
        Controller controller(nullptr, settings, stateFile);
        auto editor = editorOf(controller);
        QImage original(640, 480, QImage::Format_ARGB32);
        original.fill(Qt::white);
        auto document = fromImage(original, "test", "用户正在编辑的截图");
        Note note;
        note.isGlobal = true;
        note.comment = "界面更轻盈，色彩更克制。";
        document.notes.append(note);
        document.dirty = true;
        editor->setDocument(document);
        controller.showGuide();
        editor->findChild<QPushButton *>("guideNext")->click();
        QCOMPARE(editor->findChild<GuideOverlay *>()->currentStep(), 1);
        bool entryClicked = false;
        QTimer::singleShot(30, [&] {
            auto dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto draft = dialog->findChild<QCheckBox *>("fitImageOnOpen");
            auto guide = dialog->findChild<QPushButton *>("restartGuide");
            QVERIFY(draft && guide);
            connect(dialog, &SettingsDialog::guideRequested, dialog, [&] { entryClicked = true; });
            QTimer::singleShot(800, dialog, &QDialog::reject);
            QTest::mouseClick(guide, Qt::LeftButton);
        });
        controller.openSettings();
        QVERIFY(entryClicked);
        QTRY_VERIFY(editor->guideActive());
        QCOMPARE(editor->findChild<GuideOverlay *>()->currentStep(), 0);
        QCOMPARE(editor->document().id, document.id);
        QCOMPARE(editor->document().png, document.png);
        QVERIFY(editor->document().notes == document.notes);
        QVERIFY(editor->document().dirty);
        editor->dismissGuide();
        QVERIFY(hasSeenGuide(stateFile));
        editor->hide();
    }

    void manualStartAndRepeatedActivationKeepWhiteboard() {
        Controller controller(nullptr, quietSettings());
        controller.start(false);
        auto editor = editorOf(controller);
        QVERIFY(editor && editor->isVisible());
        QVERIFY(!editor->hasDocument());
        QVERIFY(!editor->guideActive());
        const auto artifacts = qEnvironmentVariable("H2D_TEST_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QVERIFY(QDir().mkpath(artifacts));
            QVERIFY(editor->grab().save(QDir(artifacts).filePath("whiteboard-empty.png")));
        }
        QVERIFY(!editor->findChild<QPushButton *>("captureImage"));
        auto tray = controller.findChild<QSystemTrayIcon *>("edithereTray");
        QVERIFY(tray && !tray->contextMenu()->findChild<QAction *>("trayCapture"));
        QImage image(120, 80, QImage::Format_ARGB32);
        image.fill(Qt::blue);
        editor->setDocument(fromImage(image, "clipboard", "待批注图片"));
        editor->hide();
        controller.activate();
        QVERIFY(editor->isVisible());
        QCOMPARE(editor->document().image, image);
        controller.start(false);
        QCOMPARE(editor->document().image, image);
        QVERIFY(editor->hasUnsavedChanges());
    }
    void backgroundStartWaitsForManualActivation() {
        Controller controller(nullptr, quietSettings());
        controller.start(false, {}, true);
        auto editor = editorOf(controller);
        QVERIFY(editor && !editor->isVisible());
        controller.activate();
        QVERIFY(editor->isVisible());
        QVERIFY(!editor->hasDocument());
    }
    void newWhiteboardPreservesActiveAgentSession() {
        QTemporaryDir directory;
        QImage image(100, 60, QImage::Format_ARGB32);
        image.fill(Qt::green);
        const auto input = directory.filePath("input.png");
        QVERIFY(image.save(input));
        Controller controller(nullptr, quietSettings());
        const auto started = controller.handleAgentRequest({{"command", "annotate"}, {"input", input},
            {"output", directory.filePath("feedback.json")}, {"timeout", 30}});
        QVERIFY(started["pending"].toBool());
        auto editor = editorOf(controller);
        auto tray = controller.findChild<QSystemTrayIcon *>("edithereTray");
        tray->contextMenu()->findChild<QAction *>("trayAnnotate")->trigger();
        QVERIFY(editor->hasDocument());
        QCOMPARE(editor->document().image, image);
        QVERIFY(controller.handleAgentRequest({{"command", "status"}})["agentSession"].toBool());
        const auto rejected = controller.handleAgentRequest({{"command", "capture"}});
        QVERIFY(!rejected["ok"].toBool());
        controller.cancelAgentSession(started["session"].toString(), "cancelled", "测试结束");
    }

};
QTEST_MAIN(StartupFlowTests)
#include "startup_flow_test.moc"
