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
#include "import_export.h"
#include "tilemanagement.h"
#include "system_helpers.h"

#include <QDesktopWidget>
#include <QPoint>
#include <QProcess>


MainWindow::MainWindow(QWidget *parent, int local)
	: QMainWindow(parent), ui(new Ui::MainWindow)
{
	_local = local;

	/* register new meta type to be able to use it in signals/slots */
	qRegisterMetaType<TileListP>("TileListP");

	/* Get desktop screen size to determinate mobile/vertical/horizontal mode */
	QRect desktop = QDesktopWidget().availableGeometry();

	ui->setupUi(this);

	/* fix UI (remove additional frames, margins, etc) */
	{
		ui->frameRoute->setVisible(0);
		ui->frameSettings->setVisible(0);
		ui->frameTrack->setVisible(0);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(0, top, right, bottom);

		/* disable POI for now */
		ui->groupBox->setVisible(0);
	}

	_gpsd = NULL;
	_tabOpen = false;
	_db = NULL;
	_routeDb = NULL;
	_mobileMode = false;

	/* NOT SUPORTED AT THE MOMENT! TODO */
	/* do screen resolution check and setup screen accordingly */
	if (desktop.height() >= desktop.width())
	{
		/* kill left + right button list */
		/* this is helpfull for mobile devices like openmoko */
		/* TODO: finish this/make it work nicely */

		/* left buttons */
		ui->button_left_center->setVisible(0);
		ui->button_left_minus->setVisible(0);
		ui->button_left_plus->setVisible(0);

		if(desktop.width() == 480)
			setGeometry(0, 0, 480, 640);

		_mobileMode = true;
	}
	else
	{
		/* kill top button list */
		ui->button_top_center->setVisible(0);
		ui->button_top_plus->setVisible(0);
		ui->button_top_minus->setVisible(0);

		if(desktop.height() <= 640 && desktop.width() <= 1024)
		{
			/* Show maximized window on small horizontal screens */
			this->showMaximized();

			// _mobileMode = true;
		}
	}

	/* open settings, tracks, etc. database */
	{
		QString dbname =  getDataHome(_local) + "/luckygps.sqlite";
		// QFile::remove(dbname); /* TODO: put version into db */

		/* check if configuration database was already created */
		if(!QFile::exists(dbname))
		{
			/* extract initial configuration db from resources */
			QFile::copy(":/data/luckygps.sqlite", dbname);
			QFile::setPermissions(dbname, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
		}

		if(sqlite3_open(dbname.toUtf8(), &_db) != SQLITE_OK)
			{
			/* extract initial configuration db from resources */
			QFile::remove(dbname);
			QFile::copy(":/data/luckygps.sqlite", dbname);
			QFile::setPermissions(dbname, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

			if(sqlite3_open(dbname.toUtf8().constData(), &_db) != SQLITE_OK)
		{
				QString message = "Error: Cannot open luckygps.sqlite. Please check permision to write in " + getDataHome(_local);
				printf("%s\n", message.toAscii().constData());
			exit(0);
			}
		}
		else
		{
			/* TODO: better handling of old databases, but in the moment, just delete incompatible ones */
			if(!checkDBversion())
			{
				sqlite3_close(_db);
				if(QFile::remove(dbname))
				{
					QFile::copy(":/data/luckygps.sqlite", dbname);
					QFile::setPermissions(dbname, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

					if(sqlite3_open(dbname.toUtf8().constData(), &_db) != SQLITE_OK)
					{
						QString message = "Error: Cannot open luckygps.sqlite. Please check permision to write in " + getDataHome(_local);
						printf("%s\n", message.toAscii().constData());
						exit(0);
					}
				}
				else
				{
					QString message = "Error: Cannot remove luckygps.sqlite. Please check permision to write in " + getDataHome(_local);
					printf("%s\n", message.toAscii().constData());
					exit(0);
				}
			}
		}

		sqlite3_exec(_db, "PRAGMA synchronous=OFF;", 0, NULL, 0);
		/* 10 mb cache */
		sqlite3_exec(_db, "PRAGMA cache_size=10000;", 0, NULL, 0);
	}

	/* needs to be initialized before loadSettings() */
	_dsm = new DataSourceManager();

	/* load GUI standard settings from luckygps.sqlite database */
	gps_settings_t settings;
	loadSettings(&settings, 0, 1); /* TODO: check return value */

	int ret = ui->map->_routing->init(getDataHome(_local));
	if(ret < 0)
	{
		qDebug() << "No routing data found. Continuing without routing feature.";
	}
	else if(ret == 0)
	{
		qDebug() << "Cannot load routing plugins!";
	}
	if(ret <= 0)
	{
		// TODO: disabling offline/live routing panel here
	}

	/* this is the case if we have no old settings */
	if(ui->map->get_zoom() <= 0)
	{
		ui->map->set_zoom(3);
		ui->button_left_minus->setEnabled(false);
		ui->button_top_minus->setEnabled(false);
	}
	else if(ui->map->get_zoom() >= 18)
	{
		ui->map->set_zoom(18);
		ui->button_left_plus->setEnabled(false);
		ui->button_top_plus->setEnabled(false);
	}

	/* init gpsd */
	_gpsd = new gpsd(this, settings);
	connect(_gpsd, SIGNAL(gpsd_read()), this, SLOT(callback_tab_statistics_update()));
	connect(_gpsd, SIGNAL(no_gps()), this, SLOT(callback_tab_statistics_update()));

	// clear GUI
	clearGpsInfo();

	_gpsd->gps_connect();

	/* setup route textedit completer */
	QCompleter *completer = new QCompleter(this);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setModel(new QStringListModel());
	ui->editRoutingStartCity->setCompleter(completer);
	connect(completer, SIGNAL(activated(const QString &)), this, SLOT(callbackRouteStartCompleterCity(const QString &)));
	completer = new QCompleter(this);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setModel(new QStringListModel());
	ui->editRoutingStartStreet->setCompleter(completer);
	connect(completer, SIGNAL(activated(const QString &)), this, SLOT(callbackRouteStartCompleterStreet(const QString &)));
	completer = new QCompleter(this);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setModel(new QStringListModel());
	ui->editRoutingDestCity->setCompleter(completer);
	connect(completer, SIGNAL(activated(const QString &)), this, SLOT(callbackRouteDestCompleterCity(const QString &)));
	completer = new QCompleter(this);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	completer->setModel(new QStringListModel());
	ui->editRoutingDestStreet->setCompleter(completer);
	connect(completer, SIGNAL(activated(const QString &)), this, SLOT(callbackRouteDestCompleterStreet(const QString &)));

	/* connect map widget signals */
	connect(ui->map, SIGNAL(zoom_level_changed(int, int)), this, SLOT(zoom_buttons_clicked(int, int)));
	connect(ui->map, SIGNAL(map_dragged()), this, SLOT(callback_center_toggle()));
	connect(ui->map, SIGNAL(fullscreen_clicked()), this, SLOT(callback_fullscreen()));
	connect(ui->map, SIGNAL(map_clicked(double, double)), this, SLOT(callback_map_clicked(double, double)));

	/* we want the input selected on focusing the QComboBoxes in the routing tab */
	connect(ui->lineEditRouteStartGPS, SIGNAL(gotFocus()), ui->map, SLOT(callback_routecb_getFocus()));
	connect(ui->lineEditRouteStartGPS, SIGNAL(lostFocus()), ui->map, SLOT(callback_routecb_lostFocus()));
	connect(ui->lineEditRouteDestinationGPS, SIGNAL(gotFocus()), ui->map, SLOT(callback_routecb_getFocus()));
	connect(ui->lineEditRouteDestinationGPS, SIGNAL(lostFocus()), ui->map, SLOT(callback_routecb_lostFocus()));
}

MainWindow::~MainWindow()
{
	/* save open track */
	Track &track = ui->map->_track;
	if(track.active)
	{
		saveTrack(&track);
		track.active = false;
	}
	ui->map->_activeTrack = false;

	/* we save last pos + zoom here */
	saveSettings();

	delete ui;

	/* DataSourceManager needs to be deleted after UI */
	delete _dsm;

	/* shutdown gpsd */
	delete _gpsd;

	/* close database (settings, tracks, etc.) connection */
	sqlite3_close(_db);
	sqlite3_close(_routeDb);

	sqlite3_reset_auto_extension ();
}

void MainWindow::on_button_left_plus_clicked()
{
	int old_zoom = ui->map->get_zoom();
	int new_zoom;
	QPoint point;

	ui->map->set_zoom(old_zoom + 1);
	new_zoom = ui->map->get_zoom();

	zoom_buttons_clicked(old_zoom, new_zoom);
}

void MainWindow::on_button_left_minus_clicked()
{
	int old_zoom = ui->map->get_zoom();
	int new_zoom;

	ui->map->set_zoom(old_zoom - 1);
	new_zoom = ui->map->get_zoom();

	zoom_buttons_clicked(old_zoom, new_zoom);
}

void MainWindow::handleZoomButtons(int new_zoom)
{
	if(new_zoom == 2)
	{
		ui->button_left_minus->setEnabled(false);
		ui->button_top_minus->setEnabled(false);
	}
	else if(new_zoom == 18)
	{
		ui->button_left_plus->setEnabled(false);
		ui->button_top_plus->setEnabled(false);
	}
	else
	{
		ui->button_left_minus->setEnabled(true);
		ui->button_left_plus->setEnabled(true);

		ui->button_top_minus->setEnabled(true);
		ui->button_top_plus->setEnabled(true);
	}
}

void MainWindow::zoom_buttons_clicked(int old_zoom, int new_zoom)
{
	float factor;
	QPoint point;
	nmeaGPS gpsdata;

	/* old_zoom < 0 is used to indicate scroll wheel signal from mapwidget.cpp */
	if(old_zoom >= 0)
	{
		factor = exp(new_zoom * M_LN2) / exp(old_zoom * M_LN2);

		point = ui->map->get_pos();

		point.setX(((point.x() + ui->map->width()/2) * factor) - ui->map->width()/2);
		point.setY(((point.y() + ui->map->height()/2) * factor) - ui->map->height()/2);

		ui->map->set_pos(point);
	}

	/* update zoom in/out button status (enabled/disabled) according to new zoom */
	handleZoomButtons(new_zoom);

	/* get a copy of gpsd data */
	_gpsd->gpsd_get_data(&gpsdata);

	/* do instant center if we zoom in or out if we have autozoom enabled */
	if(gpsdata.valid && ui->map->get_center())
	{
		int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
		ui->map->set_center(gpsdata.latitude, gpsdata.longitude, width_corr, true);
	}
	else
	{
		ui->map->update();
	}
}

void MainWindow::callback_map_clicked(double lon, double lat)
{
	if(ui->lineEditRouteStartGPS->hasFocus())
	{
		QString gps = QString::number(lat, 'f', 6) + ", " + QString::number(lon, 'f', 6);
		ui->lineEditRouteStartGPS->setText(gps);
	}
	else if(ui->lineEditRouteDestinationGPS->hasFocus())
	{
		QString gps = QString::number(lat, 'f', 6) + ", " + QString::number(lon, 'f', 6);
		ui->lineEditRouteDestinationGPS->setText(gps);
	}
}

void MainWindow::callback_center_toggle()
{
	ui->button_left_center->setChecked(false);
	ui->button_top_center->setChecked(false);
}

void MainWindow::on_button_left_center_toggled(bool checked)
{
	nmeaGPS gpsdata;

	/* get a copy of gpsd data */
	_gpsd->gpsd_get_data(&gpsdata);

	if(gpsdata.valid && checked)
	{
		int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
		ui->map->set_center(gpsdata.latitude, gpsdata.longitude, width_corr, true);
	}
	else
		ui->map->set_center(checked);
}

void MainWindow::on_settings_cache_spinbox_valueChanged(int value)
{
	value = ceil(value * 0.08 + 0.5);
	ui->label_cache_memusage->setText("(~ " + QString::number(value) + " MB)");
}

void MainWindow::on_button_settings_reset_clicked()
{
	gps_settings_t settings;

	loadSettings(&settings);

	ui->map->force_redraw();
}

void MainWindow::on_button_settings_defaults_clicked()
{
	gps_settings_t settings;

	loadSettings(&settings, 1);

	ui->map->force_redraw();
}

void MainWindow::on_button_settings_save_clicked()
{
	gps_settings_t settings;

	saveSettings();
	loadSettings(&settings);

	ui->map->force_redraw();
}

void MainWindow::on_trackNewButton_clicked()
{
	/* save track to file if any is active */
	if(ui->map->_activeTrack)
	{
		qDebug("on_trackNewButton_clicked - _activeTrack");

		Track &track = ui->map->_track;
		if(track.active)
		{
			saveTrack(&track);
			track.active = false;

			qDebug("on_trackNewButton_clicked - saveTrack");
		}
		ui->map->_activeTrack = false;
	}

	ui->map->_activeTrack = true;
	ui->trackConStopButton->setEnabled(true);
	ui->trackConStopButton->setText(tr("Stop"));

	QDateTime datetime = QDateTime::currentDateTime ();
	QString trackName = datetime.toString("yyyyMMdd-hhmmss");
	ui->map->_track = Track(-1, trackName, TrackSegment(-1), true);

	/* save track to fool track folder update function */
	ui->map->_track.filename = saveTrack(&ui->map->_track);

	/* select new track in combobox */
	ui->trackImportCb->addItem(ui->map->_track.filename);

	ui->trackImportCb->setCurrentIndex(ui->trackImportCb->count() - 1);
	tabfolderUpdate();
}


void MainWindow::startstopTrack(bool value)
{
	/* save track to file if any is active */
	if(!value)
	{
		Track &track = ui->map->_track;
		if(track.active)
		{
			saveTrack(&track);
			track.active = false;
		}
		ui->map->_activeTrack = false;
		ui->trackConStopButton->setText(tr("Continue"));
	}
	else
	{
		/* continue an existing track */
		ui->map->_activeTrack = true;

		ui->trackConStopButton->setText(tr("Stop"));

		/* set selected track as active */
		ui->map->_track.active = true;

		/* insert new TrackSegment on continue */
		ui->map->_track.segments.append(TrackSegment(-1));
	}
}

void MainWindow::on_trackConStopButton_clicked()
{
	/* save track to file if any is active */
	if(ui->map->_activeTrack)
	{
		startstopTrack(false);
	}
	else
	{
		startstopTrack(true);
	}
}

void MainWindow::on_routeImportButton_clicked()
{
	QDir route_path;

	/* get all route files in folder */
	route_path.setPath(_routePath);

	if(route_path.exists())
	{
		int i = 0;
		QFileInfoList file_list = route_path.entryInfoList(QStringList("*.gpx"), QDir::Files);
		// TODO: check if we got really a route gpx file!
		for(i = 0; i < file_list.length(); i++)
			if(file_list[i].fileName() == ui->routeImportCb->currentText())
				break;

		if(i < file_list.length())
		{
			import_route(ui->map->_route, file_list[i].filePath());

			if(ui->map->_route.points.length() > 0)
			{
				/* get zoom which covers whole route + center it + deactivate autocenter */
				int zoom = get_route_zoom(ui->map->width(), ui->map->height()-70, ui->map->_route.bb[3], ui->map->_route.bb[0], ui->map->_route.bb[1], ui->map->_route.bb[2]);
				ui->map->set_zoom(zoom);
				zoom = ui->map->_route.zoom = ui->map->get_zoom();

				/* update zoom in/out button status (enabled/disabled) according to new zoom */
				int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
				handleZoomButtons(zoom);
				ui->map->set_center((ui->map->_route.bb[1] + ui->map->_route.bb[3])*0.5, (ui->map->_route.bb[0] + ui->map->_route.bb[2])*0.5, width_corr, false);

				/* disable autocenter button */
				callback_center_toggle();
			}
		}
	}
}

void MainWindow::on_button_calc_route_clicked()
{
	if(ui->map->_routing->checkPos())
	{
		bool ret = ui->map->_routing->calculateRoute(ui->map->_route, ui->map->get_unit());
		if(ret)
		{
			/* get zoom which covers whole route + center it + deactivate autocenter */
			int zoom = get_route_zoom(ui->map->width(), ui->map->height()-70, ui->map->_route.bb[3], ui->map->_route.bb[0], ui->map->_route.bb[1], ui->map->_route.bb[2]);
			ui->map->set_zoom(zoom);
			zoom = ui->map->_route.zoom = ui->map->get_zoom();

			/* update zoom in/out button status (enabled/disabled) according to new zoom */
			int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
			handleZoomButtons(zoom);
			ui->map->set_center((ui->map->_route.bb[1] + ui->map->_route.bb[3])*0.5, (ui->map->_route.bb[0] + ui->map->_route.bb[2])*0.5, width_corr, false);

			/* disable autocenter button */
			callback_center_toggle();
		}
	}
}

void MainWindow::on_button_top_center_toggled(bool checked)
{
	on_button_left_center_toggled(checked);
}

void MainWindow::on_button_top_plus_clicked()
{
	on_button_left_plus_clicked();
}

void MainWindow::on_button_top_minus_clicked()
{
	on_button_left_minus_clicked();
}

void MainWindow::on_button_generate_tiles_clicked()
{
	double lon1 = pixel_to_longitude(ui->map->get_zoom(), ui->map->get_pos().x());
	double lon2 = pixel_to_longitude(ui->map->get_zoom(), ui->map->get_pos().x() + ui->map->width());
	double lat1 = pixel_to_latitude(ui->map->get_zoom(), ui->map->get_pos().y());
	double lat2 = pixel_to_latitude(ui->map->get_zoom(), ui->map->get_pos().y() + ui->map->height());

	for(int j = ui->map->get_max_generate_tiles(); j >= ui->map->get_zoom(); j--)
	{
		int x0 = longitude_to_pixel(j, lon1);
		int x1 = longitude_to_pixel(j, lon2);
		int y0 = latitude_to_pixel(j, lat1);
		int y1 = latitude_to_pixel(j, lat2);

		ui->map->generate_tiles(MIN(x0, x1), MIN(y0, y1), j, ABS(x1-x0), ABS(y1-y0));
	}

#if 0 /* map tile queue function in conjunction with a route */
	for(int j = ui->map->get_max_generate_tiles(); j >= ui->map->_route.zoom; j--)
	{
		int x0 = longitude_to_pixel(j, deg_to_rad(ui->map->_route.bb[0]));
		int x1 = longitude_to_pixel(j, deg_to_rad(ui->map->_route.bb[2]));
		int y0 = latitude_to_pixel(j, deg_to_rad(ui->map->_route.bb[1]));
		int y1 = latitude_to_pixel(j, deg_to_rad(ui->map->_route.bb[3]));

		ui->map->generate_tiles(MIN(x0, x1), MIN(y0, y1), j, ABS(x1-x0), ABS(y1-y0));
	}
#endif
}

void MainWindow::on_button_browsemapfolder_clicked()
{
	QString path = QFileDialog::getExistingDirectory (this, tr("Directory"), ui->label_map_path->text()); //  QDir::homePath()
	if ( path.isNull() == false )
	{
		ui->label_map_path->setText(path);
	}
}

/* Update the corresponding combobox entries */
/* on the UI if a new GPX file was added     */
/* to the route or tracks folder             */
// TODO: remove and outsource the combobox filling function for tracks
void MainWindow::tabfolderUpdate()
{
	int currentIndex = -1;

	if(ui->buttonLeftTrack->isChecked())
	{
		QString currentItem = ui->trackImportCb->currentText();
		int oldIndex = ui->trackImportCb->currentIndex();
		QDir trackPathDir;

		/* get all track files in folder */
		trackPathDir.setPath(_trackPath);

		if(trackPathDir.exists())
		{
			int correct = 1;
			QFileInfoList fileList = trackPathDir.entryInfoList(loadTrackFormats(), QDir::Files);

			/* check if any new files are there */
			for(int i = 0; i < fileList.length(); i++)
			{
				correct &= (ui->trackImportCb->findText(fileList[i].fileName()) >= 0);
			}

			if(!correct)
			{
				ui->trackImportCb->clear();
				ui->trackImportCb->addItem(tr("None"));

				for(int i = 0; i < fileList.length(); i++)
				{
					ui->trackImportCb->addItem(fileList[i].fileName());
					if(oldIndex > 0 && currentIndex < 0 && (currentItem == fileList[i].fileName()))
					{
						currentIndex = i + 1;
						ui->trackImportCb->setCurrentIndex(i + 1);
					}
				}
			}
		}
		else
		{
			QDir dir;
			dir.mkpath(_trackPath);

			ui->trackImportCb->clear();
			ui->trackImportCb->addItem(tr("None"));
		}
	}
	else if(ui->buttonLeftRoute->isChecked())
	{
		QString currentItem = ui->routeImportCb->currentText();
		QDir route_path_dir;

		ui->routeImportCb->clear();

		/* get all route files in folder */
		route_path_dir.setPath(_routePath);

		if(route_path_dir.exists())
		{
			QFileInfoList file_list = route_path_dir.entryInfoList(QStringList("*.gpx"), QDir::Files);

			for(int i = 0; i < file_list.length(); i++)
			{
				ui->routeImportCb->addItem(file_list[i].fileName());
				if(currentIndex < 0 && (currentItem == file_list[i].fileName()))
				{
					currentIndex = i;
					ui->routeImportCb->setCurrentIndex(i);
				}
			}
		}
		else
		{
			QDir dir;
			dir.mkpath(_routePath);
		}

		if(ui->routeImportCb->count() == 0)
		{
			ui->routeImportCb->addItem(tr("None"));
			ui->routeImportButton->setEnabled(false);
		}
		else
			ui->routeImportButton->setEnabled(true);
	}
}

void MainWindow::on_button_browsetrackfolder_clicked()
{
	QString path = QFileDialog::getExistingDirectory (this, tr("Directory"), ui->label_track_path->text()); //  QDir::homePath()
	if ( path.isNull() == false )
	{
		ui->label_track_path->setText(path);
	}
}

void MainWindow::on_button_browseroutefolder_clicked()
{
	QString path = QFileDialog::getExistingDirectory (this, tr("Directory"), ui->label_route_path->text()); //  QDir::homePath()
	if ( path.isNull() == false )
	{
		ui->label_route_path->setText(path);
	}
}

void MainWindow::on_trackImportCb_activated(int index)
{
	QDir track_path_dir;

	/* internal changed the index */
	if(index == -1)
		return;

	/* Don't do anything if the current item didn't change */
	if(ui->map->_track.filename == ui->trackImportCb->currentText())
		return;

	/* save old active track */
	/* reset UI button states */
	startstopTrack(false);
	ui->trackConStopButton->setEnabled(false);

	if(index == 0) /* Return if "None" was selected */
	{
		ui->map->_track = Track();
		ui->map->_activeTrack = false;
		return;
	}

	/* get all track files in folder */
	track_path_dir.setPath(_trackPath);

	if(track_path_dir.exists())
	{
		int i = 0;
		QFileInfoList file_list = track_path_dir.entryInfoList(loadTrackFormats(), QDir::Files);
		for(i = 0; i < file_list.length(); i++)
			if(file_list[i].fileName() == ui->trackImportCb->currentText())
				break;

		/* check if a matching track name was found - otherwise the file got deleted in the meantime */
		if(i < file_list.length())
		{
			QString filename = file_list[i].filePath();

			Track track;
			QString suffix = file_list[i].suffix().toLower();

			/* read file according to suffix*/
			if(suffix == "gpx")
			{
				qDebug("found gpx file");
				import_track(track, filename);
			}
			/*
			else if(suffix == "log")
			{
				// TODO import_log(data);
			}
			*/

			/* Successfully imported a track */
			if(track.name.length() > 0)
			{
				ui->map->_track = track;
				ui->map->_track.filename = file_list[i].fileName();
				ui->trackConStopButton->setEnabled(true);

				/* get zoom which covers whole route + center it + deactivate autocenter */
				int zoom = get_route_zoom(ui->map->width(), ui->map->height()-70, ui->map->_track.bb[3], ui->map->_track.bb[0], ui->map->_track.bb[1], ui->map->_track.bb[2]);
				ui->map->set_zoom(zoom);
				zoom = ui->map->get_zoom();

				/* update zoom in/out button status (enabled/disabled) according to new zoom */
				int width_corr = (!_mobileMode && (ui->buttonLeftTrack->isChecked() || ui->buttonLeftRoute->isChecked() || ui->buttonLeftSettings->isChecked())) ? 302: 0;
				handleZoomButtons(zoom);
				ui->map->set_center((ui->map->_track.bb[1] + ui->map->_track.bb[3])*0.5, (ui->map->_track.bb[0] + ui->map->_track.bb[2])*0.5, width_corr, false);

				/* disable autocenter button */
				callback_center_toggle();
			}
		}
	}
	else
		qDebug("Cannot find track folder!\n");
}

void MainWindow::clearGpsInfo()
{
	ui->gpsLabelAltitude->setText("");
	ui->gpsLabelHdop->setText("");
	ui->gpsLabelHeading->setText("");
	ui->gpsLabelPosition->setText("");
	ui->gpsLabelSatelliteFix->setText("");
	ui->gpsLabelSatellitesUse->setText("");
	ui->gpsLabelSatellitesView->setText("");
	ui->gpsLabelSpeed->setText("");
	ui->gpsLabelTime->setText("");
	ui->gpsLabelVdop->setText("");
}

void MainWindow::on_buttonLeftTrack_toggled(bool checked)
{
	QRect desktop = QDesktopWidget().availableGeometry();
	if(checked)
	{
		ui->buttonLeftRoute->setChecked(false);
		ui->buttonLeftSettings->setChecked(false);

		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(0);
		}
		ui->overviewMapFrame->setVisible(0);

		nmeaGPS gpsdata;
		_gpsd->gpsd_get_data(&gpsdata);
		tabGpsUpdate(gpsdata);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(4, top, right, bottom);

		ui->stackedWidget->setCurrentIndex(0);

		tabfolderUpdate();
	}
	else
	{
		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(1);
		}
		ui->overviewMapFrame->setVisible(1);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(0, top, right, bottom);
	}

	ui->frameTrack->setVisible(checked);
}

void MainWindow::on_buttonLeftRoute_toggled(bool checked)
{
	QRect desktop = QDesktopWidget().availableGeometry();
	if(checked)
	{
		QDir route_path_dir;

		ui->buttonLeftTrack->setChecked(false);
		ui->buttonLeftSettings->setChecked(false);

		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(0);
		}
		ui->overviewMapFrame->setVisible(0);

		ui->routeImportCb->clear();

		/* get all route files in folder */
		route_path_dir.setPath(_routePath);

		if(route_path_dir.exists())
		{
			QFileInfoList file_list = route_path_dir.entryInfoList(QStringList("*.gpx"), QDir::Files);
			// TODO: check if we got really a route gpx file!
			for(int i = 0; i < file_list.length(); i++)
				ui->routeImportCb->addItem(file_list[i].fileName());
		}
		else
		{
			QDir dir;
			dir.mkpath(_routePath);
		}

		if(ui->routeImportCb->count() == 0)
		{
			ui->routeImportCb->addItem(tr("None"));
			ui->routeImportButton->setEnabled(false);
		}
		else
			ui->routeImportButton->setEnabled(true);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(4, top, right, bottom);

		ui->stackedWidget->setCurrentIndex(1);
	}
	else
	{
		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(1);
		}
		ui->overviewMapFrame->setVisible(1);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(0, top, right, bottom);
	}

	ui->frameRoute->setVisible(checked);
}

