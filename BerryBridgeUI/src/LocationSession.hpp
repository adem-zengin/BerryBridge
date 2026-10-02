#ifndef LOCATIONSESSION_HPP
#define LOCATIONSESSION_HPP

#include <QtCore/QObject>
#include <QtLocationSubset/QGeoPositionInfo>
#include <QtLocationSubset/QGeoPositionInfoSource>

using namespace QtMobilitySubset;

class LocationSession : public QObject
{
    Q_OBJECT

    // QML tarafında koordinatları okumak için property tanımları
    Q_PROPERTY(double latitude READ latitude NOTIFY dataChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY dataChanged)

public:
    explicit LocationSession(QObject *parent = 0);
    virtual ~LocationSession();

    // QML'den çağrılabilecek metodlar
    Q_INVOKABLE void startUpdates();
    Q_INVOKABLE void stopUpdates();

    double latitude() const;
    double longitude() const;

Q_SIGNALS:
    void dataChanged();
    // Parametre isimleri property'ler ile çakışmaması için lat ve lng olarak değiştirildi
    void locationFound(double lat, double lng);

private Q_SLOTS:
    void positionUpdated(const QGeoPositionInfo &pos);

private:
    QGeoPositionInfoSource *m_positionSource;
    double m_latitude;
    double m_longitude;
};

#endif /* LOCATIONSESSION_HPP */
