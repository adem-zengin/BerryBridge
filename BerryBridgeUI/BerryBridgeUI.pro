APP_NAME = BerryBridgeUI

CONFIG += qt warn_on cascades10

include(config.pri)

LIBS += -lbb -lbbsystem
LIBS += -lbbdata -lbbdevice
LIBS += -lbbpim
LIBS += -lbbplatform
QT += network
LIBS += -lQtLocationSubset
LIBS += -lcurl
LIBS += -lbbcascadespickers
QT += gui