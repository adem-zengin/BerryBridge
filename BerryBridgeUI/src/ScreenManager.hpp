#ifndef SCREENMANAGER_HPP
#define SCREENMANAGER_HPP

#include <QObject>

class ScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit ScreenManager(QObject *parent = 0);

    Q_INVOKABLE void setKeepAwake(bool keepAwake);
};

#endif
