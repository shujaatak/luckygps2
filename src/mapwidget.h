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

#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QFrame>
#include <QMouseEvent>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QWidget>

#include "gpsd.h"
#include "datasourcemanager.h"
#include "tiledownload.h"
#include "route.h"
#include "routing.h"
#include "track.h"


class MapWidget : public QFrame
{
	Q_OBJECT

public:
	MapWidget(QWidget *parent = 0);
	~MapWidget();

	/* access to private variables */
	int get_zoom() { return _zoom; }
	QPoint get_pos() { return QPoint(_x,_y); }
	void set_pos(QPoint point) { _x = point.x(); _y = point.y(); }
	void force_redraw() { /* _gotMissingTiles = true; */ repaint(); }

	/* widget functions */
	void set_center(double latitude, double longitude, int hor_corr, bool autocenter, bool do_update = true);
	void set_center(bool autocenter) { _centerMap = autocenter; }
	bool get_center() { return _centerMap; }
	void set_zoom(int value);
	void set_scroll_wheel(bool value) { _enableScrollWheel = value; }
	void set_mirror(int value) { _mirror = (bool)value; }
	int get_mirror() { return _mirror; }
	void set_unit(int value) {_unit = value; }
	int get_unit() { return _unit; }

	/* update gps data for map, compass and information display on MapWidget */
	void set_gpsdata(nmeaGPS *gpsdata) { memcpy(&_gpsdata, gpsdata, sizeof(nmeaGPS)); }

	/* generate tiles for a specific area and zoom level */
	void generate_tiles(int x, int y, int zoom, int w, int h);
	void set_max_generate_tiles(int t) { _max_generate_zoom = t; }
	int get_max_generate_tiles() { return _max_generate_zoom; }

protected:
	void paintEvent(QPaintEvent *event);
	void mousePressEvent( QMouseEvent *event );
	void mouseMoveEvent( QMouseEvent *event );
	void mouseReleaseEvent( QMouseEvent *event );
	void wheelEvent(QWheelEvent *event);

private:
	void drawPolyline(QPainter* painter, const QRect& boundingBox, QPoint *points, int size);

	QToolButton *_fullscreenButton;
	bool _sameevent;

	DataSourceManager *_dsm;

	/* handle map movement by mouse drag-and-drop */
	int _x, _y, _startx, _starty, _zoom;

	/* center the map automatically? */
	bool _centerMap;

	/* mirror map display (e.g. for cheap HUD) */
	bool _mirror;

	/* compass management */
	double _degree;

	/* position drawing management */
	double _lat, _lon;

	/* unit */
	int _unit; // 0 == metrical, 1 == imperial

	/* minimum UI redraw timer */
	QTimer _redrawTimer;

	/* gps data for map drawing */
	nmeaGPS _gpsdata;

	/* enable / disable scroll wheel event */
	bool _enableScrollWheel;

	/* max zoom level to generate */
	int _max_generate_zoom;

	/* cache UI png's in pixmaps */
	QImage _rsImage, _rfImage, _posImage, _posinvImage, _arrowImage, _cbImage, _cfImage;

	/* interaction with navigation combobox */
	bool _cbInFocus;
	bool _grabGPSCoords;

	/* map state: fullscreen = 1 / normal = 0 */
	bool _mapStyle;

	/* Supoprt fast redraw on mouse movement */
	QImage *_painterImg;
	int _mapRedrawCounter;

public:

	/* routing info box */
	int _routingInfoHeight;

	/* track management */
	Track _track;
	bool _activeTrack; /* TODO: make this private var? */

	/* route management */
	Route _route;
	Routing *_routing;

private slots:

	/* full map redraw timer */
	void callback_redraw();

	/* support direct selection of gps position on the map */
	void callback_routecb_getFocus();
	void callback_routecb_lostFocus();

	void callback_fullscreen_clicked();

signals:
	void zoom_level_changed(int old_zoom, int new_zoom);
	void map_dragged();
	void map_clicked(double lon, double lat);
	void newTiles(TileListP);
	void fullscreen_clicked();

};

#endif // MAPWIDGET_H
