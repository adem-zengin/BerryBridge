#include "ScreenManager.hpp"
#include <screen/screen.h>
#include <bb/cascades/Application>
#include <bb/cascades/Window>

ScreenManager::ScreenManager(QObject *parent) : QObject(parent)
{
}

void ScreenManager::setKeepAwake(bool keepAwake)
{
    bb::cascades::Window* window = bb::cascades::Application::instance()->mainWindow();
    if (window) {
        screen_window_t screenWindow = window->handle();
        if (screenWindow) {
            int idleMode = keepAwake ? SCREEN_IDLE_MODE_KEEP_AWAKE : SCREEN_IDLE_MODE_NORMAL;
            screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_IDLE_MODE, &idleMode);
        }
    }
}
