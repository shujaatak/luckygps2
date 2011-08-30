// TODO: LICENSE HEADER

#ifndef ROUTE_DESCRIPTION_H
#define ROUTE_DESCRIPTION_H

#include <QVector>
#include <QStringList>
#include <QtDebug>

#include "interfaces/irouter.h"
#include "route.h"


class DescriptionGenerator
{

public:

	DescriptionGenerator()
	{
	}

	typedef struct {
		QString &name;
		QString &nextName;
		int direction;
		int distance;
		QStringList* icons;
		QStringList* labels;
		int exitNumber;
		QString &lastType;
		int units;
		int lastPoint;
		int exitLink;
		int enterLink;
		QString &type;
	} descInfo;

	struct EdgeInfo {
		unsigned int nameID;
		unsigned int typeID;
		QString name;
		QString type;
		bool branchingPossible;
		int direction;
		int exitNumber;
		int enterLink;
		int exitLink;
	} EdgeInfo;

	void reset(struct EdgeInfo &info)
	{
		info.nameID = UINT_MAX;
		info.typeID = UINT_MAX;
		info.exitNumber = 0;
		info.direction = 0;
		info.branchingPossible = 0;
		info.enterLink = 0;
		info.exitLink = 0;
		info.enterLink = 0;
	}

	void descriptions(Route &route, IRouter* router, QStringList* icons, QVector< IRouter::Node > pathNodes, QVector< IRouter::Edge > pathEdges, int units, int maxSeconds = INT_MAX)
	{
		// TODO: calc label in dependance of driving speed?


		/* Information of current edge */
		struct EdgeInfo info;

		/* List of node indices where descriptions take place */
		QList< int > descList;

		icons->clear();

		/* Sanity checks */
		if(router == NULL || pathEdges.empty() || pathNodes.empty())
		{
			*icons = QStringList();
			return;
		}

		/* Insert a route point with gps positions for every PathNode */
		for(int j=0; j < route.pathNodes.size(); j++)
		{
			GPSCoordinate gps = pathNodes[j].coordinate.ToGPSCoordinate();
			route.insert_point(RoutePoint(gps.latitude, gps.longitude));
		}

		/* Start a new description */
		reset(info);
		newDescription(router, pathEdges.first(), info);
		descList.append(0); // first point is 0, even if there is no description


		int node = 0;
		for (unsigned int edge = 0; edge < pathEdges.size() - 1; edge++)
		{
			node += pathEdges[edge].length;

			info.branchingPossible = pathEdges[edge].branchingPossible;

			/* Support traffic circle: Loop through all edges of a traffic circle */
			if(info.type == "roundabout" && pathEdges[edge + 1].type == info.typeID)
			{
				if(info.branchingPossible)
					info.exitNumber++;
				continue;
			}

			int direction = angle(pathNodes[node - 1].coordinate, pathNodes[node].coordinate, pathNodes[node +  1].coordinate);
			bool breakDescription = false;
			QString edgeType;
			bool typeAvailable = router->GetType(&edgeType, pathEdges[edge + 1].type);
			assert(typeAvailable);

			/* Suport links: Assign the name of street the link is linking to to the link ("motorway_link", "primary_link", etc.) */
			if(edgeType.endsWith("_link"))
			{
				if(!info.type.endsWith("_link") || (info.nameID != pathEdges[edge + 1].name))
				{
					for(unsigned int nextEdge = edge + 2; nextEdge < pathEdges.size(); nextEdge++)
					{
						if((pathEdges[nextEdge].type != pathEdges[edge + 1].type) && (pathEdges[nextEdge].name != 0))
						{
							for(unsigned int otherEdge = edge + 1; otherEdge < nextEdge; otherEdge++)
								pathEdges[otherEdge].name = pathEdges[nextEdge].name;
							break;
						}
					}

					breakDescription = true;

					if(!info.type.endsWith("_link"))
						info.enterLink = 1;
				}
			}
			else if(!edgeType.endsWith("_link") && info.type.endsWith("_link"))
			{
				info.exitLink = 1;
				info.enterLink = 0; // nbot necessary?
			}

			/* Support traffic circle: Generate description on entering/exit of a traffic circle */
			if((edgeType == "roundabout") != (info.type == "roundabout"))
			{
				breakDescription = true;

				/* Check if next edge is also part of this traffic circle */
				if(edgeType != "roundabout")
					direction = 0;

				if(info.type == "roundabout" && edgeType != "roundabout")
				{
					info.exitLink = 1;
				}
				else if(edgeType == "roundabout" && info.type != "roundabout")
				{
					info.enterLink = 1;
				}
			}
			else
			{
				if(info.branchingPossible)
				{
					if(abs(direction) > 1)
						breakDescription = true;
				}

				/* Generate a new description whenever a new street name occurs */
				if(info.nameID != pathEdges[edge + 1].name)
				{
					breakDescription = true;
				}
			}

			/* New description is needed */
			if(breakDescription)
			{
				/* calculate distance of edge/route BACKWARDS! */
				double routeEdgeLen = 0.0;
				for(int i = node - 1; i >= descList.last(); i--)
				{
					RoutePoint &point = route.points[i];
					double edgeLength = fast_distance_deg(point.latitude, point.longitude, route.points[i + 1].latitude, route.points[i + 1].longitude);

					routeEdgeLen += edgeLength;

					point.length = routeEdgeLen;
					point.edgeLength = edgeLength;
				}

				QStringList labels;
				QString nextName;
				bool nameAvailable = router->GetName(&nextName, pathEdges[edge + 1].name);
				RoutePoint &point = route.points[node];
				RoutePoint &oldPoint = route.points[descList.last()];

				info.direction = direction;

				point.enterLink = info.enterLink;
				point.exitLink = info.exitLink;
				point.type = edgeType;

				descInfo desc = {info.name, nextName, info.direction, oldPoint.length*1000.0, icons, &labels, info.exitNumber, info.type, units, 0, point.exitLink, point.enterLink, point.type};
				describe(desc);

				point.desc = labels.last();
				point.direction = info.direction;
				point.exitNumber = info.exitNumber;
				point.lastType = info.type;
				point.name = info.name;
				point.nextName = nextName;

				/*  Add exitNumber to the first node in a traffic circle */
				if(point.exitLink)
				{
					bool changed = false;
					RoutePoint *tmpPoint = NULL;
					int i = 0;
					for(i = node - 1; i >= 0; i--)
					{
						tmpPoint = &(route.points[i]);
						if(tmpPoint->enterLink)
						{
							tmpPoint->exitNumber = point.exitNumber;
							changed = true;
							break;
						}
					}
					if(changed && tmpPoint)
					{
						QStringList labels;

						RoutePoint *tmpPoint2 = NULL;
						i--;
						for(; i >= 0; i--)
						{
							tmpPoint2 = &(route.points[i]);
							if(tmpPoint2->desc.length() > 0)
							{
								break;
							}
						}

						qDebug() << "Found traffic circle to adapt description info.";
						descInfo desc = {tmpPoint->name, tmpPoint->nextName, tmpPoint->direction, tmpPoint2->length*1000.0, icons, &labels, tmpPoint->exitNumber, tmpPoint->lastType, units, 0, tmpPoint->exitLink, tmpPoint->enterLink, tmpPoint->type};
						describe(desc);
						tmpPoint->desc = labels.last();
					}
				}

				/*
				// Commented because we want the description for the whole route
				if(seconds >= maxSeconds)
					break;
				*/

				newDescription(router, pathEdges[edge + 1], info);
				descList.append(node);
			}
		}

		/* description for last edge on route */
		// Commented because we want the description for the whole route
		// if(seconds < maxSeconds)
		{
			QStringList labels;
			QString nextName, nextType;
			bool nameAvailable = router->GetName(&nextName, pathEdges.last().name);
			bool typeAvailable = router->GetType(&nextType, pathEdges.last().type);
			RoutePoint &point = route.points.last();
			RoutePoint &oldPoint = route.points[descList.last()];
			point.length = 0.0;
			double routeEdgeLen = 0.0;

			for(int i = node + pathEdges.last().length - 1; i >= node - 1; i--)
			{
				RoutePoint &point = route.points[i];
				double edgeLength = fast_distance_deg(point.latitude, point.longitude, route.points[i + 1].latitude, route.points[i + 1].longitude);

				routeEdgeLen += edgeLength;

				point.length = routeEdgeLen;
				point.edgeLength = edgeLength;
			}

			point.enterLink = info.enterLink;
			point.exitLink = info.exitLink;

			descInfo desc = {info.name, nextName, info.direction, oldPoint.length*1000.0, icons, &labels, info.exitNumber, info.type, units, 1, point.exitLink, point.enterLink, nextType};
			describe(desc);

			point.desc = labels.last();
			point.direction = info.direction;
			point.exitNumber = info.exitNumber;
			point.lastType = info.type;
			point.name = info.name;
			point.nextName = nextName;
			point.type = nextType;
		}

		/* now fill additional information about e.g. descriptions */
		int j = -1;
		double length = 0.0;
		for(int i = route.points.length() - 1; i >= 0; i--)
		{
			RoutePoint &point = route.points[i];

			// qDebug() << i << ": " << point.length*1000.0;

			point.nextDesc = j;

			/* we like to know the index of the next item with a non-empty description/name/stretname tag */
			if(point.desc.length() > 0)
			{
				j = i;

				length = 0.0;
			}

			/* TODO: add edge distances too */
		}
	}

public:

