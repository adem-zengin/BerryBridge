/****************************************************************************
** Meta object code from reading C++ file 'service.hpp'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.6)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/service.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'service.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_Service[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
      19,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
       9,    8,    8,    8, 0x05,

 // slots: signature, parameters, type, tag, flags
      27,    8,    8,    8, 0x08,
      76,   67,    8,    8, 0x08,
     107,    8,    8,    8, 0x08,
     129,    8,    8,    8, 0x08,
     145,    8,    8,    8, 0x08,
     170,    8,    8,    8, 0x08,
     192,    8,    8,    8, 0x08,
     210,    8,    8,    8, 0x08,
     231,    8,    8,    8, 0x08,
     261,  249,    8,    8, 0x08,
     303,    8,    8,    8, 0x08,
     323,    8,    8,    8, 0x08,
     347,    8,    8,    8, 0x08,
     384,  371,    8,    8, 0x08,

 // methods: signature, parameters, type, tag, flags
     435,    8,    8,    8, 0x02,
     450,    8,    8,    8, 0x02,
     494,  466,    8,    8, 0x02,
     577,  536,    8,    8, 0x02,

       0        // eod
};

static const char qt_meta_stringdata_Service[] = {
    "Service\0\0messagesUpdated()\0"
    "handleInvoke(bb::system::InvokeRequest)\0"
    "isOnline\0handleConnectivityChange(bool)\0"
    "performPeriodicSync()\0startSyncLoop()\0"
    "onSyncResponseReceived()\0startPushConnection()\0"
    "onPushConnected()\0onPushDisconnected()\0"
    "onPushReadyRead()\0socketError\0"
    "onPushError(QAbstractSocket::SocketError)\0"
    "onPushPingTimeout()\0onPushWatchdogTimeout()\0"
    "onStatusUpdateTimeout()\0reply,errors\0"
    "onGlobalSslErrors(QNetworkReply*,QList<QSslError>)\0"
    "pauseSyncing()\0resumeSyncing()\0"
    "msgId,chatId,previewJsonStr\0"
    "processChatAsync(QString,QString,QString)\0"
    "accountID,chatID,senderName,msgType,text\0"
    "createMessageNotification(QString,QString,QString,QString,QString)\0"
};

void Service::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        Service *_t = static_cast<Service *>(_o);
        switch (_id) {
        case 0: _t->messagesUpdated(); break;
        case 1: _t->handleInvoke((*reinterpret_cast< const bb::system::InvokeRequest(*)>(_a[1]))); break;
        case 2: _t->handleConnectivityChange((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->performPeriodicSync(); break;
        case 4: _t->startSyncLoop(); break;
        case 5: _t->onSyncResponseReceived(); break;
        case 6: _t->startPushConnection(); break;
        case 7: _t->onPushConnected(); break;
        case 8: _t->onPushDisconnected(); break;
        case 9: _t->onPushReadyRead(); break;
        case 10: _t->onPushError((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        case 11: _t->onPushPingTimeout(); break;
        case 12: _t->onPushWatchdogTimeout(); break;
        case 13: _t->onStatusUpdateTimeout(); break;
        case 14: _t->onGlobalSslErrors((*reinterpret_cast< QNetworkReply*(*)>(_a[1])),(*reinterpret_cast< const QList<QSslError>(*)>(_a[2]))); break;
        case 15: _t->pauseSyncing(); break;
        case 16: _t->resumeSyncing(); break;
        case 17: _t->processChatAsync((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 18: _t->createMessageNotification((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData Service::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject Service::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_Service,
      qt_meta_data_Service, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &Service::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *Service::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *Service::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Service))
        return static_cast<void*>(const_cast< Service*>(this));
    return QObject::qt_metacast(_clname);
}

int Service::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void Service::messagesUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
