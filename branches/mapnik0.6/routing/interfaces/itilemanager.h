#ifndef ITILEMANAGER_H
#define ITILEMANAGER_H

#include <QImage>
#include <QtPlugin>
#include <QString>

class ITilemanager
{
public:

	virtual QImage *RequestTile(int x, int y, int zoom) = 0;

	virtual QString GetName() = 0;
	virtual void SetInputDirectory( const QString& dir ) = 0;
	virtual bool IsCompatible( int fileFormatVersion ) = 0;
	virtual bool LoadData() = 0;
	// get the maximal zoom level; possible zoom levels are: [0,GetMaxZoom()]
	virtual int GetMaxZoom() = 0;
	// modify the request to respond to a mouse movement
	virtual ~ITilemanager() {}
};

Q_DECLARE_INTERFACE( ITilemanager, "monav.ITilemanager/1.0" )

#endif // ITILEMANAGER_H