void MainWindow::on_buttonLeftSettings_toggled(bool checked)
{
	QRect desktop = QDesktopWidget().availableGeometry();
	if(checked)
	{
		ui->buttonLeftTrack->setChecked(false);
		ui->buttonLeftRoute->setChecked(false);

		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(0);
		}
		ui->overviewMapFrame->setVisible(0);

		/* only make settings available which are used by OS */
#ifndef Q_OS_WIN
		// ui->gb_gps_device->setVisible(false);
		ui->stackedWidgetSettings->setCurrentIndex(0);
#else
		// ui->gb_gps_gpsd->setVisible(false);
		ui->stackedWidgetSettings->setCurrentIndex(1);
#endif

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(4, top, right, bottom);

		ui->stackedWidget->setCurrentIndex(2);
	}
	else
	{
		/* only load_settings if we changed from settings tab */
		gps_settings_t settings;
		loadSettings(&settings);

		/* close map on very small width < height screens */
		if(desktop.width() < desktop.height())
		{
			ui->routeFrameBottom->setVisible(1);
		}
		ui->overviewMapFrame->setVisible(1);

		int left, right, top, bottom;
		ui->tabLayoutRight->getContentsMargins(&left, &top, &right, &bottom);
		ui->tabLayoutRight->setContentsMargins(0, top, right, bottom);
	}

	ui->frameSettings->setVisible(checked);
}
void MainWindow::callback_fullscreen()
{
	if(this->isFullScreen())
		this->showNormal();
	else
		this->showFullScreen();

	ui->map->force_redraw();
}

