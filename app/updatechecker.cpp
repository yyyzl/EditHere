#include "updatechecker.h"
#include "diagnostics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QVersionNumber>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace h2d {
namespace {
constexpr int maximumResponse = 1024 * 1024;
UpdateChecker::Result failure(const QString &message) {
    return {UpdateChecker::Failed, message, UpdateChecker::releasesUrl(), {}, {}, {}, {}, {}};
}
bool isValidAssetUrl(const QUrl &url) {
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.port(-1) != -1 || url.hasFragment())
        return false;
    return (url.host() == "github.com" &&
            (url.path().startsWith("/yyyzl/EditHere/releases/download/") ||
             url.path().startsWith("/yyyzl/EditHere/releases/latest/download/"))) ||
           url.host() == "objects.githubusercontent.com" || url.host() == "release-assets.githubusercontent.com";
}
} // namespace
QUrl UpdateChecker::releasesUrl() {
    return QUrl("https://github.com/yyyzl/EditHere/releases");
}
UpdateChecker::Result UpdateChecker::parseRelease(const QByteArray &bytes, const QString &currentVersion) {
    if (bytes.size() > maximumResponse)
        return failure(tr("更新信息过大，请稍后重试。"));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return failure(tr("无法读取更新信息，请稍后重试。"));
    const auto object = document.object();
    const auto tag = object.value("tag_name").toString();
    static const QRegularExpression versionPattern("^v?([0-9]+\\.[0-9]+\\.[0-9]+)$");
    const auto match = versionPattern.match(tag);
    const auto localMatch = versionPattern.match(currentVersion);
    const QUrl expectedUrl(releasesUrl().toString() + "/tag/" + tag);
    const QUrl url(object.value("html_url").toString());
    const bool invalidDraft = !object.value("draft").isBool() || object.value("draft").toBool();
    if (!match.hasMatch() || !localMatch.hasMatch() || invalidDraft ||
        !object.value("prerelease").isBool() || object.value("prerelease").toBool() ||
        url.scheme() != "https" || url.host() != expectedUrl.host() ||
        !url.userInfo().isEmpty() || url.port(-1) != -1 || url.hasQuery() || url.hasFragment() ||
        url.path() != expectedUrl.path())
        return failure(tr("发行信息不是有效的 EditHere 正式版本，请到发布页查看。"));
    const auto remote = QVersionNumber::fromString(match.captured(1));
    const auto local = QVersionNumber::fromString(localMatch.captured(1));
    if (remote.segmentCount() != 3 || local.segmentCount() != 3)
        return failure(tr("版本号无法比较，请到发布页查看。"));
    const QString ver = match.captured(1);
    const auto assets = object.value("assets").toArray();
    // Releases publish version-less asset names so documentation links stay valid.
    // The historical versioned names are still accepted: pick whichever exists,
    // preferring the versioned name when a release carries both.
    const auto pickAsset = [&assets](const QStringList &candidates) {
        Asset found;
        for (const QString &candidate : candidates) {
            for (const auto &item : assets) {
                const auto obj = item.toObject();
                if (obj.value("name").toString() != candidate)
                    continue;
                const QUrl assetUrl(obj.value("browser_download_url").toString());
                if (!isValidAssetUrl(assetUrl))
                    continue;
                found = {candidate, assetUrl, obj.value("size").toVariant().toLongLong()};
                return found;
            }
        }
        return found;
    };
    const QStringList installerNames{QString("EditHere-%1-win-x64-setup.exe").arg(ver),
                                     QStringLiteral("EditHere-win-x64-setup.exe")};
    const QStringList portableNames{QString("EditHere-%1-win-x64.zip").arg(ver),
                                    QStringLiteral("EditHere-win-x64.zip")};
    const Asset installer = pickAsset(installerNames);
    const Asset installerHash = installer.name.isEmpty() ? Asset{} : pickAsset({installer.name + ".sha256"});
    const Asset portable = pickAsset(portableNames);
    const Asset portableHash = portable.name.isEmpty() ? Asset{} : pickAsset({portable.name + ".sha256"});
    const int order = QVersionNumber::compare(remote, local);
    if (order > 0)
        return {Available, tr("发现新版本 %1。").arg(tag), url,
                tag, installer, installerHash, portable, portableHash,
                QVersionNumber::compare(remote, QVersionNumber(0, 9, 9)) >= 0};
    if (order < 0)
        return {NewerLocal, tr("当前版本高于已发布的正式版 %1。").arg(tag), url,
                tag, {}, {}, {}, {}};
    return {Current, tr("已是最新正式版 %1。").arg(tag), url,
            tag, {}, {}, {}, {}};
}
UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    handoffTimer_.setInterval(250);
    connect(&handoffTimer_, &QTimer::timeout, this, &UpdateChecker::pollHandoff);
    timeout_.setSingleShot(true);
    timeout_.setInterval(15000);
    cliTimeout_.setSingleShot(true);
    cliTimeout_.setInterval(3000);
    connect(&cliTimeout_, &QTimer::timeout, this, [this] {
        if (!busy_) return;
        publicRequested_ = true;
        process_.kill();
        requestPublic();
    });
