/****************************************************************************
** Meta object code from reading C++ file 'LocationSession.hpp'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.6)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/LocationSession.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LocationSession.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_LocationSession[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       2,   39, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: signature, parameters, type, tag, flags
      17,   16,   16,   16, 0x05,
      39,   31,   16,   16, 0x05,

 // slots: signature, parameters, type, tag, flags
      72,   68,   16,   16, 0x08,

 // methods: signature, parameters, type, tag, flags
     106,   16,   16,   16, 0x02,
     121,   16,   16,   16, 0x02,

 // properties: name, type, flags
     142,  135, 0x06495001,
     151,  135, 0x06495001,

 // properties: notify_signal_id
       0,
       0,

       0        // eod
};

static const char qt_meta_stringdata_LocationSession[] = {
    "LocationSession\0\0dataChanged()\0lat,lng\0"
    "locationFound(double,double)\0pos\0"
    "positionUpdated(QGeoPositionInfo)\0"
    "startUpdates()\0stopUpdates()\0double\0"
    "latitude\0longitude\0"
};

void LocationSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        LocationSession *_t = static_cast<LocationSession *>(_o);
        switch (_id) {
        case 0: _t->dataChanged(); break;
        case 1: _t->locationFound((*reinterpret_cast< double(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        case 2: _t->positionUpdated((*reinterpret_cast< const QGeoPositionInfo(*)>(_a[1]))); break;
        case 3: _t->startUpdates(); break;
        case 4: _t->stopUpdates(); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData LocationSession::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject LocationSession::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_LocationSession,
      qt_meta_data_LocationSession, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &LocationSession::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *LocationSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *LocationSession::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_LocationSession))
        return static_cast<void*>(const_cast< LocationSession*>(this));
    return QObject::qt_metacast(_clname);
}

int LocationSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = latitude(); break;
        case 1: *reinterpret_cast< double*>(_v) = longitude(); break;
        }
        _id -= 2;
    } else if (_c == QMetaObject::WriteProperty) {
        _id -= 2;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 2;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void LocationSession::dataChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}

// SIGNAL 1
void LocationSession::locationFound(double _t1, double _t2)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_END_MOC_NAMESPACE