void MainWindow::on_buttonRouteStartAddress_clicked()
{
	ui->stackedWidgetRoutingStart->setCurrentIndex(0);
	ui->buttonRouteStartGPS->setChecked(0);
}

void MainWindow::on_buttonRouteStartGPS_clicked()
{
	ui->stackedWidgetRoutingStart->setCurrentIndex(1);
	ui->buttonRouteStartAddress->setChecked(0);
	ui->lineEditRouteStartGPS->setFocus(Qt::OtherFocusReason);
}

void MainWindow::on_buttonRouteDestinationAddress_clicked()
{
	ui->stackedWidgetRoutingDestination->setCurrentIndex(0);
	ui->buttonRouteDestinationGPS->setChecked(0);
}

void MainWindow::on_buttonRouteDestinationGPS_clicked()
{
	ui->stackedWidgetRoutingDestination->setCurrentIndex(1);
	ui->buttonRouteDestinationAddress->setChecked(0);
	ui->lineEditRouteDestinationGPS->setFocus(Qt::OtherFocusReason);
}

/* A suggested CITY was selected as route start */
void MainWindow::callbackRouteStartCompleterCity(const QString &text)
{
	if(ui->map->_routing->suggestionClicked(text, Routing::ID_START_CITY))
	{
		ui->editRoutingStartStreet->setEnabled(1);
	}
	else
	{
		ui->editRoutingStartCity->clear();
	}
}

