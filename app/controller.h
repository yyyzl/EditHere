#pragma once
#include "editor.h"
#include "settings.h"
#include "updatechecker.h"
#include <QSystemTrayIcon>
namespace h2d {
class Controller final : public QObject {
    Q_OBJECT
  public:
    explicit Controller(QObject *parent = nullptr, const AppSettings &settings = loadSettings(),
                        const QString &settingsFile = {});
    void start(bool demo, const QString &path = {}, bool background = false);
    void showGuide();
    void activate();
    void quit();
    void openSettings(bool updates = false, bool toolbar = false);
    QJsonObject handleAgentRequest(const QJsonObject &request);
    void cancelAgentSession(const QString &id, const QString &code, const QString &message);
  signals:
    void agentSessionFinished(const QString &id, const QJsonObject &result);
  private:
    friend class StartupFlowTestAccess;
    void openAnnotation();
    void finishAgentSession();
    void retranslate();
    void raiseEditor();
    void restoreClosedWindow(Qt::ApplicationState state);
    void scheduleStartupUpdate();
    void startupUpdateFinished(const UpdateChecker::Result &result);
    void presentStartupUpdate();
    void readInstallationReceipt();
    void presentInstallationReceipt();
    void openSettingsDialog(bool updates, bool toolbar, const UpdateChecker::Result *release = nullptr);
    AppSettings settings_;
    QString settingsFile_;
    QAction *openAction_ = nullptr, *annotateAction_ = nullptr;
    QAction *settingsAction_ = nullptr, *updatesAction_ = nullptr, *quitAction_ = nullptr;
    Editor editor_;
    QSystemTrayIcon tray_;
    QString agentSessionId_, agentOutput_;
    bool agentEmbed_ = true;
    bool closedToTray_ = false;
    bool startupUpdateChecked_ = false;
    std::optional<UpdateChecker::Result> pendingStartupUpdate_;
    bool installationReceiptChecked_ = false;
    QJsonObject pendingInstallationReceipt_;
};
} // namespace h2d
