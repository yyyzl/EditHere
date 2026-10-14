#include "controller.h"
#include "diagnostics.h"
#include "agentprotocol.h"
#include "i18n.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include "autostart.h"
#include "settingsdialog.h"
#include "ui.h"
#include "updatechecker.h"
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
namespace h2d {
Controller::Controller(QObject *parent, const AppSettings &settings, const QString &settingsFile)
    : QObject(parent), settings_(settings), settingsFile_(settingsFile), editor_(),
      tray_(QApplication::windowIcon().isNull() ? glyph("open", accent()) : QApplication::windowIcon(), this) {
    connect(&editor_, &Editor::agentFinishRequested, this, &Controller::finishAgentSession);
    connect(&editor_, &Editor::agentCancelRequested, this, [this] {
        cancelAgentSession(agentSessionId_, "cancelled", "User cancelled. Edits remain in EditHere.");
    });
    editor_.setPreferences(settings_);
    editor_.setShortcuts(settings_.shortcuts);
    tray_.setObjectName("edithereTray");
#ifdef Q_OS_MACOS
    // macOS 菜单栏使用系统模板着色，随背景自动切换明暗。
    auto menuBarIcon = tray_.icon();
    menuBarIcon.setIsMask(true);
    tray_.setIcon(menuBarIcon);
    // Dock 再次打开也会发送此通知，即使应用已经处于活动状态。
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &Controller::restoreClosedWindow);
#endif
    connect(&editor_, &Editor::hiddenToTray, this, [this] { closedToTray_ = true; });
    auto menu = new QMenu(&editor_);
    annotateAction_ = menu->addAction(QString(), this, &Controller::openAnnotation);
    annotateAction_->setObjectName("trayAnnotate");
    openAction_ = menu->addAction(QString(), &editor_, [this] {
        scheduleStartupUpdate();
        editor_.openFile();
    });
    menu->addSeparator();
    settingsAction_ = menu->addAction(QString(), this, [this] { openSettings(); });
    settingsAction_->setObjectName("traySettings");
    updatesAction_ = menu->addAction(QString(), this, [this] { openSettings(true); });
    updatesAction_->setObjectName("trayUpdates");
    menu->addSeparator();
    quitAction_ = menu->addAction(QString(), this, &Controller::quit);
    tray_.setContextMenu(menu);
    retranslate();
    tray_.show();
#ifdef Q_OS_LINUX
    connect(&editor_, &Editor::hiddenToTray, this, [] {
        if (QGuiApplication::platformName() != "offscreen" && !QSystemTrayIcon::isSystemTrayAvailable())
            QCoreApplication::quit();
    });
#endif
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            activate();
    });
    connect(&editor_, &Editor::guideDismissed, this, [this] {
        QString error;
        if (!hasSeenGuide(settingsFile_) && !markGuideSeen(&error, settingsFile_)) {
            diagnostics::write(diagnostics::Level::Error, "settings.guide", error);
            tray_.showMessage("EditHere", tr("无法记录引导状态，下次启动时可能再次显示。\n") + error);
        }
    });
    connect(&editor_, &Editor::toolbarSettingsRequested,this,[this] { openSettings(false,true); });
    connect(&editor_, &Editor::settingsRequested, this, [this] { openSettings(); });
    connect(&editor_, &Editor::preferencesChanged, this, [this] {
        // The editor turned a preference off by itself — the "don't ask again" box on
        // the close question does — and an answer the user gave has to outlive the
        // window it was given in.
        settings_.confirmBeforeDiscard = false;
        QString error;
        if (!saveSettings(settings_, &error, settingsFile_)) {
            diagnostics::write(diagnostics::Level::Error, "settings.persist", error);
            tray_.showMessage("EditHere", tr("设置未能保存：") + error);
        }
    });
}
void Controller::retranslate() {
    if (openAction_) openAction_->setText(tr("打开图片或项目"));
    if (annotateAction_) annotateAction_->setText(tr("新建批注白板"));
    if (settingsAction_) settingsAction_->setText(tr("设置…"));
    if (updatesAction_) updatesAction_->setText(tr("检查更新…"));
    if (quitAction_) quitAction_->setText(tr("退出"));
    tray_.setToolTip(tr("EditHere · 改这里"));
}
void Controller::openAnnotation() {
    if (QApplication::activeModalWidget())
        return;
    scheduleStartupUpdate();
    if (auto menu = tray_.contextMenu())
        menu->close();
    editor_.openEmpty();
}
void Controller::openSettings(bool updates, bool toolbar) {
    openSettingsDialog(updates, toolbar);
}
void Controller::openSettingsDialog(bool updates, bool toolbar, const UpdateChecker::Result *release) {
    if (QApplication::activeModalWidget())
        return;
    scheduleStartupUpdate();
    if (auto menu = tray_.contextMenu())
        menu->close();
    auto draft = settings_;
    QString startupReadError;
    const bool registered = launchAtLoginEnabled(&startupReadError);
    if (startupReadError.isEmpty())
        draft.launchAtLogin = registered;
    SettingsDialog dialog(draft, &editor_);
    dialog.setUpdatePreparationHandler([this] {
        // Never discard an active agent session to make room for an update.
        // allowReplace also commits inline edits and offers Save/Discard/Cancel.
        return editor_.allowReplace();
    });
    bool updateStarted = false;
    connect(&dialog, &SettingsDialog::updateInstallStarted, &dialog, [&] { updateStarted = true; });
    dialog.setLaunchAtLoginNotice(startupReadError.isEmpty() ? launchAtLoginNotice() : startupReadError);
    // A language switch inside the dialog is applied live; the tray menu is not a
    // top-level widget, so it has to rebuild its own labels here.
    connect(&dialog, &SettingsDialog::languageApplied, this, &Controller::retranslate);
    bool guideRequested = false;
    connect(&dialog, &SettingsDialog::guideRequested, &dialog, [&] { guideRequested = true; });
    if (release)
        dialog.offerUpdate(*release, true);
    else if (updates)
        dialog.showUpdates(true);
    else if (toolbar)
        dialog.showToolbar();
    dialog.setApplyHandler([this](const AppSettings &next) -> QString {
        if (auto error = validateSettings(next); !error.isEmpty())
            return error;
        const auto previousLanguage = settings_.language;
        if (!installLanguage(next.language))
            return tr("界面语言加载失败，请重新安装 EditHere。");
        QString startupError;
        const bool wasRegistered = launchAtLoginEnabled(&startupError);
        if (!startupError.isEmpty())
            return startupError;
        QString error;
        const bool startupChanged = wasRegistered != next.launchAtLogin;
        if (!setLaunchAtLoginEnabled(next.launchAtLogin, &error)) {
            return error;
        }
        if (!saveSettings(next, &error, settingsFile_)) {
            // Nothing was written, so the live language preview has to go back too.
            if (next.language != previousLanguage)
                installLanguage(previousLanguage);
            QString rollbackError;
            if (startupChanged && !setLaunchAtLoginEnabled(wasRegistered, &rollbackError))
                error += tr("\n开机自启未能恢复：") + rollbackError;
            return error;
        }
        settings_ = next;
        editor_.setPreferences(settings_);
        editor_.setShortcuts(settings_.shortcuts);
        applyTheme(settings_.theme);
        retranslate();
        return {};
    });
    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (updateStarted) {
        tray_.hide();
        // The user has confirmed, and the maintenance worker has validated and
        // prepared the update. Its ACK allows it to wait for this process to exit.
        // Do not run the close-to-tray path or ask the same question a second time.
        QCoreApplication::exit(0);
        return;
    }
    if (accepted) {
        const auto notice = launchAtLoginNotice();
        if (!notice.isEmpty()) tray_.showMessage(tr("EditHere 开机自启"), notice);
    }
    if (guideRequested)
        showGuide();
}
void Controller::showGuide() {
    if (QApplication::activeModalWidget())
        return;
    if (auto menu = tray_.contextMenu())
        menu->close();
    editor_.showGuide();
}
void Controller::start(bool demo, const QString &path, bool background) {
    if (background && path.isEmpty() && !demo)
        return;
    scheduleStartupUpdate();
    if (!agentSessionId_.isEmpty()) {
        raiseEditor();
        return;
    }
    if (!path.isEmpty())
        editor_.openFile(path);
    else if (demo)
        editor_.setDocument(fromImage(exampleImage(), "demo", tr("示例产品页面")));
    else
        activate();
}
void Controller::scheduleStartupUpdate() {
    if (!agentSessionId_.isEmpty())
        return;
    if (!installationReceiptChecked_) {
        installationReceiptChecked_ = true;
        QTimer::singleShot(3000, this, &Controller::readInstallationReceipt);
    }
    if (startupUpdateChecked_ || !settings_.checkUpdatesOnStartup)
        return;
    startupUpdateChecked_ = true;
    auto checker = new UpdateChecker(this);
    checker->setObjectName("startupUpdateChecker");
    connect(checker, &UpdateChecker::finished, this, [this, checker] {
        startupUpdateFinished(checker->lastResult());
        checker->deleteLater();
    });
    QTimer::singleShot(3000, checker, &UpdateChecker::check);
}
void Controller::startupUpdateFinished(const UpdateChecker::Result &result) {
    if (result.status != UpdateChecker::Available || !settings_.checkUpdatesOnStartup)
        return;
    pendingStartupUpdate_ = result;
    presentStartupUpdate();
}
void Controller::presentStartupUpdate() {
    if (!pendingStartupUpdate_)
        return;
    if (!settings_.checkUpdatesOnStartup) {
        pendingStartupUpdate_.reset();
        return;
    }
    if (!agentSessionId_.isEmpty() || QApplication::activeModalWidget() || editor_.guideActive()) {
        QTimer::singleShot(500, this, &Controller::presentStartupUpdate);
        return;
    }
    const auto release = std::move(*pendingStartupUpdate_);
    pendingStartupUpdate_.reset();
    QMessageBox prompt(QMessageBox::Information, tr("EditHere 更新"),
                       tr("发现新版本 %1。").arg(release.tagName), QMessageBox::NoButton, &editor_);
    prompt.setObjectName("startupUpdatePrompt");
    auto update = prompt.addButton(tr("立即更新"), QMessageBox::AcceptRole);
    auto skip = prompt.addButton(tr("跳过"), QMessageBox::RejectRole);
    prompt.setDefaultButton(skip);
    prompt.setEscapeButton(skip);
    prompt.exec();
    if (prompt.clickedButton() == update)
        openSettingsDialog(true, false, &release);
}
void Controller::readInstallationReceipt() {
    // An explicit settings file keeps test/development instances isolated from
    // the normal application's persistent installer result.
    const auto path = settingsFile_.isEmpty()
                          ? UpdateChecker::installationResultPath()
                          : QDir(QFileInfo(settingsFile_).absolutePath()).filePath("installer-results/last.json");
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > 65536)
        return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;
    const auto receipt = document.object();
    if (UpdateChecker::installationResultMessage(receipt, QCoreApplication::applicationDirPath(),
                                                  QCoreApplication::applicationVersion()).isEmpty())
        return;
    const auto settingsPath = settingsFile_.isEmpty()
                                  ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                        .filePath("settings.ini") : settingsFile_;
    QSettings persisted(settingsPath, QSettings::IniFormat);
    if (persisted.value("updates/seenInstallerResult").toString() == receipt.value("id").toString())
        return;
    pendingInstallationReceipt_ = receipt;
    presentInstallationReceipt();
}
void Controller::presentInstallationReceipt() {
    if (pendingInstallationReceipt_.isEmpty())
        return;
    if (!agentSessionId_.isEmpty() || QApplication::activeModalWidget() || editor_.guideActive()) {
        QTimer::singleShot(500, this, &Controller::presentInstallationReceipt);
        return;
    }
    const auto receipt = std::move(pendingInstallationReceipt_);
    pendingInstallationReceipt_ = {};
    const auto message = UpdateChecker::installationResultMessage(receipt, QCoreApplication::applicationDirPath(),
                                                                  QCoreApplication::applicationVersion());
    QMessageBox prompt(QMessageBox::Warning, tr("EditHere 安装结果"), message, QMessageBox::Ok, &editor_);
    prompt.setObjectName("installationResultPrompt");
    QStringList details;
    if (const auto value = receipt.value("log").toString(); !value.isEmpty())
        details.append(tr("安装日志：%1").arg(value));
    if (const auto value = receipt.value("journal").toString(); !value.isEmpty())
        details.append(tr("事务记录：%1").arg(value));
    if (const auto value = receipt.value("recovery").toString(); !value.isEmpty())
        details.append(tr("恢复目录：%1").arg(value));
    if (const auto value = receipt.value("stage").toString(); !value.isEmpty())
        details.append(tr("暂存目录：%1").arg(value));
    if (!details.isEmpty()) prompt.setDetailedText(details.join('\n'));
    prompt.exec();
    const auto settingsPath = settingsFile_.isEmpty()
                                  ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                        .filePath("settings.ini") : settingsFile_;
    QSettings persisted(settingsPath, QSettings::IniFormat);
    persisted.setValue("updates/seenInstallerResult", receipt.value("id").toString());
    persisted.sync();
    if (persisted.status() != QSettings::NoError)
        diagnostics::write(diagnostics::Level::Warning, "updates.receipt", "Could not persist installer result acknowledgement.");
}
void Controller::raiseEditor() {
    if (editor_.isMinimized())
        editor_.setWindowState(editor_.windowState() & ~Qt::WindowMinimized);
    editor_.show();
    editor_.raise();
    editor_.activateWindow();
}
void Controller::activate() {
    if (QApplication::activeModalWidget())
        return;
    scheduleStartupUpdate();
    if (editor_.hasDocument() || editor_.hasVideo() || !agentSessionId_.isEmpty())
        raiseEditor();
    else
        editor_.openEmpty();
}
void Controller::restoreClosedWindow(Qt::ApplicationState state) {
    // 只恢复用户关闭过的窗口，避免初始激活使登录自启或 AI 后台启动弹窗。
    if (state == Qt::ApplicationActive && closedToTray_ && !editor_.isVisible())
        activate();
}
void Controller::quit() {
    const bool pendingSession = !agentSessionId_.isEmpty();
    if (pendingSession)
        cancelAgentSession(agentSessionId_, "cancelled", "EditHere is exiting. No feedback was returned.");
    // The update flow and installers ask a tray-resident instance to exit, so the
    // save/discard/cancel prompt has to come to the front instead of hiding there.
    if ((pendingSession || editor_.hasUnsavedChanges()) && !editor_.isVisible())
        raiseEditor();
    if (!editor_.allowReplace())
        return;
    tray_.hide();
    qApp->quit();
}
QJsonObject Controller::handleAgentRequest(const QJsonObject &request) {
    const auto command = request["command"].toString();
    if (command == "status") {
        QString startupError;
        const bool startupRegistered = launchAtLoginEnabled(&startupError);
        return {{"ok", true}, {"running", true}, {"version", EDITHERE_VERSION},
                {"executable", QCoreApplication::applicationFilePath()},
                {"startupRegistered", startupRegistered}, {"startupNotice", startupError.isEmpty() ? launchAtLoginNotice() : startupError},
                {"hasDocument", editor_.hasDocument()}, {"dirty", editor_.hasUnsavedChanges()},
                {"agentSession", !agentSessionId_.isEmpty()}};
    }
    if (command != "open" && command != "annotate")
        return agentError("invalid_arguments", "Unsupported command.");
    if (!agentSessionId_.isEmpty() || QApplication::activeModalWidget())
        return agentError("busy", "EditHere is busy with another request or dialog.");
    if (editor_.hasUnsavedChanges())
        return agentError("busy", "The current document has unsaved changes. Save or close it in EditHere, then retry.");
    const auto input = request["input"].toString();
    if (!QFileInfo(input).isAbsolute()) return agentError("invalid_arguments", "Input path must be absolute.");
    QString output;
    int timeout = 1800;
    if (command == "annotate") {
        output = request["output"].toString();
        const auto error = validateNewFeedbackPath(output);
        if (!error.isEmpty()) return agentError("io_error", error);
        timeout = request["timeout"].toInt(1800);
        if (timeout < 1 || timeout > 86400) return agentError("invalid_arguments", "Timeout must be between 1 and 86400 seconds.");
    }
    try {
        editor_.loadMedia(input);
    } catch (const std::exception &error) {
        diagnostics::write(diagnostics::Level::Error, "agent.load_media", QString::fromUtf8(error.what()));
        return agentError("io_error", QString::fromUtf8(error.what()));
    }
    if (command == "open") return {{"ok", true}, {"command", command}, {"accepted", true}, {"input", input}};
    agentSessionId_ = uniqueId();
    agentOutput_ = output;
    agentEmbed_ = request["embed"].toBool(true);
    editor_.setAgentSession(true);
    const auto id = agentSessionId_;
    diagnostics::write(diagnostics::Level::Info, "agent.session_start", "Annotation session started",
                       {{"session", id}, {"timeoutSeconds", timeout}, {"imageIncluded", agentEmbed_}});
    QTimer::singleShot(timeout * 1000, this, [this, id] {
        cancelAgentSession(id, "timeout", "Annotation timed out. Your edits remain in EditHere.");
    });
    return {{"ok", true}, {"pending", true}, {"session", id}};
}
void Controller::cancelAgentSession(const QString &id, const QString &code, const QString &message) {
    if (id.isEmpty() || id != agentSessionId_) return;
    diagnostics::write(code == "io_error" ? diagnostics::Level::Error : diagnostics::Level::Info,
                       "agent.session_end", message, {{"session", id}, {"code", code}});
    const auto finishedId = agentSessionId_;
    agentSessionId_.clear();
    agentOutput_.clear();
    editor_.setAgentSession(false);
    emit agentSessionFinished(finishedId, agentError(code, message));
}
void Controller::finishAgentSession() {
    if (agentSessionId_.isEmpty()) return;
    const auto id = agentSessionId_;
    const auto output = agentOutput_;
    try {
        writeNewFeedback(output, editor_.agentFeedback(agentEmbed_));
        diagnostics::write(diagnostics::Level::Info, "agent.feedback_written", "Feedback file committed",
                           {{"session", id}, {"bytes", QFileInfo(output).size()}});
    } catch (const std::exception &error) {
        cancelAgentSession(id, "io_error", QString::fromUtf8(error.what()));
        return;
    }
    const QJsonObject result{{"ok", true}, {"command", "annotate"}, {"output", output},
                            {"annotations", editor_.totalAnnotationCount()}, {"imageIncluded", agentEmbed_}};
    agentSessionId_.clear();
    agentOutput_.clear();
    editor_.setAgentSession(false);
    diagnostics::write(diagnostics::Level::Info, "agent.session_end", "Annotation completed; returning feedback",
                       {{"session", id}, {"code", "success"}, {"annotationCount", result["annotations"]}});
    emit agentSessionFinished(id, result);
}
} // namespace h2d