/* A suggested STREET was selected as route start */
void MainWindow::callbackRouteStartCompleterStreet(const QString &text)
{
	if(ui->map->_routing->suggestionClicked(text, Routing::ID_START_STREET))
	{
		ui->editRoutingStartStreet->setEnabled(1);

		if(ui->map->_routing->checkPos())
			ui->button_calc_route->setEnabled(1);
	}
	else
	{
		ui->editRoutingStartCity->clear();
	}
}

/* A suggested CITY was selected as route destination */
void MainWindow::callbackRouteDestCompleterCity(const QString &text)
{
	if(ui->map->_routing->suggestionClicked(text, Routing::ID_DEST_CITY))
	{
		ui->editRoutingDestStreet->setEnabled(1);
	}
	else
	{
		ui->editRoutingDestCity->clear();
	}
}

/* A suggested STREET was selected as route destination */
void MainWindow::callbackRouteDestCompleterStreet(const QString &text)
{
	if(ui->map->_routing->suggestionClicked(text, Routing::ID_DEST_STREET))
	{
		ui->editRoutingDestStreet->setEnabled(1);

		if(ui->map->_routing->checkPos())
			ui->button_calc_route->setEnabled(1);
	}
	else
	{
		ui->editRoutingDestCity->clear();
	}
}

