#include "platform.h"
#include <QWidget>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
namespace h2d {
void configureNativeWindow(QWidget *window) {
    const auto handle = reinterpret_cast<HWND>(window->winId());
    SetWindowPos(handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    const DWORD preference = 2;
    DwmSetWindowAttribute(handle, 33, &preference, sizeof(preference));
}
}