	int angle(UnsignedCoordinate first, UnsignedCoordinate second, UnsignedCoordinate third)
	{
		double x1 =(double) second.x - first.x; // a =(x1, y1)
		double y1 =(double) second.y - first.y;
		double x2 =(double) third.x - second.x; // b =(x2, y2)
		double y2 =(double) third.y - second.y;
		int angle =(atan2(y1, x1) - atan2(y2, x2)) * 180 / M_PI + 720;
		angle %= 360;
		static const int forward = 10;
		static const int sharp = 45;
		static const int slightly = 20;
		if(angle > 180)
		{
			if(angle > 360 - forward - slightly)
			{
				if(angle > 360 - forward)
					return 0;
				else
					return 1;
			}
			else
			{
				if(angle > 180 + sharp)
					return 2;
				else
					return 3;
			}
		}
		else
		{
			if(angle > forward + slightly)
			{
				if(angle > 180 - sharp)
					return -3;
				else
					return -2;
			}
			else
			{
				if(angle > forward)
					return -1;
				else
					return 0;
			}
		}
	}

	void describe(descInfo &desc)
	{
		bool distClose = true; /* true if(desc.distance <= 3000 && desc.distance >= 50) */
		bool needReturn = false; /* true if special cases don't need end of sentense */
		QString continueStr; /* "Continue on .... for", "Continue for" */
		QString distStr; /* Used for distance strings like "500m", "2km", "300ft" */
		QString directionStr; /* "turn left", "turn right", etc. */
		QString linkStr; /* ramps, exits */
		QString trCircStr; /* Traffic circles: "Take the 1. exit in the traffic circle." */

		/* Don't display any distance when entering a traffic circle */
		if(desc.exitLink && desc.exitNumber != 0)
			desc.distance = 0.0;

		/* Use units (imperial/metrical) if distance is greater than 50 meters */
		if(desc.distance >= 50.0)
			distStr = getDistStr(desc);

		/* Display "Continue" messages only if distance to next "action" is more than 3km */
		/* continueStr needs distStr added to the end */
		if(desc.distance > 3000.0)
		{
			continueStr = getContinueStr(desc);
			desc.labels->push_back(continueStr + distStr);
			return;
		}
		else if(desc.distance <= 3000 && desc.distance >= 50)
		{
			desc.labels->push_back(QString("In %2 ").arg(distStr));
			distClose = false;
		}
		else
		{
			desc.labels->push_back("");
		}

		/* Destination point gets special handling */
		if(!needReturn && desc.lastPoint)
		{
			desc.labels->back() += QString("you have reached the destination.");
			needReturn = true;
		}

		/* Special handling of traffic circles */
		if(!needReturn && desc.exitNumber != 0)
		{
			trCircStr = getTrCircStr(desc);
			desc.labels->back() += trCircStr;
			needReturn = true;
		}

		/* Special handling of links */
		if(!needReturn && (desc.lastType.endsWith("_link") ||
				(desc.type.endsWith("_link"))))
		{
			linkStr = getLinkStr(desc);
			desc.labels->back() += linkStr;
			needReturn = true;
		}

		/* "turn left", "turn right", etc. */
		if(!needReturn)
		{
			directionStr = getdirectionStr(desc);
			desc.labels->back() += directionStr;
		}

		/* First character need to be in uppercase */
		if(distClose)
		{
			if(desc.labels->back().length() > 0) // still need to convert first char to uppercase
				desc.labels->back()[0] = desc.labels->back()[0].toUpper();
		}

		/* Special cases want to early exit */
		if(needReturn)
			return;

		/* if a direction string is given --> end the sentense */
		if(desc.direction != 0)
		{
			if(!desc.nextName.isEmpty())
				desc.labels->back() += " into " + desc.nextName + ".";
			else
				desc.labels->back() += ".";
		}
	}

