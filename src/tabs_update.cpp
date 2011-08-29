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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "convertunits.h"

#include <cmath>

#define ABS(a) (((a) < 0) ? -(a) : (a))

void MainWindow::tabGpsUpdate(const nmeaGPS &gpsdata)
{
	QString info = "";

	/* position */
	info += QString::number(gpsdata.latitude, 'f', 6) + "N - ";
	info += QString::number(gpsdata.longitude, 'f', 6) + "E";
	ui->gpsLabelPosition->setText(info);

	/* altitude */
	info = QString::number(gpsdata.altitude, 'f', 1);
	ui->gpsLabelAltitude->setText(info);

	/* speed */
	int speed = gpsdata.speed * 3.6; // km/h
	if(ui->map->get_unit() == 0)
		info = QString::number(speed, 'f', 1) + " km/h   ";
	else
	{
		speed *= 0.62137; // mph
		info = QString::number(speed, 'f', 1) + " mph   ";
	}
	ui->gpsLabelSpeed->setText(info);

	/* gps position errors: hdop */
	info = QString::number(gpsdata.hdop, 'f', 1);
	if(gpsdata.hdop > 20.0)
		ui->gpsLabelHdop->setStyleSheet("color: #C80000;");
	else if(gpsdata.hdop <= 20.0 && gpsdata.hdop > 10.0)
		ui->gpsLabelHdop->setStyleSheet("color: #FF5000;");
	else if(gpsdata.hdop <= 10.0 && gpsdata.hdop > 5.0)
		ui->gpsLabelHdop->setStyleSheet("color: #FFB400;");
	else if(gpsdata.hdop <= 5.0 && gpsdata.hdop > 2.0)
		ui->gpsLabelHdop->setStyleSheet("color: #BFC43D;");
	else if(gpsdata.hdop <= 2.0 && gpsdata.hdop > 1.0)
		ui->gpsLabelHdop->setStyleSheet("color: #64C864;");
	else if(gpsdata.hdop <= 1.0)
		ui->gpsLabelHdop->setStyleSheet("color: #00C800;");
	ui->gpsLabelHdop->setText(info);

	/* gps position errors: vdop */
	info = QString::number(gpsdata.vdop, 'f', 1);
	if(gpsdata.vdop > 20.0)
		ui->gpsLabelVdop->setStyleSheet("color: #C80000;");
	else if(gpsdata.vdop <= 20.0 && gpsdata.vdop > 10.0)
		ui->gpsLabelVdop->setStyleSheet("color: #FF5000;");
	else if(gpsdata.vdop <= 10.0 && gpsdata.vdop > 5.0)
		ui->gpsLabelVdop->setStyleSheet("color: #FFB400;");
	else if(gpsdata.vdop <= 5.0 && gpsdata.vdop > 2.0)
		ui->gpsLabelVdop->setStyleSheet("color: #BFC43D;");
	else if(gpsdata.vdop <= 2.0 && gpsdata.vdop > 1.0)
		ui->gpsLabelVdop->setStyleSheet("color: #64C864;");
	else if(gpsdata.vdop <= 1.0)
		ui->gpsLabelVdop->setStyleSheet("color: #00C800;");
	ui->gpsLabelVdop->setText(info);

	/* satellites in view */
	info = QString::number(gpsdata.satellitesView);
	ui->gpsLabelSatellitesView->setText(info);

	/* satellites in use */
	info = QString::number(gpsdata.satellitesUse);
	ui->gpsLabelSatellitesUse->setText(info);

	/* satellite fix type */
	ui->gpsLabelSatelliteFix->setText(nmeaFix_to_gpxType(gpsdata.fix));

	/* heading */
	info = QString::number(gpsdata.track, 'f', 1) + "�";
	ui->gpsLabelHeading->setText(info);

	/* gps time */
	QDateTime t = QDateTime::fromTime_t(gpsdata.time);
	t = t.toUTC();
	info = t.toString("yyyy-MM-dd hh:mm:ss.zzz");
	ui->gpsLabelTime->setText(info);
}

