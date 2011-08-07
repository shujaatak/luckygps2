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

#ifndef COMPASSWIDGET_H
#define COMPASSWIDGET_H

#include <stdio.h>

#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QProgressBar>
#include <QTabWidget>
#include <QWidget>
#include <QFrame>

#include "tile.h"

class LineEditWidget : public QLineEdit
{
	Q_OBJECT

public:
	LineEditWidget(QWidget *parent = 0):QLineEdit(parent) {}

protected:
	void focusInEvent(QFocusEvent *event) { emit gotFocus(); QLineEdit::focusInEvent(event); }
	void focusOutEvent(QFocusEvent *event) { emit lostFocus(); QLineEdit::focusOutEvent(event); }

signals:
	void gotFocus();
	void lostFocus();
};

class ButtonFrameWidget : public QFrame
{
	Q_OBJECT

public:
	ButtonFrameWidget(QWidget *parent = 0);
	~ButtonFrameWidget();

protected:
	void paintEvent(QPaintEvent *event);

};

/* --------------------- */
/* MapOverviewWidget     */
/* --------------------- */
class MapOverviewWidget : public QFrame
{
	Q_OBJECT

public:
	MapOverviewWidget(QWidget *parent = 0);
	~MapOverviewWidget();
	void update_overviewMap(QImage *, int, int, int, int, int, int, int, int, float);

	QImage *_outlineMapImg;
	int _x, _y, _width, _height, _imgx, _imgy, _imgw, _imgh;
	float _factor;

protected:
	void paintEvent(QPaintEvent *event);
};
/* --------------------- */

#endif // COMPASSWIDGET_H