#ifdef Q_OS_WIN
    process_.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    connect(&timeout_, &QTimer::timeout, this, &UpdateChecker::completeCheck);
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
        if (!busy_ || publicRequested_ || checkResult_)
            return;
        output_ += process_.readAllStandardOutput();
        if (output_.size() > maximumResponse) {
            requestPublic();
            process_.kill();
        }
    });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] { process_.readAllStandardError(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (busy_ && error == QProcess::FailedToStart)
            requestPublic();
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                if (!busy_ || checkResult_)
                    return;
                cliTimeout_.stop();
                if (publicRequested_) return;
                output_ += process_.readAllStandardOutput();
                const auto result = parseRelease(output_, QStringLiteral(EDITHERE_VERSION));
                if (code == 0 && status == QProcess::NormalExit && result.status != Failed)
                    collectSource(result);
                else
                    requestPublic();
            });
}
UpdateChecker::~UpdateChecker() {
    cancelHandoff();
    busy_ = false;
    process_.disconnect(this);
    for (auto *pointer : {&reply_, &downloadReply_, &hashReply_}) {
        if (*pointer) {
            auto *reply = pointer->data();
            *pointer = nullptr;
            reply->disconnect(this);
            reply->abort();
        }
    }
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(1000);
    }
}
void UpdateChecker::check() {
    if (busy_ || installing_)
        return;
    if (process_.state() != QProcess::NotRunning) {
        QTimer::singleShot(100, this, &UpdateChecker::check);
        return;
    }
    busy_ = true;
    publicRequested_ = false;
    output_.clear();
    checkResult_.reset();
    timeout_.start();
    auto gh = QStandardPaths::findExecutable("gh");
#ifdef Q_OS_WIN
    if (gh.isEmpty()) {
        const auto installed = qEnvironmentVariable("ProgramFiles") + "/GitHub CLI/gh.exe";
        if (QFileInfo::exists(installed))
            gh = installed;
    }
#else
    if (gh.isEmpty()) {
        for (const auto &path : {"/opt/homebrew/bin/gh", "/usr/local/bin/gh"})
            if (QFileInfo(path).isExecutable()) {
                gh = path;
                break;
            }
    }
#endif
    if (gh.isEmpty()) {
        requestPublic();
        return;
    }
    process_.start(gh, {"api", "repos/yyyzl/EditHere/releases/latest", "--hostname", "github.com"});
    if (!publicRequested_) cliTimeout_.start();
}
void UpdateChecker::requestPublic() {
    if (!busy_ || reply_ || checkResult_)
        return;
    publicRequested_ = true;
    cliTimeout_.stop();
    requestSource();
}
void UpdateChecker::requestSource() {
    if (!busy_ || reply_ || checkResult_)
        return;
    QNetworkRequest request(QUrl("https://api.github.com/repos/yyyzl/EditHere/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "EditHere/" EDITHERE_VERSION);
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(12000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    output_.clear();
    auto *reply = network_.get(request);
    reply_ = reply;
    reply->setReadBufferSize(maximumResponse + 1);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (!busy_ || reply_ != reply) return;
        output_ += reply->readAll();
        if (output_.size() > maximumResponse)
            collectSource(failure(tr("更新信息过大，请稍后重试。")));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (!busy_ || reply_ != reply) return;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
            reply->error() != QNetworkReply::NoError) {
            collectSource(failure(tr("网络连接失败，暂时无法判断是否有更新。请稍后重试。")));
            return;
        }
        output_ += reply->readAll();
        collectSource(parseRelease(output_, QStringLiteral(EDITHERE_VERSION)));
    });
}
void UpdateChecker::collectSource(Result result) {
    if (!busy_ || checkResult_) return;
    checkResult_ = std::move(result);
    completeCheck();
}
void UpdateChecker::completeCheck() {
    if (!busy_)
        return;
    const auto timeout = failure(tr("检查超时，请检查网络后重试。"));
    finish(checkResult_.value_or(timeout));
}
void UpdateChecker::finish(Result result) {
    if (!busy_)
        return;
    if (result.status == Failed)
        diagnostics::write(diagnostics::Level::Warning, "updates.check", result.message);
    busy_ = false;
    timeout_.stop();
    cliTimeout_.stop();
    for (auto *pointer : {&reply_}) {
        if (*pointer) {
            auto *reply = pointer->data();
            *pointer = nullptr;
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
    if (process_.state() != QProcess::NotRunning)
        process_.kill();
    lastResult_ = result;
    emit finished(result.status, result.message, result.url);
}
QString UpdateChecker::prepareDownloadTarget(const QString &directory, const QString &packageName) {
    const QString path = QDir(directory).filePath("EditHere-update-" + packageName);
    QFile::remove(path);
    return path;
}
void UpdateChecker::downloadAndInstall(const Asset &package, const Asset &hashAsset, bool isInstaller,
                                       const QString &expectedVersion) {
    if (busy_ || installing_)
        return;
#ifndef Q_OS_WIN
    emit installFailed(tr("未找到适合当前系统的更新包，请前往发布页手动下载。"));
    return;
#endif
    if (!isValidAssetUrl(package.url) || !isValidAssetUrl(hashAsset.url) ||
        !package.name.endsWith("-win-x64-setup.exe") ||
        QFileInfo(package.name).fileName() != package.name ||
        hashAsset.name != package.name + ".sha256") {
        emit installFailed(tr("未找到校验文件，无法验证安装包完整性。"));
        return;
    }
    installing_ = true;
    isInstallerUpdate_ = isInstaller;
    downloadFileName_ = package.name;
    expectedInstallVersion_ = expectedVersion.startsWith('v') ? expectedVersion.mid(1) : expectedVersion;
    // Qt supplies unique paths and atomic writes; retries cannot concatenate an
    // earlier download or overwrite another running installer's payload.
    downloadDirectory_ = std::make_unique<QTemporaryDir>(
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("EditHere-update-XXXXXX"));
    downloadPath_ = downloadDirectory_->filePath(package.name);
    downloadFile_ = std::make_unique<QSaveFile>(downloadPath_);
    if (!downloadDirectory_->isValid() || !downloadFile_->open(QIODevice::WriteOnly)) {
        failInstall(tr("无法写入更新文件：%1").arg(downloadFile_->errorString()));
        return;
    }
    // Download the package.
    QNetworkRequest request(package.url);
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    downloadReply_ = network_.get(request);
    connect(downloadReply_, &QNetworkReply::downloadProgress, this, &UpdateChecker::downloadProgress);
    connect(downloadReply_, &QNetworkReply::readyRead, this, [this] {
        if (!downloadReply_)
            return;
        const auto bytes = downloadReply_->readAll();
        if (downloadFile_->write(bytes) != bytes.size())
            failInstall(tr("无法写入更新文件：%1").arg(downloadFile_->errorString()));
    });
    connect(downloadReply_, &QNetworkReply::finished, this, [this, hashAsset] {
        if (!downloadReply_)
            return;
        const auto reply = downloadReply_.data();
        downloadReply_ = nullptr;
        reply->disconnect(this);
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            failInstall(tr("下载失败，请检查网络后重试。"));
            return;
        }
        const auto remaining = reply->readAll();
        reply->deleteLater();
        if (downloadFile_->write(remaining) != remaining.size() || !downloadFile_->commit()) {
            failInstall(tr("无法写入更新文件：%1").arg(downloadFile_->errorString()));
            return;
        }
        downloadFile_.reset();
        QNetworkRequest hashRequest(hashAsset.url);
        hashRequest.setTransferTimeout(15000);
        hashRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        hashReply_ = network_.get(hashRequest);
        hashReply_->setReadBufferSize(4097);
        connect(hashReply_, &QNetworkReply::readyRead, this, [this] {
            if (hashReply_ && hashReply_->bytesAvailable() > 4096)
                failInstall(tr("校验失败：安装包已损坏或不完整。"));
        });
        connect(hashReply_, &QNetworkReply::finished, this, [this] {
            if (!hashReply_)
                return;
            const auto reply = hashReply_.data();
            hashReply_ = nullptr;
            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                failInstall(tr("无法下载校验文件，请检查网络后重试。"));
                return;
            }
            const QByteArray hashData = reply->readAll();
            reply->deleteLater();
            // Parse the hash: first whitespace-delimited token.
            const QString expectedHash = QString::fromUtf8(hashData).trimmed().section(QRegularExpression("\\s+"), 0, 0);
            verifyAndInstall(downloadPath_, expectedHash, isInstallerUpdate_);
        });
    });
}
void UpdateChecker::failInstall(const QString &message) {
    cancelHandoff();
    diagnostics::write(diagnostics::Level::Error, "updates.install", message);
    for (auto *pointer : {&downloadReply_, &hashReply_}) {
        if (*pointer) {
            auto reply = pointer->data();
            *pointer = nullptr;
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
    downloadFile_.reset();
    downloadDirectory_.reset();
    installing_ = false;
    emit installFailed(message);
}
void UpdateChecker::cancelDownload() {
    if (installing_)
        failInstall(tr("已取消更新，当前工作保持不变。"));
}
bool UpdateChecker::prepareHandoff(const QString &filePath, const QString &expectedHash, QString *error) {
    static const QRegularExpression idPattern("^EditHere-update-[A-Za-z0-9]+$");
    static const QRegularExpression versionPattern("^[0-9]+\\.[0-9]+\\.[0-9]+$");
    handoffTarget_ = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    handoffId_ = downloadDirectory_ ? QFileInfo(downloadDirectory_->path()).fileName() : QString();
    if (!downloadDirectory_ || !downloadDirectory_->isValid() || !idPattern.match(handoffId_).hasMatch() ||
        !versionPattern.match(expectedInstallVersion_).hasMatch()) {
        if (error) *error = tr("无法准备更新请求：安装目录或版本信息无效。");
        return false;
    }
    handoffToken_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject request{{"id", handoffId_}, {"target", handoffTarget_},
                              {"version", expectedInstallVersion_}, {"token", handoffToken_},
                              {"processId", QCoreApplication::applicationPid()},
                              {"package", QFileInfo(filePath).fileName()}, {"sha256", expectedHash}};
    QSaveFile file(downloadDirectory_->filePath("request.json"));
    const auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
        !file.commit()) {
        if (error) *error = tr("无法写入更新请求：%1").arg(file.errorString());
        return false;
    }
    return true;
}
bool UpdateChecker::writeHandoffAction(const QString &action) {
    if (!downloadDirectory_)
        return false;
    QSaveFile file(downloadDirectory_->filePath("ack.json"));
    const auto bytes = QJsonDocument(QJsonObject{{"action", action}, {"token", handoffToken_}})
                           .toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
void UpdateChecker::cancelHandoff() {
    handoffTimer_.stop();
    if (!awaitingHandoff_)
        return;
    // Keep the cancellation marker until the worker observes it. The worker owns
    // cleanup after it acknowledges cancellation, including when this dialog dies.
    writeHandoffAction("cancel");
    awaitingHandoff_ = false;
}
void UpdateChecker::pollHandoff() {
    if (!awaitingHandoff_ || !downloadDirectory_)
        return;
    if (handoffDeadline_.elapsed() >= handoffTimeoutMs_) {
        failInstall(tr("安装器准备超时，更新已取消，当前工作保持不变。"));
        return;
    }
    QFile file(downloadDirectory_->filePath("status.json"));
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
        failInstall(tr("无法读取安装器状态，更新已取消。"));
        return;
    }
    QJsonParseError error;
    const auto bytes = file.readAll();
    file.close(); // Release the Windows handle before reclaiming a failed handoff.
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        failInstall(tr("安装器状态无效，更新已取消。"));
        return;
    }
    const auto status = document.object();
    const QString target = QDir::cleanPath(QDir::fromNativeSeparators(status.value("target").toString()));
    const QString expectedTarget = QDir::cleanPath(QDir::fromNativeSeparators(handoffTarget_));
    if (status.value("token").toString() != handoffToken_ ||
        status.value("version").toString() != expectedInstallVersion_ ||
        target.compare(expectedTarget, Qt::CaseInsensitive) != 0) {
        failInstall(tr("安装器状态与本次更新不一致，更新已取消。"));
        return;
    }
    const auto state = status.value("status").toString();
    if (state == "failed" || state == "cancelled") {
        // The worker is terminal before ACK, so Qt can reclaim the request and package.
        awaitingHandoff_ = false;
        handoffTimer_.stop();
        downloadDirectory_->setAutoRemove(true);
        failInstall(status.value("message").toString().isEmpty()
                        ? tr("安装器准备失败，当前工作保持不变。") : status.value("message").toString());
        return;
    }
    if (state != "ready") {
        failInstall(tr("安装器状态无效，更新已取消。"));
        return;
    }
    if (!writeHandoffAction("ack")) {
        failInstall(tr("无法确认安装器已就绪，更新已取消。"));
        return;
    }
    // READY means validation and preparation succeeded. ACK permits the worker
    // to wait for this application to exit before it touches the installation.
    awaitingHandoff_ = false;
    handoffTimer_.stop();
    installing_ = false;
    emit installStarted();
}
QString UpdateChecker::installationResultPath() {
#ifdef Q_OS_WIN
    const auto local = qEnvironmentVariable("LOCALAPPDATA");
    return local.isEmpty() ? QString() : QDir(local).filePath("EditHere/Installer/Results/last.json");
#else
    return {};
#endif
}
QString UpdateChecker::installationResultMessage(const QJsonObject &receipt, const QString &target,
                                                  const QString &currentVersion) {
    const auto receiptTarget = QDir::cleanPath(QDir::fromNativeSeparators(receipt.value("target").toString()));
    const auto expectedTarget = QDir::cleanPath(QDir::fromNativeSeparators(target));
    static const QRegularExpression versionPattern("^[0-9]+\\.[0-9]+\\.[0-9]+$");
    const auto version = receipt.value("version").toString();
    if (receipt.value("id").toString().isEmpty() || !versionPattern.match(version).hasMatch() ||
        receiptTarget.compare(expectedTarget, Qt::CaseInsensitive) != 0)
        return {};
    const auto status = receipt.value("status").toString();
    if (status == "failed")
        return receipt.value("message").toString().isEmpty()
                   ? tr("上次安装未完成。请检查安装日志和恢复信息。") : receipt.value("message").toString();
    if (status == "committed" && version != currentVersion)
        return tr("安装文件已切换，但当前运行版本与预期版本 %1 不一致。请检查启动路径。").arg(version);
    return {};
}
bool UpdateChecker::canAutoInstall(const Result &result, bool installed, bool windows) {
    return windows && result.status == Available && (installed || result.portableInstallerSupported) &&
           isValidAssetUrl(result.installer.url) && isValidAssetUrl(result.installerHash.url) &&
           result.installerHash.name == result.installer.name + ".sha256";
}
bool UpdateChecker::verifyPackage(const QString &filePath, const QString &expectedHash, QString *error) {
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    static const QRegularExpression sha256("^[0-9a-fA-F]{64}$");
    if (!sha256.match(expectedHash).hasMatch())
        return fail(tr("校验失败：安装包已损坏或不完整。"));
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return fail(tr("无法读取已下载的文件。"));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f))
        return fail(tr("校验文件失败。"));
    const QString actualHash = QString::fromLatin1(hash.result().toHex());
    if (actualHash.compare(expectedHash, Qt::CaseInsensitive) != 0)
        return fail(tr("校验失败：安装包已损坏或不完整。"));
    if (error) error->clear();
    return true;
}
void UpdateChecker::verifyAndInstall(const QString &filePath, const QString &expectedHash, bool isInstaller) {
    QString error;
    if (!verifyPackage(filePath, expectedHash, &error)) {
        failInstall(error);
        return;
    }
    // Confirmation comes after downloading, before ANY external installer runs.
    if (!confirmInstall_ || !confirmInstall_()) {
        failInstall(tr("已取消更新，当前工作保持不变。"));
        return;
    }
#ifdef Q_OS_WIN
    if (!prepareHandoff(filePath, expectedHash, &error)) {
        failInstall(error);
        return;
    }
    QProcess installer;
    installer.setProgram(filePath);
    installer.setArguments(isInstaller ? QStringList{"/S", "/UPDATE"}
                                       : QStringList{"/S", "/UPDATE", "/PORTABLE"});
    auto arguments = installer.arguments();
    arguments.append("/HANDOFF=" + handoffId_);
    installer.setArguments(arguments);
    // NSIS requires /D= last and unquoted even with spaces. No command shell is
    // involved, so &, %, Unicode, and parentheses are ordinary path characters.
    installer.setNativeArguments("/D=" + QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
    installer.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    if (!(launchInstaller_ ? launchInstaller_(installer) : installer.startDetached())) {
        failInstall(tr("无法启动更新安装器。") + "\n" + installer.errorString());
        return;
    }
    // The detached NSIS process still needs this file after our QObject dies.
    downloadDirectory_->setAutoRemove(false);
    awaitingHandoff_ = true;
    handoffDeadline_.start();
    handoffTimer_.start();
    emit installerPreparing();
#else
    failInstall(tr("未找到适合当前系统的更新包，请前往发布页手动下载。"));
#endif
}
} // namespace h2d
