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
#include "qextserialenumerator.h"

#include <QIODevice>
#include <QList>

QStringList gps_get_portnames()
{
	QStringList list;
	QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();

	for (int i = 0; i < ports.size(); i++)
		list.append(ports[i].portName);

	return list;
}

gpsd::gpsd(QObject *parent, gps_settings_t &settings) : QObject(parent)
{
	memset(&_gpsData, 0, sizeof(nmeaGPS));

	_port = new QextSerialPort(QextSerialPort::EventDriven);
	_niceRead = false;

	// set timeout to 1 sec, since normal gps devices only sent 1 message per second (1 Hz)
	_port->setTimeout(1000);

	_gpsSettings.portname = settings.portname;
	_gpsSettings.baudrate = settings.baudrate;
	_gpsSettings.flow = settings.flow;
	_gpsSettings.parity = settings.parity;
	_gpsSettings.databits = settings.databits;
	_gpsSettings.stopbits = settings.stopbits;

	connect(_port, SIGNAL(readyRead()), this, SLOT(callback_gpsd_read()));
	connect(&_readTimer, SIGNAL(timeout()), this, SLOT(callback_gpsd_error()));
}

bool gpsd::gps_connect()
{
	bool success = false;

	if(_port->isOpen())
		return true;

	_niceRead = false;
	_gpsData.valid = false;
	_gpsData.nice_connection = false;

	_port->setBaudRate((BaudRateType)_gpsSettings.baudrate);
	_port->setDataBits((DataBitsType)_gpsSettings.databits);
	_port->setStopBits((StopBitsType)_gpsSettings.stopbits);
	_port->setFlowControl((FlowType)_gpsSettings.flow);
	_port->setParity((ParityType)_gpsSettings.parity);

	if(_gpsSettings.portname.length() == 0 || _gpsSettings.portname == "AUTO")
	{
		/* try *simple* autodetection here */
		QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();

		/* first round: try to find known gps devices via friendly name */
		for (int i = ports.size() - 1; i >=0 ; i--)
		{
			if(ports[i].friendName.contains("u-blox") || ports[i].friendName.contains("gps"))
			{
				_port->setPortName(ports[i].portName);

				if(_port->open(QIODevice::ReadOnly)) // | QIODevice::Unbuffered
				{
					success = true;
					break;
				}
			}
		}

		/* now try to open the first one available, starting from bottom to top (meaning: first COM9 then COM1) */
		if(!success)
			for (int i = ports.size() - 1; i >=0 ; i--)
			{
				_port->setPortName(ports[i].portName);

				if(_port->open(QIODevice::ReadOnly)) // | QIODevice::Unbuffered
				{
					success = true;
					break;
				}
			}
	}
	else
	{
		_port->setPortName(_gpsSettings.portname);
		if(_port->open(QIODevice::ReadOnly))
			success = true;
	}


	if(!success) // | QIODevice::Unbuffered
	{
		printf("Cannot find gps device.\n");
		return false;
	}

	_gpsData.nice_connection = true;

	_readTimer.start(1500);

	return true;
}

void gpsd::callback_gpsd_error()
{
	if(!_niceRead)
	{
		gpsd_close();
		emit no_gps();
		// QTimer::singleShot(2000, this, SLOT(gps_connect()));
	}
}

void gpsd::gpsd_close()
{
	if(_port && _port->isOpen())
		_port->close();

	_gpsData.valid = false;
	_gpsData.seen_valid = false;
	_gpsData.nice_connection = false;
}

gpsd::~gpsd()
{
	gpsd_close();

	if(_port)
		delete _port;
}

bool gpsd::gpsd_update_settings(gps_settings_t &settings)
{
	_gpsSettings.portname = settings.portname;
	_gpsSettings.baudrate = settings.baudrate;
	_gpsSettings.flow = settings.flow;
	_gpsSettings.parity = settings.parity;
	_gpsSettings.databits = settings.databits;
	_gpsSettings.stopbits = settings.stopbits;

	return gps_connect();
}

void gpsd::callback_gpsd_read()
{
	int avail = _port->bytesAvailable();
	if( avail > 0 )
	{
		QByteArray usbdata;
		usbdata.resize(avail + 1);
		int read = _port->read(usbdata.data(), usbdata.size() - 1);

		if( read > 0 )
		{
			int n = 0;

			usbdata[read] = '\0';

			/* skip nonsense data */
			while(n < read && usbdata[n] != '$') n++;

			QString msg = usbdata.right(read - n + 1);

			if(parse_nmea0183(msg, _gpsData))
			{
				emit gpsd_read();
				_niceRead = true;
			}
		}
	}
}
