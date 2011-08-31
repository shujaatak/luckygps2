/*
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ROUTING_H
#define ROUTING_H

#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QtDebug>
#include <QTime>

#include "interfaces/iaddresslookup.h"
#include "interfaces/igpslookup.h"
#include "interfaces/irouter.h"

#include "route.h"
#include "route_description.h"

class Routing
{
public:
    Routing();
	~Routing();

	enum
	{
		ID_START_STREET = 0,
		ID_DEST_STREET = 1,
		ID_START_CITY = 2,
		ID_DEST_CITY = 3,
	};

	bool getCitySuggestions(QString city, QStringList &suggestions);
	bool getStreetSuggestions(QString street, QStringList &suggestions, int typeID);

	bool suggestionClicked(QString text, int typeID);
	bool gpsEntered(QString text, int idType);

	bool calculateRoute(Route &route, int units, QString hnStart, QString hnDest);
	void getInstructions(RoutePoint *rp, RoutePoint *nextRp, double distance, QStringList* labels, QStringList* icons, int units);

	void setPos(double lat, double lon, int idType) { _pos[idType][0] = lat; _pos[idType][1] = lon; }
	void resetPos(int idType) { _pos[idType][0] = _pos[idType][1] = 0.0; }
	bool checkPos() { return ((_pos[0][0] != 0.0) && (_pos[1][0] != 0.0) && (_pos[0][1] != 0.0) && (_pos[1][1] != 0.0));}

	bool getInit() { return _init; }

	bool init(QString dataDirectory);

	size_t getPlaceID(int typeID) { if(typeID == ID_START_CITY) return _startPlaceID; else return _destPlaceID; }

private:
	bool computeRoute(double* resultDistance, QVector< IRouter::Node >* resultNodes, QVector< IRouter::Edge >* resultEdge, GPSCoordinate source, GPSCoordinate target, double lookupRadius);
	void getInstructions(Route &route, QStringList* icons, int units);
	int dataFolder(QString dir, QString *result);
	int loadPlugins(QString path);

	IGPSLookup				*_gpsLookup;		/* Lookup nearest edge / point	*/
	IRouter					*_router;			/* Calculate route				*/
	IAddressLookup			*_addressLookup;	/* Route address				*/
	DescriptionGenerator	*_descGenerator;	/* Description Generator		*/

	double _pos[2][2];

	size_t _startPlaceID;
	size_t _destPlaceID;

	QString _startStreet;
	QString _destStreet;

	bool _init;
};

#endif // ROUTING_H