void MainWindow::on_editRoutingStartCity_textEdited(QString text)
{
	QStringList suggestions;

	if(text.length() > 0)
	{
		QCompleter *mCompleter = ui->editRoutingStartCity->completer();
		QStringListModel *model = (QStringListModel *)mCompleter->model();

		ui->map->_routing->getCitySuggestions(text, suggestions);
		model->setStringList(suggestions);
	}

	/* Reset street if city name gets changed */
	ui->editRoutingStartStreet->setText("");
	ui->editRoutingStartStreet->setEnabled(0);
	ui->button_calc_route->setEnabled(0);

	if(!suggestions.length())
		ui->editRoutingStartCity->setFocus(Qt::OtherFocusReason);
}

void MainWindow::on_editRoutingStartStreet_textEdited(QString text)
{
	QStringList suggestions;

	if(text.length() > 0)
	{
		QCompleter *mCompleter = ui->editRoutingStartStreet->completer();
		QStringListModel *model = (QStringListModel *)mCompleter->model();

		ui->map->_routing->getStreetSuggestions(text, suggestions, Routing::ID_START_STREET);
		model->setStringList(suggestions);
	}

	/* Reset street if city name gets changed */
	ui->map->_routing->resetPos(Routing::ID_START_STREET);
	ui->button_calc_route->setEnabled(0);

	if(!suggestions.length())
		ui->editRoutingStartStreet->setFocus(Qt::OtherFocusReason);
}

