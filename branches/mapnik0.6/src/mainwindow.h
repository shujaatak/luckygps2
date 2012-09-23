/*
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QtGui>
#include <QtNetwork>
#include <QTimer>

#include "sqlite3.h"

#include "routing.h" // this headers needs to be first because of the MAX macro defined later
#include "customwidgets.h"
#include "gpsd.h"
#include "track.h"
#include "gpsd.h"

#include "datasourcemanager.h"


namespace Ui
{
    class MainWindow;
}

namespace
{
	inline QList<QDir> splitDirList(const QString& str)
	{
		QList<QDir> list;

		foreach(QString str, str.split(':'))
		{
			list.append(QDir(str));
		}
		foreach(QString str, str.split(QLatin1Char(':')))
			list.append(QDir(str));

		return list;
	}

	inline QString getValue(const char *varName, const QString &defValue)
	{
		QByteArray env = qgetenv(varName);
		return env.isEmpty() ? defValue : QString::fromLocal8Bit(env.constData(), env.size());
	}
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
	MainWindow(QWidget *parent = 0, int local = 0);
    ~MainWindow();

    Ui::MainWindow *ui;

	/* Defines if the application is executed loca/without installation */
	int _local;

private:
    /* gps server/client handling */
    gpsd *_gpsd;

    /* save settings/etc. tabWidget status here (closed/open) */
    bool _tabOpen;

    /* disable animations, adjust for small screen, etc */
    bool _mobileMode;

    /* settings and tracks DB */
    sqlite3 *_db;
    sqlite3 *_routeDb;

    /* track & route path */
    QString _routePath, _trackPath;

	/* route + tracks folder update timer */
	QTimer _folderTimer;

	/* DataSourceManager for managing map sources */
	/* and creating map crops for display */
	DataSourceManager *_dsm;

private slots:
	void on_lineEditRouteDestinationGPS_textChanged(QString );
	void on_route_end_gps_button_clicked();
	void on_route_start_gps_button_clicked();
	void on_lineEditRouteStartGPS_textChanged(QString );
	void on_editRoutingDestStreet_textEdited(QString );
	void on_editRoutingDestCity_textEdited(QString );
	void on_editRoutingStartStreet_textEdited(QString );
	void on_editRoutingStartCity_textEdited(QString );
	void on_buttonRouteDestinationGPS_clicked();
	void on_buttonRouteDestinationAddress_clicked();
	void on_buttonRouteStartGPS_clicked();
	void on_buttonRouteStartAddress_clicked();
	void on_buttonLeftSettings_toggled(bool checked);
	void on_buttonLeftRoute_toggled(bool checked);
	void on_buttonLeftTrack_toggled(bool checked);
	void on_trackImportCb_activated(int index);
	void on_button_browseroutefolder_clicked();
	void on_button_browsetrackfolder_clicked();
	void on_button_browsemapfolder_clicked();
	void on_button_generate_tiles_clicked();
	void on_button_calc_route_clicked();
    void on_button_top_minus_clicked();
    void on_button_top_plus_clicked();
    void on_button_top_center_toggled(bool checked);
    void on_routeImportButton_clicked();
    void on_trackConStopButton_clicked();
    void on_trackNewButton_clicked();
	void on_button_settings_save_clicked();
    void on_button_settings_defaults_clicked();
    void on_button_settings_reset_clicked();
    void on_settings_cache_spinbox_valueChanged(int );
    void on_button_left_center_toggled(bool checked);
    void on_button_left_minus_clicked();
    void on_button_left_plus_clicked();

	/* Map callbacks */
    void zoom_buttons_clicked(int old_zoom, int new_zoom);
    void callback_center_toggle();
	void callback_map_clicked(double lon, double lat);

    /* MainWindow callbacks for UI redraw (in tabs_update.cpp) */
    void callback_tab_statistics_update();
	void callback_fullscreen();

	void callbackRouteStartCompleterCity(const QString &);
	void callbackRouteStartCompleterStreet(const QString &);
	void callbackRouteDestCompleterCity(const QString &);
	void callbackRouteDestCompleterStreet(const QString &);

private:
    /* settings/db handling (in settings_update.cpp) */
	bool loadSettings(gps_settings_t *settings, bool reset = 0,  bool startup = 0);
	bool saveSettings();
	QStringList loadTrackFormats();
	QString saveTrack(Track *newtrack);
	void getGpsSettings(gps_settings_t &settings);
	void setGpsSettings(gps_settings_t &settings);
	bool checkDBversion();
	void tabGpsUpdate(const nmeaGPS &gpsdata);
	void clearGpsInfo();

	void handleZoomButtons(int new_zoom);

	/* Tracks tab - "start"/"stop" UI button updates */
	void startstopTrack(bool value);

	/* tracks + routes folder update UI function */
	void tabfolderUpdate();
};

#endif // MAINWINDOW_H
