//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: main.cpp,v 1.15 2022/02/19 18:39:13 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

//
#include "auatomres.h"
#include "aubodypartsres.h"
#include "auclientapp.h"
#include "auclientframe.h"
#include "auclientlib.h"
#include "auclientmailres.h"
#include "auclientres.h"
#include "aumodelres.h"
#include "autexture.h"
#include <iostream>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include "auglobal.h"
#include "auglsl.h"
#include <signal.h>

void SignalHandler(int value)
{
	auApp->HandleSignal(value);
}

int main(int argc, char** argv)
{
	// Resolve a legacy input relative to the caller before changing directory.
	bool legacySpacecraft = false;
	QString legacyShipFile;
	for(int i = 1; i < argc; ++i)
		if(QString::fromLocal8Bit(argv[i]) == "-legacy-spacecraft")
		{
			legacySpacecraft = true;
			if(i + 1 < argc && !QString::fromLocal8Bit(argv[i + 1]).startsWith('-'))
				legacyShipFile = QFileInfo(QString::fromLocal8Bit(argv[i + 1])).absoluteFilePath();
			break;
		}

	int returnValue;
	{
		// Determine the path to my files.
		QFileInfo exe(QString::fromLocal8Bit(argv[0]));
		QDir dir(exe.absoluteDir());

		// Change my current directory.
		QDir::setCurrent(dir.absolutePath());

		QStringList libPaths;
#if defined(Q_OS_MACOS)
		libPaths << dir.absolutePath();
		dir.cdUp();
		dir.cd("PlugIns");
		libPaths << dir.absolutePath();
#elif defined(Q_OS_WIN)
		libPaths << dir.absolutePath();
		dir.cd("platforms");
		libPaths << dir.absolutePath();
#endif
		libPaths << QCoreApplication::libraryPaths();

		/*
		qDebug() << "Library Paths";
		for(QStringList::const_iterator pi = libPaths.cbegin(); pi != libPaths.cend(); pi++)
			qDebug() << *pi;
		*/

		QCoreApplication::setLibraryPaths(libPaths);
		//QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

		// Must set default OpenGL version before app constructor, particularly for Mac.
		QSurfaceFormat glformat;
		glformat.setProfile(QSurfaceFormat::CoreProfile);
		glformat.setVersion(GLMajorVersion, GLMinorVersion);

		{
			QSettings::Format settingsFormat = QSettings::NativeFormat;
			for(int i=1; i<argc; i++)
				if(QString::fromLocal8Bit(argv[i]) == "-ini")
					settingsFormat = QSettings::IniFormat;
			QSettings displaySettings(settingsFormat, QSettings::UserScope, "OpenShores", "OpenShores");
			AuGLSLShader::SetPerPixelLighting(displaySettings.value(AuGlobal::PerPixelLightingKey(), false).toBool());
			AuGLSLShader::SetPerPixelSkinGloss(displaySettings.value(AuGlobal::PerPixelSkinGlossKey(), .4).toFloat());
			AuGLSLShader::SetPerPixelClothGloss(displaySettings.value(AuGlobal::PerPixelClothGlossKey(), .1).toFloat());
			AuGLSLShader::SetPerPixelHairGloss(displaySettings.value(AuGlobal::PerPixelHairGlossKey(), .5).toFloat());
			AuGLSLShader::SetShadows(displaySettings.value(AuGlobal::ShadowsKey(), false).toBool());
			AuGLSLShader::SetShadowsDebug(displaySettings.value(AuGlobal::ShadowsDebugKey(), false).toBool());
			AuGLSLShader::SetShadowMaps(displaySettings.value(AuGlobal::ShadowMapsKey(), true).toBool());
			AuGLSLShader::SetShadowMapSoftness(displaySettings.value(AuGlobal::ShadowMapSoftnessKey(), 1).toInt());
			AuGLSLShader::SetShadowMapSize(displaySettings.value(AuGlobal::ShadowMapSizeKey(), 2048).toInt());
			AuGLSLShader::SetShadowMapRadiusMeters(displaySettings.value(AuGlobal::ShadowMapRadiusKey(), 500.).toDouble());
			AuGLSLShader::SetShadowMapBias(displaySettings.value(AuGlobal::ShadowMapBiasKey(), 1.).toDouble());
			AuRenderCounters::s_On = displaySettings.value(AuGlobal::RenderCountersKey(), false).toBool();
			AuGLSLShader::SetShadowCasters(displaySettings.value(AuGlobal::ShadowCastersKey(), 15).toUInt());
			AuGLSLShader::SetShadowSecondSun(displaySettings.value(AuGlobal::ShadowSecondSunKey(), false).toBool());
			AuGLSLShader::SetShadowLights(displaySettings.value(AuGlobal::ShadowLightsKey(), 0).toInt());
			AuGLSLShader::SetShadowCasterLimitMeters(SSF_Creatures,
					displaySettings.value(AuGlobal::ShadowCreatureDistanceKey(), 100).toInt());
		}
		QSurfaceFormat::setDefaultFormat(glformat);

		// Both launch paths use the game application and its shared services.
		signal(SIGINT, SignalHandler);
		signal(SIGTERM, SignalHandler);
#if !defined(Q_OS_WIN)
		signal(SIGPIPE, SignalHandler);	// Broken pipe gets ignored.
		signal(SIGHUP, SignalHandler);
		signal(SIGQUIT, SignalHandler);
#endif

		// Create an object from each library, so their constructors can call the appropriate Q_INIT_RESOURCE macros.
		// The objects for shared libraries must persist through the life of the program. Otherwise, they get thrashed loading and
		// unloading every time a resource is needed on Windows, which makes this program load very slowly.
		AuAtomRes auAtomRes;
		AuBodyPartsRes auBodyPartsRes;
		AuClientMailRes auClientMailRes;
		AuClientRes auClientRes;
		AuDesigner auDesigner;
		AuGL auGL;
		AuModelRes auModelRes;
		AuTexture auTexture;
		AuUtil auUtil;

		AuClientApp a(argc, argv);
		QStringList args = a.arguments();
		if(args.contains("-version") || args.contains("-v"))
		{
			// Display version information and then exit.
			cout << "AuSceneClient " << AuUtil::VersionFull().toLatin1().data() << '\n';
			exit(0);
		}

		// Create the main frame window.
		AuClientFrame cw;
		a.SetFrame(&cw);
		if(legacySpacecraft)
			cw.OpenLegacyDesign(legacyShipFile);
		cw.showNormal();

		// Start chuggin'.
		returnValue = a.exec();
	}

//#if !defined(QT_NO_DEBUG)
	//AuGlobal::MemoryInfo();
//#endif

	return returnValue;
}