void MainWindow::callback_tab_statistics_update()
{
	nmeaGPS gpsdata;

	/* get a copy of gpsd data */
	_gpsd->gpsd_get_data(&gpsdata);

	if(gpsdata.valid)
	{
		ui->button_left_center->setEnabled(1);
		ui->button_top_center->setEnabled(1);

		/* buttons to insert current gps coordinates into route */
		ui->route_start_gps_button->setEnabled(1);
		ui->route_end_gps_button->setEnabled(1);
		ui->poi_gps_button->setEnabled(1);
	}
	else
	{
		ui->button_left_center->setEnabled(0);
		ui->button_top_center->setEnabled(0);

		/* buttons to insert current gps coordinates into route */
		ui->route_start_gps_button->setEnabled(0);
		ui->route_end_gps_button->setEnabled(0);
		ui->poi_gps_button->setEnabled(0);
	}

	/* update gpsdata in our MapWidget */
	ui->map->set_gpsdata(&gpsdata);

	/* update position in autocenter mode */
	if(gpsdata.valid && ui->map->get_center())
	{
		int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
		ui->map->set_center(gpsdata.latitude, gpsdata.longitude, width_corr, true, false);
	}

	/* -------------------------- */
	/* update gps tab (only if gps tab is visible) */
	/* -------------------------- */
	if(gpsdata.valid && ui->buttonLeftTrack->isChecked())
	{
		tabGpsUpdate(gpsdata);
	}
	/* -------------------------- */


	/* -------------------------- */
	/* update track */
	/* -------------------------- */

	/* only update active tracks */
	if(ui->map->_track.active)
	{
		if(gpsdata.valid)
		{
			/* check that we only insert one point per timecode (1 point/sec) */
			int numpoints = ui->map->_track.segments.last().points.length();
			if((numpoints &&
				ui->map->_track.segments.last().points.last().gpsInfo.time < gpsdata.time) ||
			   !numpoints)
				ui->map->_track.insert_point(TrackPoint(gpsdata));
		}
		else
		{
			/* in case we lost gps signal, start new track segment */

			/* check if we already started a new track segment */
			int numpoints = ui->map->_track.segments.last().points.length();

			if(numpoints)
				ui->map->_track.segments.append(TrackSegment(-1));
		}
	}
	/* -------------------------- */


	/* update route */
	if(ui->map->_route.active && ui->map->_route.points.length() > 0 /* && gpsdata.valid */)
	{
		Route &route = ui->map->_route;

		// -------------------------------------------------
		//DEBUG
		// -------------------------------------------------
		// gpsdata.latitude = route.points[0].latitude;
		// gpsdata.longitude = route.points[0].longitude;
		// -------------------------------------------------

		// update route.pos
		route.getCurrentPosOnRoute(gpsdata.latitude, gpsdata.longitude);

		RoutePoint &point = route.points[route.pos];
		if(route.points.length() > 0 && (point.nextDesc >= 0 || point.desc.length() > 0))
		{
			/* Car is on edge driving to this point with a description */
			RoutePoint *nextpoint = NULL;

			if(point.desc.length() > 0)
				nextpoint = &point;
			else
				nextpoint = &route.points[point.nextDesc];

			// TODO: calc edge distance lengths in advance from nodes
			{
				RoutePoint *nextpoint2 = NULL;
				QStringList icons;
				QStringList labels;
				double distance = ABS(fast_distance_deg(gpsdata.latitude, gpsdata.longitude, point.latitude, point.longitude));

				/* Only add remaining street length if car is not already on final edge */
				if(nextpoint != &point)
					distance += point.length;
				distance *= 1000.0;

				if(nextpoint->nextDesc >= 0)
					nextpoint2 = &route.points[nextpoint->nextDesc];
				else
				{
					if(distance < 20)
					{
						route.active = false;
					}
				}

				ui->map->_routing->getInstructions(nextpoint, nextpoint2, distance, &labels, &icons, ui->map->get_unit());

				if(!labels.empty())
					ui->label_route_text->setText(labels.last());

				qDebug(QString::number(distance).toAscii().constData());
				qDebug("");
			}

			if(nextpoint->nextDesc >= 0)
			{
				ui->label_route_text2->setText(route.points[nextpoint->nextDesc].desc);
			}
			else
				ui->label_route_text2->setText("");

			/* Draw driving directions text box */
			if(!route.points.length() &&  (ui->routeFrameBottom->maximumHeight() > 0))
			{
				ui->routeFrameBottom->setMinimumHeight(0);
				ui->routeFrameBottom->setMaximumHeight(0);
			}
			else if(ui->routeFrameBottom->maximumHeight() == 0)
			{
				/* enlarge route label */
				ui->label_route_icon2->show();
				ui->label_route_text2->show();
				ui->routeFrameBottom->setMinimumHeight(0);
				ui->routeFrameBottom->setMaximumHeight(100);
			}
			else if(!ui->label_route_text2->text().length()) // only show one text field
			{
				ui->label_route_icon2->hide();
				ui->label_route_text2->hide();
			}
		}
		else
		{
			// if NOT (route.points.length() > 0 && (point.nextDesc >= 0 || point.desc.length() > 0))
		}

	}
	else
	{
		/* TODO: Emergency plan if car is e.g. in a tunnel */
	}

	ui->map->update();
}