void MainWindow::on_editRoutingDestCity_textEdited(QString text)
{
	QStringList suggestions;

	if(text.length() > 0)
	{
		QCompleter *mCompleter = ui->editRoutingDestCity->completer();
		QStringListModel *model = (QStringListModel *)mCompleter->model();

		ui->map->_routing->getCitySuggestions(text, suggestions);
		model->setStringList(suggestions);
	}

	/* Reset street if city name gets changed */
	ui->editRoutingDestStreet->setEnabled(0);
	ui->editRoutingDestStreet->setText("");
	ui->button_calc_route->setEnabled(0);

	if(!suggestions.length())
		ui->editRoutingDestCity->setFocus(Qt::OtherFocusReason);
}

void MainWindow::on_editRoutingDestStreet_textEdited(QString text)
{
	QStringList suggestions;

	if(text.length() > 0)
	{
		QCompleter *mCompleter = ui->editRoutingDestStreet->completer();
		QStringListModel *model = (QStringListModel *)mCompleter->model();

		ui->map->_routing->getStreetSuggestions(text, suggestions, Routing::ID_DEST_STREET);
		model->setStringList(suggestions);
	}

	/* Reset street if city name gets changed */
	ui->map->_routing->resetPos(Routing::ID_DEST_STREET);
	ui->button_calc_route->setEnabled(0);

	if(!suggestions.length())
		ui->editRoutingDestStreet->setFocus(Qt::OtherFocusReason);
}


