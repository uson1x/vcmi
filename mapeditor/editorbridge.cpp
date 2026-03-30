/*
 * editorbridge.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "mainwindow.h"

void openMapEditor()
{
	auto * editorWindow = new MainWindow();
	editorWindow->setAttribute(Qt::WA_DeleteOnClose);
	editorWindow->show();
}
