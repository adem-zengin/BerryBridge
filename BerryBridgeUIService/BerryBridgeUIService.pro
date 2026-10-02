APP_NAME = BerryBridgeUIService


CONFIG += qt warn_on

include(config.pri)

LIBS += -lbb -lbbsystem -lbbplatform
QT += network
QT += cascades sql
LIBS += -lbbdata