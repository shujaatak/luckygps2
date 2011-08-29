/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2010 Daniel Genrich <dg@luckygps.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GPSD_H
#define GPSD_H

#include <QMutex>
#include <QtNetwork>
#include <QObject>
#include <QString>
#include <QTimer>

#include "nmea0183.h"

#ifdef Q_OS_WIN
#include "qextserialport.h"
#endif

#ifdef GPS_DEBUG
#include "route.h"
#endif


typedef struct gps_settings_t
{
    QString host;
    QString port;

    QString portname;
    int baudrate;
    int parity;
    int databits;
    int stopbits;
    int flow;

} gps_settings_t;

class gpsd : public QObject
{
    Q_OBJECT

public:
    gpsd(QObject *parent, gps_settings_t &settings);
    ~gpsd();

    /* function to manage gpsd connection */
    void gpsd_close();
    bool gpsd_update_settings(gps_settings_t &settings);

	void gpsd_get_data(nmeaGPS *tgpsdata)  { memcpy(tgpsdata, &_gpsData, sizeof(nmeaGPS)); }
	void gpsd_get_settings(gps_settings_t *tgpssettings)  { memcpy(tgpssettings, &_gpsSettings, sizeof(gps_settings_t)); }

#ifndef GPS_DEBUG
#ifdef Q_OS_LINUX
	QTcpSocket *gpsd_get_socket() { return &_socket; }
#endif
#else
	Route *_route;
#endif

public slots:
    bool gps_connect();

private:

#ifdef GPS_DEBUG

	/* debug interface with bogus coordinates */
	QTimer _readTimer;
	unsigned int _count;
	unsigned int _routeIndex;
	float _deltaLat;
	float _deltaLon;
#else

	#ifdef Q_OS_LINUX

		QTcpSocket _socket;

	#endif // Q_OS_LINUX

	#ifdef Q_OS_WIN

		QextSerialPort* _port;
		bool _niceRead;
		QTimer _readTimer;

	#endif // Q_OS_WIN

#endif // GPS_DEBUG

	nmeaGPS _gpsData;

	gps_settings_t _gpsSettings;

private slots:
    /* callbacks for gps management*/
    void callback_gpsd_read();

#ifndef GPS_DEBUG
#ifdef Q_OS_WIN
	void callback_gpsd_error();
#else
	void callback_gpsd_error(QAbstractSocket::SocketError socketError);
#endif
#endif

signals:
     void gpsd_read();
     void no_gps();
};

#ifndef GPS_DEBUG
#ifdef Q_OS_WIN
QStringList gps_get_portnames();
#endif
#endif
#endif // GPSD_H
