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

#include <QPainter>

#include "glmapwidget.h"

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE  0x809D
#endif

#ifndef GL_BGR
#define GL_BGR GL_BGR_EXT
#endif
#ifndef GL_BGRA
#define GL_BGRA GL_BGRA_EXT
#endif



#ifndef Q_CC_MSVC
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
static double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#else
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
	QueryPerformanceCounter ( &liCurrentTime );
}
static double tval()
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)*(double)1000.0/(double)liFrequency.QuadPart ));
}

#endif

GLMapWidget::GLMapWidget(QWidget *parent)
	: QGLWidget(QGLFormat(QGL::SampleBuffers), parent) /* SampleBuffers */
{
	setFixedSize(200, 200);
	setAutoFillBackground(false);


	_x = 0;
	_y = 0;
	_startx = 0;
	_starty = 0;
	_zoom = 0;
	_centerMap = false;
	_cacheSize = 0;
	_outlineCacheSize = 0;
	_degree = 0.0;
	_activeTrack = false;
	_enableScrollWheel = true;
	_max_generate_zoom = 17;
	_mapImg = NULL;
	_gotMissingTiles = false;

	memset(&_gpsdata, 0, sizeof(nmeaGPS));
	memset(&_tileInfo, 0, sizeof(TileInfo));

	_tilesManager = new TileDownload(this);

	_routing = new Routing();
}

GLMapWidget::~GLMapWidget()
{
	// glDeleteLists(tile_list, 1);
	glDeleteTextures(1, &texId);

	delete _mapImg;

	delete _tilesManager;

	delete _routing;
}

#define glError() { \
	GLenum err = glGetError(); \
	while (err != GL_NO_ERROR) { \
		QString test = "glError: " + QString::fromAscii((char *)gluErrorString(err)); \
		qDebug(test.toAscii()); \
		err = glGetError(); \
	} \
}

void GLMapWidget::initializeGL()
{
	qDebug("initializeGL");

#if !defined(QT_OPENGL_ES_2)
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
#ifndef QT_OPENGL_ES
	glOrtho(0, width(), height(), 0, 0, 1);
#else
	glOrthof(0, width(), height(), 0, 0, 1);
#endif
	glMatrixMode(GL_MODELVIEW);
#endif

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	// glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, texId);
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width(), height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL ); // GL_BGRA
	glDisable(GL_TEXTURE_2D);

	glError();

	_mapImg = new QImage("e:/kamelwelt.png");

	if(!_mapImg)
		qDebug("no Image loaded");
}

void GLMapWidget::paintGL()
{
	qDebug("paintGL");

	tstart();

	int w = width();
	int h = height();

	glClear(GL_COLOR_BUFFER_BIT);
	// glCallList(tile_list);

	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, texId);
	QImage test = QGLWidget::convertToGLFormat( *_mapImg );

	// constBits
	// width + height has to be smaller than the glTexImage2d parent
	// glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, qMin(w, _mapImg->width()), qMin(h, _mapImg->height()),  GL_BGRA, GL_UNSIGNED_BYTE, _mapImg->constBits() );
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, qMin(w, test.width()), qMin(h, test.height()),  GL_RGBA, GL_UNSIGNED_BYTE, test.constBits() );

	glError();

	glBegin(GL_QUADS);
	{
		glTexCoord2i(1, 1);
		glVertex2d(0, 0);
		glTexCoord2i(0, 1);
		glVertex2d(w, 0);
		glTexCoord2i(0, 0);
		glVertex2d(w, h);
		glTexCoord2i(1, 0);
		glVertex2d(0, h);
	}
	glEnd();

	glDisable(GL_TEXTURE_2D);

	//necessary because QImage test is only available locally
	glFlush();


	// QGLWidget* ptr;GLuint texId = ptr->bindTexture( QImage( filename ), GL_TEXTURE_2D );
	// glBindTexture( GL_TEXTURE_2D, texId );

	tend();
	qDebug(QString::number(tval()).toAscii());
}

void GLMapWidget::resizeGL(int w, int h)
{
	qDebug("resizeGL");

	glViewport(0, 0, w, h);

#if !defined(QT_OPENGL_ES_2)
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
#ifndef QT_OPENGL_ES
	glOrtho(0, w, h, 0, 0, 1);
#else
	glOrthof(0, w, h, 0, 0, 1);
#endif
	glMatrixMode(GL_MODELVIEW);
#endif

}

void GLMapWidget::set_center(double latitude, double longitude, int hor_corr, bool autocenter, bool do_update)
{
	int pixel_x, pixel_y;
	double longitude_rad, latitude_rad;

	_centerMap = autocenter;

	/* convert units to radiance */
	longitude_rad = deg_to_rad(longitude);
	latitude_rad = deg_to_rad(latitude);

	/* get their "pixel" value to load correct tiles from file */
	pixel_x = longitude_to_pixel(get_zoom(), longitude_rad);
	pixel_y = latitude_to_pixel(get_zoom(), latitude_rad);

	// hor_corr = horizontal correction if e.g. a settings tab is open
	_x = pixel_x - (width() - hor_corr)/2;
	_y = pixel_y - height()/2;
}

void GLMapWidget::set_zoom(int zoom)
{
	if(zoom >= 2 && zoom <= 18)
	{
		_zoom = zoom;
	}
}

void GLMapWidget::slotRequestFinished(QNetworkReply *reply)
{
}

void GLMapWidget::callback_redraw()
{
}

void GLMapWidget::callback_fullscreen_clicked()
{
}

void GLMapWidget::callback_inet_connection_update()
{
}

void GLMapWidget::callback_routecb_getFocus()
{
}

void GLMapWidget::callback_routecb_lostFocus()
{
}

void GLMapWidget::generate_tiles(int x, int y, int zoom, int w, int h)
{
}
