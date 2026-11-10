//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auclientview.cpp,v 1.6 2022/02/19 18:39:09 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auclientview.h"
#include "auglobal.h"
#include <QSurfaceFormat>
//

AuClientView::AuClientView(QWidget* parent) : AuDesignerView(parent)
{
	// Request edge antialiasing before Qt creates the scene framebuffer.
	QSurfaceFormat requested = format();
	requested.setSamples(g_Au.SceneMsaaSamples());
	setFormat(requested);
}

AuClientView::~AuClientView()
{
}
