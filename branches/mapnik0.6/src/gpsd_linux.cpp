/*
 * This file is part of the luckyGPS project.
 *
 * Copyright (C) 2009 - 2011 Daniel Genrich <dg@luckygps.com>
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

#include "gpsd.h"

#include <string.h> /* memset */

#include <QMessageBox>


gpsd::gpsd(QObject *parent, gps_settings_t &settings) : QObject(parent)
{
    _gpsSettings.host = settings.host;
    _gpsSettings.port = settings.port;

	memset(&_gpsData, 0, sizeof(nmeaGPS));

    connect(&_socket, SIGNAL(readyRead()), this, SLOT(callback_gpsd_read()));
    connect(&_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(callback_gpsd_error(QAbstractSocket::SocketError)));
}

bool gpsd::gps_connect()
{
    if(_socket.state() == QAbstractSocket::UnconnectedState)
    {
        /* this character signals gpsd to send data */
        char request[] = "r";
        char request2[] = "?WATCH={\"raw\":0,\"nmea\":true}";

        _gpsData.valid = false;
        _gpsData.nice_connection = false;

        /* connect to gpsd server */
        _socket.connectToHost(_gpsSettings.host, _gpsSettings.port.toInt());

        if (!_socket.waitForConnected(1000))
        {
            gpsd_close();
            return false;
        }
        else
            _gpsData.nice_connection = true;

        /* request data from gpsd server */
        _socket.write(request2, strlen(request2));

        /* read first lines received */
        QByteArray array = _socket.read(500);

		/* check if gpsd < 2.92 is present */
        if(array.length() > 0 && !array.contains("class"))
            _socket.write(request, strlen(request));
    }

    return true;
}

gpsd::~gpsd()
{
    gpsd_close();
}

void gpsd::gpsd_close()
{
    if(_socket.isValid())
    {
        _socket.disconnectFromHost();
        if(_socket.state()!=QAbstractSocket::UnconnectedState)
            _socket.waitForDisconnected();
    }

    _gpsData.valid = false;
    _gpsData.seen_valid = false;
    _gpsData.nice_connection = false;
}

bool gpsd::gpsd_update_settings(gps_settings_t &settings)
{
    _gpsSettings.host = settings.host;
    _gpsSettings.port = settings.port;

    return gps_connect();
}

void gpsd::callback_gpsd_read()
{
    int bytes_available = (int)(_socket.bytesAvailable());
    QString result(_socket.readAll());

    if(bytes_available < 6)
        return;

    parse_nmea0183(result, _gpsData);

    /* update gui */
    emit gpsd_read();
}

void gpsd::callback_gpsd_error(QAbstractSocket::SocketError socketError)
{
    if (socketError == QTcpSocket::RemoteHostClosedError || socketError == QTcpSocket::SocketTimeoutError)
    {
        gpsd_close();
        gps_connect();
        return;
    }

     gpsd_close();
}

