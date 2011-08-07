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

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QTextStream>
#include <QTransform>
#include <QUrl>


#include "customwidgets.h"
#include "tilemanagement.h"

#include <cmath>

#ifdef Q_CC_MSVC
#define snprintf _snprintf
#endif

/* ----------------------------	*/
/* ButtonFrameWidget			*/
/* ----------------------------	*/
ButtonFrameWidget::ButtonFrameWidget(QWidget *parent) : QFrame(parent)
{
}

ButtonFrameWidget::~ButtonFrameWidget()
{

}

void ButtonFrameWidget::paintEvent(QPaintEvent *event)
{
	QPainter painter;

	if(!painter.begin(this))
		return;

	painter.fillRect(0,0, width(), height(), QColor(255, 255, 255, 220));

	QPen pen  = painter.pen();
	pen.setWidth(1);
	pen.setCosmetic(true);
	pen.setColor(QColor(0, 0, 0));
	pen.setStyle(Qt::SolidLine);
	painter.setPen(pen);

	painter.drawRect(0, 0, width()-1, height()-1);

	painter.end();
}

/* --------------------- */
/* MapOverviewWidget     */
/* --------------------- */
MapOverviewWidget::MapOverviewWidget(QWidget *parent) : QFrame(parent)
{
	_outlineMapImg = NULL;
}

MapOverviewWidget::~MapOverviewWidget()
{
	//
}

void MapOverviewWidget::update_overviewMap(QImage *outlineMapImg, int x, int y, int width, int height, int imgx, int imgy, int imgw, int imgh, float factor)
{
	_outlineMapImg = outlineMapImg;

	if(!_outlineMapImg)
		return;

	_x = x;
	_y = y;
	_width = width;
	_height = height;
	_imgx = imgx;
	_imgy = imgy;
	_imgw = imgw;
	_imgh = imgh;
	_factor = factor;

	if(this->width() != _imgw)
	{
		setMinimumWidth(_imgw);
	}
	else if(this->height() != _imgh)
	{
		setMinimumHeight(_imgh);
	}
}

void MapOverviewWidget::paintEvent(QPaintEvent *event)
{
	if(!_outlineMapImg)
		return;

	QPainter painter;

	if(!painter.begin(this))
		return;

	if(_outlineMapImg)
	{
		QPen newPen = painter.pen();

		newPen.setWidth(1);
		newPen.setCosmetic(true);
		newPen.setColor(QColor(0,0,0));
		newPen.setStyle(Qt::SolidLine);
		painter.setOpacity(1.0);

		painter.setPen(newPen);

		/* disable antialiasing */
		painter.setRenderHint(QPainter::Antialiasing, false);

		painter.drawImage(0, 0, *_outlineMapImg, _imgx, _imgy, _imgw, _imgh);

		painter.drawRect(0, 0, _imgw - 1, _imgh - 1);

		/* draw detail region box */
		float minx =  _imgw/2 - (_width/2) * _factor;
		float miny = _imgh/2 - (_height/2) * _factor;
		float maxx = _width * _factor;
		float maxy = _height * _factor;

		newPen.setColor(QColor(0,0,255));
		painter.setPen(newPen);

		painter.drawRect(minx, miny, maxx, maxy);
	}

	painter.end();
}



