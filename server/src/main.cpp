/*
 * main.cpp - main file for Veyon Server
 *
 * Copyright (c) 2006-2018 Tobias Junghans <tobydox@users.sf.net>
 *
 * This file is part of Veyon - http://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "VeyonCore.h"

#include <QCoreApplication>
#include <QAbstractNativeEventFilter>

#include "ComputerControlServer.h"
#include "VeyonConfiguration.h"

#ifdef VEYON_BUILD_WIN32
static HANDLE hShutdownEvent = NULL;

// event filter which makes ICA recognize logoff events etc.
class LogoffEventFilter : public QAbstractNativeEventFilter
{
public:
	virtual bool nativeEventFilter( const QByteArray& eventType, void *message, long *result)
	{
		Q_UNUSED(eventType);
		Q_UNUSED(result);

		DWORD winMsg = ( ( MSG *) message )->message;

		if( winMsg == WM_QUERYENDSESSION )
		{
			qInfo( "Got WM_QUERYENDSESSION - initiating server shutdown" );

			// tell UltraVNC server to quit
			SetEvent( hShutdownEvent );
		}

		return false;
	}

};

#endif


int main( int argc, char **argv )
{
	QCoreApplication app( argc, argv );

	VeyonCore core( &app, QStringLiteral("Server") );

	// parse arguments
	QStringListIterator argIt( app.arguments() );
	argIt.next();

	while( argc > 1 && argIt.hasNext() )
	{
		const QString a = argIt.next().toLower();

		if( a == QStringLiteral("-session") && argIt.hasNext() )
		{
			int sessionId = argIt.next().toUInt();
			if( sessionId > 0 )
			{
				core.config().setPrimaryServicePort( core.config().primaryServicePort() + sessionId );
				core.config().setVncServerPort( core.config().vncServerPort() + sessionId );
				core.config().setFeatureWorkerManagerPort( core.config().featureWorkerManagerPort() + sessionId );
			}
		}
	}

#ifdef VEYON_BUILD_WIN32
	hShutdownEvent = OpenEvent( EVENT_ALL_ACCESS, false, L"Global\\SessionEventUltra" );
	if( !hShutdownEvent )
	{
		// no global event available already as we're not running under the
		// control of the veyon service supervisor?
		if( GetLastError() == ERROR_FILE_NOT_FOUND )
		{
			qWarning( "Creating session event" );
			// then create our own event as otherwise the VNC server main loop
			// will eat 100% CPU due to failing WaitForSingleObject() calls
			hShutdownEvent = CreateEvent( NULL, false, false, L"Global\\SessionEventUltra" );
		}
		else
		{
			qWarning( "Could not open or create session event" );
		}
	}

	LogoffEventFilter eventFilter;

	app.installNativeEventFilter( &eventFilter );
#endif

	auto server = new ComputerControlServer;
	server->start();

	qInfo( "Exec" );

	int ret = app.exec();

	delete server;

	qInfo( "Exec Done" );

#ifdef VEYON_BUILD_WIN32
	CloseHandle( hShutdownEvent );
#endif

	return ret;
}
