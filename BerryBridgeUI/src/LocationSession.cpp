#include "LocationSession.hpp"
#include <QtCore/QDebug>

LocationSession::LocationSession(QObject *parent)
    : QObject(parent)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_positionSource(QGeoPositionInfoSource::createDefaultSource(this))
{
    if (m_positionSource) {
        // Konum güncellendiğinde tetiklenecek slot bağlantısı
        connect(m_positionSource, SIGNAL(positionUpdated(const QGeoPositionInfo &)),
                this, SLOT(positionUpdated(const QGeoPositionInfo &)));

        // Cihazın tüm konum yöntemlerini (GPS, Wi-Fi, Hücresel) kullan
        m_positionSource->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
    } else {
        qWarning() << "QGeoPositionInfoSource baslatilamadi!";
    }
}

LocationSession::~LocationSession()
{
}

void LocationSession::startUpdates()
{
    if (m_positionSource) {
        m_positionSource->startUpdates();
    }
}

void LocationSession::stopUpdates()
{
    if (m_positionSource) {
        m_positionSource->stopUpdates();
    }
}

double LocationSession::latitude() const
{
    return m_latitude;
}

double LocationSession::longitude() const
{
    return m_longitude;
}

void LocationSession::positionUpdated(const QGeoPositionInfo &pos)
{
    // Koordinatları alıyoruz (isValid kontrolünü atlıyoruz)
    m_latitude = pos.coordinate().latitude();
    m_longitude = pos.coordinate().longitude();

    // Momentics konsolunda bu yazıyı kesin olarak göreceksiniz
    qDebug() << "C++ SLOT TETIKLENDI! Konum:" << m_latitude << "," << m_longitude;

    emit dataChanged();

    // Yeni isimlendirdiğimiz sinyali fırlatıyoruz
    emit locationFound(m_latitude, m_longitude);
}
