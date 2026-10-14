#pragma once
#include <QKeySequence>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
namespace h2d {
enum class ThemeMode { System, Light, Dark };
// Interface language. "System" follows the operating system and falls back to
// English on non-Chinese systems; the built-in source language is Simplified
// Chinese, so zh_CN needs no translation file.
enum class LanguageMode { System, SimplifiedChinese, English };
struct AppSettings {
    ThemeMode theme = ThemeMode::System;
    LanguageMode language = LanguageMode::System;
    QMap<QString, QKeySequence> shortcuts;
    QStringList toolbarActions = {"saveProject", "saveImage", "exportJson", "copyJsonText", "copyJson", "copyImage"};
    bool launchAtLogin = false;
    bool fitImageOnOpen = true;
    bool embedOriginal = true;
    // Whether closing the annotation window asks about unsaved changes. Turning it off
    // is what the "don't ask again" box on that question does: from then on closing
    // throws the changes away instead of stopping to ask. It is a preference rather
    // than a one-off so that it can be turned back on in the settings.
    bool confirmBeforeDiscard = true;
    bool checkUpdatesOnStartup = true;
    QString feedbackDir; // Empty = default CacheLocation/feedback.
    int defaultTool = 0; // Canvas::Smart, Point, Rectangle, Adjust.
    bool operator==(const AppSettings &) const = default;
};
struct ShortcutDefinition {
    QString id, label;
    QKeySequence defaultKey;
};
struct ToolbarActionDefinition {
    QString id, label;
};
QVector<ShortcutDefinition> shortcutDefinitions();
QVector<ToolbarActionDefinition> toolbarActionDefinitions();
AppSettings defaultSettings();
AppSettings loadSettings(const QString &filePath = {});
bool saveSettings(const AppSettings &settings, QString *error = nullptr, const QString &filePath = {});
bool hasSeenGuide(const QString &filePath = {});
bool markGuideSeen(QString *error = nullptr, const QString &filePath = {});
QString validateSettings(const AppSettings &settings);
} // namespace h2d