void MainWindow::on_lineEditRouteStartGPS_textChanged(QString text)
{
	ui->map->_routing->gpsEntered(text, Routing::ID_START_STREET);

	if(ui->map->_routing->checkPos())
	{
		ui->button_calc_route->setEnabled(1);
	}
	else
	{
		ui->button_calc_route->setEnabled(0);
	}
}

void MainWindow::on_route_start_gps_button_clicked()
{
	nmeaGPS gpsdata;
	_gpsd->gpsd_get_data(&gpsdata);

	if(gpsdata.valid)
	{
		QString gps = QString::number(gpsdata.latitude, 'f', 6) + ", " + QString::number(gpsdata.longitude, 'f', 6);
		ui->lineEditRouteStartGPS->setText(gps);
	}
}

void MainWindow::on_route_end_gps_button_clicked()
{
	nmeaGPS gpsdata;
	_gpsd->gpsd_get_data(&gpsdata);

	if(gpsdata.valid)
	{
		QString gps = QString::number(gpsdata.latitude, 'f', 6) + ", " + QString::number(gpsdata.longitude, 'f', 6);
		ui->lineEditRouteDestinationGPS->setText(gps);
	}
}

void MainWindow::on_lineEditRouteDestinationGPS_textChanged(QString text)
{
	ui->map->_routing->gpsEntered(text, Routing::ID_DEST_STREET);

	if(ui->map->_routing->checkPos())
	{
		ui->button_calc_route->setEnabled(1);
	}
	else
	{
		ui->button_calc_route->setEnabled(0);
	}
}
