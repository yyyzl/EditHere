#include "platform.h"
#include <QGuiApplication>
#include <QWidget>
#import <Cocoa/Cocoa.h>
namespace h2d {
void configureNativeWindow(QWidget *widget) {
    if (QGuiApplication::platformName() != "cocoa") return;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
    NSWindow *window = view.window;
    if (!window) return;
    window.collectionBehavior = NSWindowCollectionBehaviorDefault;
    window.level = NSNormalWindowLevel;
}
}