	void newDescription(IRouter* router, const IRouter::Edge& edge, struct EdgeInfo &info)
	{
		if(info.nameID != edge.name)
		{
			info.nameID = edge.name;
			bool nameAvailable = router->GetName(&(info.name), info.nameID);
			assert(nameAvailable);
		}
		if(info.typeID != edge.type)
		{
			info.typeID = edge.type;
			bool typeAvailable = router->GetType(&(info.type), info.typeID);
			assert(typeAvailable);
		}
		info.branchingPossible = edge.branchingPossible;
		info.direction = 0;
		info.exitNumber = (info.type == "roundabout") ? 1 : 0;
	}

	QString getTrCircStr(descInfo &desc)
	{
		QString trCircStr = "";

		desc.icons->push_back(QString(":/images/directions/roundabout_exit%1.png").arg(desc.exitNumber));
		trCircStr = QString("take the %1. exit in the traffic circle.").arg(desc.exitNumber);
		return trCircStr;
	}

	QString getLinkStr(descInfo &desc)
	{
		QString linkStr = "";

		if(desc.enterLink)
		{
			if(!desc.nextName.isEmpty())
				linkStr = "take the ramp towards " + desc.nextName + ".";
			else
				linkStr = "take the ramp.";
		}
		else if(desc.exitLink)
		{
			if(!desc.nextName.isEmpty())
				linkStr = "take the exit to " + desc.nextName + ".";
			else
				linkStr = "take the exit.";
		}

		if(desc.direction == 0)
		{
			desc.icons->push_back(":/images/directions/forward.png");
								// labels->push_back("");
		}

		return linkStr;

	}

