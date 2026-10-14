#include "settings.h"
#include "settingsdialog.h"
#include "diagnostics.h"
#include "ui.h"
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <algorithm>
using namespace h2d;
class SettingsTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == "offscreen") {
            const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
            QFontDatabase::addApplicationFont(fonts + "segoeui.ttf");
            QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
        }
#endif
        applyTheme(ThemeMode::Light);
    }
    void diagnosticsAreAccessibleWithoutChangingSettings() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        diagnostics::Options options;
        options.directory = directory.path();
        QVERIFY(diagnostics::start(options));
        struct StopLog { ~StopLog() { diagnostics::stop(); } } stop;
        diagnostics::write(diagnostics::Level::Error, "test.save", "Synthetic save failure");
        auto original = defaultSettings();
        SettingsDialog dialog(original);
        auto page = dialog.findChild<QWidget *>("settingsDiagnosticsPage");
        auto tabs = dialog.findChild<QTabWidget *>("settingsTabs");
        QVERIFY(page && tabs);
        tabs->setCurrentWidget(page);
        dialog.show();
        QTest::qWait(10);
        auto exportButton = page->findChild<QPushButton *>("exportDiagnostics");
        auto folderButton = page->findChild<QPushButton *>("openDiagnosticsFolder");
        QVERIFY(exportButton && exportButton->isEnabled());
        QVERIFY(folderButton && folderButton->isEnabled());
        QCOMPARE(page->findChild<QLabel *>("diagnosticsPath")->text(), QDir::toNativeSeparators(directory.path()));
        QVERIFY(dialog.settings() == original);
        const auto reportPath = directory.filePath("user-feedback.txt");
        bool selected = false;
        QTimer chooser;
        connect(&chooser, &QTimer::timeout, &dialog, [&] {
            if (auto fileDialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()); fileDialog && !selected) {
                selected = true;
                auto filename = fileDialog->findChild<QLineEdit *>("fileNameEdit");
                QVERIFY(filename);
                filename->setText(reportPath);
                QMetaObject::invokeMethod(fileDialog, "accept", Qt::DirectConnection);
            }
        });
        chooser.start(10);
        exportButton->click();
        chooser.stop();
        QVERIFY(selected);
        QFile report(reportPath);
        QVERIFY(report.open(QIODevice::ReadOnly));
        const auto contents = report.readAll();
        QVERIFY(contents.contains("Synthetic save failure"));
        QVERIFY(contents.contains("Environment:"));
        QVERIFY(page->findChild<QLabel *>("diagnosticsStatus")->text().contains("日志已导出"));
        const auto artifact = qEnvironmentVariable("H2D_TEST_ARTIFACTS");
        if (!artifact.isEmpty()) {
            QDir().mkpath(artifact);
            for (auto mode : {ThemeMode::Light, ThemeMode::Dark}) {
                applyTheme(mode);
                QTest::qWait(10);
                QVERIFY(dialog.grab().save(QDir(artifact).filePath(mode == ThemeMode::Light ? "diagnostics-light.png" : "diagnostics-dark.png")));
            }
            applyTheme(ThemeMode::Light);
        }
        dialog.reject();
        QVERIFY(dialog.settings() == original);
    }
    void toolbarValidationPersistenceAndMigration() {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.ini");
        auto settings = defaultSettings();
        QCOMPARE(settings.toolbarActions.size(), 6);
        for (const auto &id : {"saveProject", "saveImage", "exportJson", "copyJson", "copyImage"})
            QVERIFY(settings.toolbarActions.contains(id));
        QVERIFY(!settings.toolbarActions.contains("capture"));
        QVERIFY(!settings.toolbarActions.contains("fit"));
        for (const auto &id : {"copyJson", "saveImage", "hideAnnotations", "addGlobalNote"}) {
            QVERIFY(settings.shortcuts.contains(id));
            QVERIFY(settings.shortcuts.value(id).isEmpty());
        }
        settings.theme = ThemeMode::Dark;
        settings.toolbarActions = {"fit", "copyJson"};
        QString error;
        QVERIFY2(saveSettings(settings, &error, path), qPrintable(error));
        QVERIFY(loadSettings(path) == settings);
        settings.toolbarActions.clear();
        QVERIFY2(saveSettings(settings, &error, path), qPrintable(error));
        QVERIFY(loadSettings(path) == settings);
        settings.toolbarActions = {"saveProject", "saveProject"};
        QVERIFY(!validateSettings(settings).isEmpty());
        QVERIFY(!saveSettings(settings, &error, path));
        QVERIFY(loadSettings(path).toolbarActions.isEmpty());
        settings.toolbarActions = {"copyImage", "unrecognized"};
        QVERIFY(!validateSettings(settings).isEmpty());
        QVERIFY(!saveSettings(settings, &error, path));
        // A damaged toolbar preference must not erase the user's other preferences.
        QSettings source(path, QSettings::IniFormat);
        source.setValue("toolbar/actions", QStringList{"copyImage", "unrecognized"});
        source.sync();
        auto recovered = loadSettings(path);
        QCOMPARE(recovered.theme, ThemeMode::Dark);
        QCOMPARE(recovered.toolbarActions, defaultSettings().toolbarActions);
        source.setValue("toolbar/actions", QStringList{"copyJson", "copyJson"});
        source.sync();
        QCOMPARE(loadSettings(path).toolbarActions, defaultSettings().toolbarActions);
        // Existing users without the preference get all five actions on upgrade.
        source.remove("toolbar/actions");
        source.sync();
        QCOMPARE(loadSettings(path).toolbarActions, defaultSettings().toolbarActions);
        // Renaming exportJson must preserve the former selection and Ctrl+E binding.
        const QStringList legacyActions{"saveProject", "saveImage", "copyJson", "exportJson", "copyImage"};
        source.setValue("toolbar/actions", legacyActions);
        source.setValue("shortcuts/export", "Ctrl+E");
        source.sync();
        auto migrated = loadSettings(path);
        QCOMPARE(migrated.toolbarActions, legacyActions);
        QCOMPARE(migrated.shortcuts.value("export"), QKeySequence("Ctrl+E"));
        QVERIFY2(saveSettings(migrated, &error, path), qPrintable(error));
        QVERIFY(loadSettings(path) == migrated);
    }
    void toolbarDraftAndUpdateNavigation() {
        SettingsDialog dialog(defaultSettings());
        auto tabs = dialog.findChild<QTabWidget *>("settingsTabs");
        QVERIFY(tabs);
        dialog.showToolbar();
        QCOMPARE(tabs->currentWidget()->objectName(), QString("settingsToolbarPage"));
        for (const auto &definition : toolbarActionDefinitions()) {
            auto checkbox = dialog.findChild<QCheckBox *>("toolbar_" + definition.id);
            QVERIFY(checkbox);
            QCOMPARE(checkbox->isChecked(), defaultSettings().toolbarActions.contains(definition.id));
            checkbox->setChecked(false);
        }
        QVERIFY(dialog.settings().toolbarActions.isEmpty());
        dialog.findChild<QCheckBox *>("toolbar_copyJson")->setChecked(true);
        QCOMPARE(dialog.settings().toolbarActions, QStringList{"copyJson"});
        dialog.findChild<QCheckBox *>("toolbar_fit")->setChecked(true);
        QCOMPARE(dialog.settings().toolbarActions, (QStringList{"copyJson", "fit"}));
        QVERIFY(validateSettings(dialog.settings()).isEmpty());
        dialog.showUpdates();
        QCOMPARE(tabs->currentWidget()->objectName(), QString("settingsAboutPage"));
        // The update page carries the feedback group, so users who cannot update
        // still have a way to report the problem.
        auto feedback = dialog.findChild<QLabel *>("aboutFeedback");
        QVERIFY(feedback);
        QVERIFY(feedback->text().contains("1018416966"));
        dialog.findChild<QPushButton *>("settingsReset")->click();
        QCOMPARE(dialog.settings().toolbarActions, defaultSettings().toolbarActions);
    }
    void failedWriteReportsError() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString error;
        QVERIFY(!saveSettings(defaultSettings(), &error, directory.path()));
        QVERIFY(!error.isEmpty());
    }
    void resetAndCancelOnlyChangeDraft() {
        auto original = defaultSettings();
        original.theme = ThemeMode::Dark;
        original.launchAtLogin = true;
        original.fitImageOnOpen = false;
        original.embedOriginal = false;
        original.checkUpdatesOnStartup = true;
        original.defaultTool = 1;
        original.toolbarActions = {"copyImage"};
        original.shortcuts["paste"] = QKeySequence("Ctrl+Alt+8");
        SettingsDialog dialog(original);
        int applyCount = 0;
        dialog.setApplyHandler([&](const AppSettings &) {
            ++applyCount;
            return QString();
        });
        QVERIFY(dialog.settings() == original);
        dialog.show();
        QTest::mouseClick(dialog.findChild<QPushButton *>("settingsReset"), Qt::LeftButton);
        QVERIFY(dialog.settings() == defaultSettings());
        QCOMPARE(applyCount, 0);
        QTest::mouseClick(dialog.findChild<QPushButton *>("settingsCancel"), Qt::LeftButton);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(applyCount, 0);
        QCOMPARE(original.theme, ThemeMode::Dark);
        QVERIFY(original.launchAtLogin);
    }
    void launchAtLoginFailureKeepsDraftUntilSaved() {
        SettingsDialog dialog(defaultSettings());
        auto login = dialog.findChild<QCheckBox *>("launchAtLogin");
        auto save = dialog.findChild<QPushButton *>("settingsSave");
        auto error = dialog.findChild<QLabel *>("errorLabel");
        QVERIFY(login && save && error);
        QVERIFY(!login->isChecked());
        login->setChecked(true);
        AppSettings persisted = defaultSettings();
        dialog.setApplyHandler([&](const AppSettings &draft) {
            if (draft.launchAtLogin)
                return QString("无法设置开机启动，请检查系统权限。");
            persisted = draft;
            return QString();
        });
        dialog.show();
        save->click();
        QVERIFY(dialog.isVisible());
        QVERIFY(error->isVisible());
        QVERIFY(error->text().contains("开机启动"));
        QVERIFY(login->isChecked());
        QVERIFY(!persisted.launchAtLogin);
        login->setChecked(false);
        QVERIFY(!error->isVisible());
        save->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(!dialog.isVisible());
        QVERIFY(!persisted.launchAtLogin);
    }
    void launchAtLoginNoticeDoesNotChangeDraft() {
        SettingsDialog dialog(defaultSettings());
        auto notice = dialog.findChild<QLabel *>("launchAtLoginNotice");
        QVERIFY(notice);
        QVERIFY(notice->isHidden());
        const auto message = QString("请前往系统设置批准 EditHere 登录项。");
        dialog.setLaunchAtLoginNotice(message);
        QCOMPARE(notice->text(), message);
        QVERIFY(!notice->isHidden());
        QVERIFY(dialog.settings() == defaultSettings());
        dialog.setLaunchAtLoginNotice({});
        QVERIFY(notice->isHidden());
    }
    void replayGuideClosesWithoutApplyingDraft() {
        const auto original = defaultSettings();
        SettingsDialog dialog(original);
        auto guide = dialog.findChild<QPushButton *>("restartGuide");
        auto login = dialog.findChild<QCheckBox *>("launchAtLogin");
        QVERIFY(guide && login);
        QVERIFY(guide->toolTip().contains("未保存"));
        login->setChecked(true);
        int applyCount = 0;
        dialog.setApplyHandler([&](const AppSettings &) {
            ++applyCount;
            return QString();
        });
        QSignalSpy requested(&dialog, &SettingsDialog::guideRequested);
        QSignalSpy rejected(&dialog, &QDialog::rejected);
        bool closedBeforeRequest = false;
        connect(&dialog, &SettingsDialog::guideRequested, &dialog, [&] {
            closedBeforeRequest = !dialog.isVisible() && dialog.result() == QDialog::Rejected;
        });
        QTimer::singleShot(0, guide, &QPushButton::click);
        QCOMPARE(dialog.exec(), int(QDialog::Rejected));
        QCOMPARE(requested.count(), 1);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(closedBeforeRequest);
        QCOMPARE(applyCount, 0);
        QVERIFY(!original.launchAtLogin);
    }
    void guideStateSurvivesOldDraftAndRestoredDefaults() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("config/settings.ini");
        QVERIFY(!hasSeenGuide(path));
        auto preferences = defaultSettings();
        preferences.theme = ThemeMode::Dark;
        preferences.launchAtLogin = true;
        QString error;
        QVERIFY2(saveSettings(preferences, &error, path), qPrintable(error));
        QVERIFY(!hasSeenGuide(path));
        SettingsDialog staleDialog(loadSettings(path));
        error = "old error";
        QVERIFY2(markGuideSeen(&error, path), qPrintable(error));
        QVERIFY(error.isEmpty());
        QVERIFY(hasSeenGuide(path));
        QVERIFY(loadSettings(path) == preferences);
        staleDialog.setApplyHandler([&](const AppSettings &draft) {
            saveSettings(draft, &error, path);
            return error;
        });
        staleDialog.findChild<QPushButton *>("settingsSave")->click();
        QCOMPARE(staleDialog.result(), int(QDialog::Accepted));
        QVERIFY(hasSeenGuide(path));
        SettingsDialog resetDialog(loadSettings(path));
        resetDialog.setApplyHandler([&](const AppSettings &draft) {
            saveSettings(draft, &error, path);
            return error;
        });
        resetDialog.findChild<QPushButton *>("settingsReset")->click();
        QVERIFY(resetDialog.settings() == defaultSettings());
        QVERIFY(hasSeenGuide(path));
        resetDialog.findChild<QPushButton *>("settingsSave")->click();
        QCOMPARE(resetDialog.result(), int(QDialog::Accepted));
        QVERIFY(loadSettings(path) == defaultSettings());
        QVERIFY(hasSeenGuide(path));
        QVERIFY2(markGuideSeen(&error, path), qPrintable(error));
        QVERIFY(hasSeenGuide(path));
    }
    void guideStateCreationAndMalformedValues() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("new/settings.ini");
        QString error;
        QVERIFY2(markGuideSeen(&error, path), qPrintable(error));
        QVERIFY(hasSeenGuide(path));
        QVERIFY(loadSettings(path) == defaultSettings());
        QSettings source(path, QSettings::IniFormat);
        for (const auto &value : {"invalid", "false", "0", ""}) {
            source.setValue("onboarding/seen", value);
            source.sync();
            QVERIFY(!hasSeenGuide(path));
        }
        for (const auto &value : {"true", "1"}) {
            source.setValue("onboarding/seen", value);
            source.sync();
            QVERIFY(hasSeenGuide(path));
        }
    }
    void failedGuideWriteReportsError() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString error;
        QVERIFY(!markGuideSeen(&error, directory.path()));
        QVERIFY(!error.isEmpty());
        QFile blocker(directory.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        QCOMPARE(blocker.write("keep"), qint64(4));
        blocker.close();
        error.clear();
        QVERIFY(!markGuideSeen(&error, blocker.fileName() + "/settings.ini"));
        QVERIFY(!error.isEmpty());
        QVERIFY(blocker.open(QIODevice::ReadOnly));
        QCOMPARE(blocker.readAll(), QByteArray("keep"));
    }

    void whiteboardPreferencesPreserveUsefulSettingsAndDropCapture() {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.ini");
        const auto defaults = defaultSettings();
        QVERIFY(!defaults.shortcuts.contains("capture"));
        QVERIFY(!defaults.shortcuts.contains("annotate"));
        QVERIFY(!defaults.toolbarActions.contains("capture"));
        QSettings legacy(path, QSettings::IniFormat);
        legacy.setValue("version", 1);
        legacy.setValue("appearance/theme", "dark");
        legacy.setValue("defaults/captureOnStartup", true);
        legacy.setValue("capture/scrollAxis", "horizontal");
        legacy.setValue("shortcuts/capture", "not a key");
        legacy.setValue("shortcuts/paste", "Ctrl+Alt+V");
        legacy.setValue("toolbar/actions", QStringList{"capture", "copyJson", "fit"});
        legacy.sync();
        auto settings = loadSettings(path);
        QCOMPARE(settings.theme, ThemeMode::Dark);
        QCOMPARE(settings.shortcuts.value("paste"), QKeySequence("Ctrl+Alt+V"));
        QCOMPARE(settings.toolbarActions, (QStringList{"copyJson", "fit"}));
        QString error;
        QVERIFY2(saveSettings(settings, &error, path), qPrintable(error));
        QVERIFY(loadSettings(path) == settings);
        QSettings saved(path, QSettings::IniFormat);
        QVERIFY(!saved.contains("defaults/captureOnStartup"));
        QVERIFY(!saved.contains("capture/scrollAxis"));
        QVERIFY(!saved.contains("shortcuts/capture"));
        SettingsDialog dialog(settings);
        QVERIFY(!dialog.findChild<QWidget *>("settingsScrollPage"));
        QVERIFY(!dialog.findChild<QCheckBox *>("captureOnStartup"));
        QVERIFY(!dialog.findChild<QComboBox *>("ocrLanguage"));
        QVERIFY(!dialog.findChild<QKeySequenceEdit *>("shortcut_capture"));
        QVERIFY(dialog.settings() == settings);
        settings.shortcuts["paste"] = settings.shortcuts["save"];
        QVERIFY(!validateSettings(settings).isEmpty());
    }

};
QTEST_MAIN(SettingsTests)
#include "settings_test.moc"