	QString getdirectionStr(descInfo &desc)
	{
		QString directionStr = "";

		switch(desc.direction)
		{
			case 0:
				break;
			case 1:
				{
					desc.icons->push_back(":/images/directions/slightly_right.png");
					 directionStr = "keep slightly right";
					break;
				}
			case 2:
			case 3:
				{
					desc.icons->push_back(":/images/directions/right.png");
					directionStr = "turn right";
					break;
				}
			case -1:
				{
					desc.icons->push_back(":/images/directions/slightly_left.png");
					directionStr = "keep slightly left";
					break;
				}
			case -2:
			case -3:
				{
					desc.icons->push_back(":/images/directions/left.png");
					directionStr = "turn left";
					break;
				}
		}

		return directionStr;
	}

	QString getContinueStr(descInfo &desc)
	{
		QString continueStr = "";

		desc.icons->push_back(":/images/directions/forward.png");
		if(!desc.name.isEmpty())
			continueStr = "Continue on " + desc.name + " for ";
		else
			continueStr = "Continue for ";

		return continueStr;
	}

	/* Create distance string in metrical and imperial units */
	QString getDistStr(descInfo &desc)
	{
		QString distStr = "";

		if(desc.units == 0) /* metrical */
		{
			if(desc.distance < 100)
				distStr = QString("%1m").arg((int) desc.distance);
			else if(desc.distance < 1000)
				distStr = QString("%1m").arg((int) desc.distance / 10 * 10);
			else if(desc.distance < 10000)
				distStr = QString("%1.%2km").arg((int) desc.distance / 1000).arg(((int) desc.distance / 100) % 10);
			else
				distStr = QString("%1km").arg((int) desc.distance / 1000);
		}
		else if(desc.units == 1) /* imperial */
		{
			double unit_conversion = 5280.0/1.6093444;

			double tmpDistance = unit_conversion * desc.distance;

			if(desc.distance < 100)
				distStr = QString("%1ft").arg((int) tmpDistance);
			else if(desc.distance < 1000)
				distStr = QString("%1ft").arg((int) tmpDistance / 10 * 10);
			else if(desc.distance < 10000)
				distStr = QString::number(tmpDistance / 5280.0, 'f', 1) + "m";
			else
				distStr = QString("%1m").arg((int)(tmpDistance / 5280.0));
		}

		return distStr;
	}
};

#endif// ROUTE_DESCRIPTION_H
