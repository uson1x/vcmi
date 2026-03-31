/*
 * androidfilepicker.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "androidfilepicker.h"

#ifdef VCMI_ANDROID

#include <QFileDialog>
#include <QMessageBox>

#include <QAndroidJniObject>
#include <QtAndroid>

QString AndroidFilePicker::getOpenFileName(QWidget * parent, const QString & title,
	const QString & internalDir, const QString & filter, Mode mode)
{
	if(mode == Mode::ExternalOnly)
	{
		return nativeOpenFile("*/*");
	}

	// Ask user: internal or external
	QMessageBox box(parent);
	box.setWindowTitle(title);
	box.setText(QObject::tr("Where do you want to open the file from?"));
	auto * btnInternal = box.addButton(QObject::tr("Internal (Maps folder)"), QMessageBox::AcceptRole);
	auto * btnExternal = box.addButton(QObject::tr("External (File picker)"), QMessageBox::ActionRole);
	box.addButton(QMessageBox::Cancel);
	box.exec();

	if(box.clickedButton() == btnInternal)
	{
		return QFileDialog::getOpenFileName(parent, title, internalDir, filter,
			nullptr, QFileDialog::DontUseNativeDialog);
	}
	else if(box.clickedButton() == btnExternal)
	{
		return nativeOpenFile("*/*");
	}

	return {};
}

QString AndroidFilePicker::getSaveFileName(QWidget * parent, const QString & title,
	const QString & internalDir, const QString & filter, Mode mode,
	QString & outContentUri)
{
	outContentUri.clear();

	if(mode == Mode::ExternalOnly)
	{
		QString uri = nativeSaveFile("*/*", "file");
		if(uri.isEmpty())
			return {};
		outContentUri = uri;
		// Return a temp path for the caller to save into; the caller then must
		// call writeFileToUri() to push the temp file to the content:// URI.
		QString tempPath = QDir::tempPath() + "/vcmi_export_tmp";
		return tempPath;
	}

	// Ask user: internal or external
	QMessageBox box(parent);
	box.setWindowTitle(title);
	box.setText(QObject::tr("Where do you want to save the file?"));
	auto * btnInternal = box.addButton(QObject::tr("Internal (Maps folder)"), QMessageBox::AcceptRole);
	auto * btnExternal = box.addButton(QObject::tr("External (File picker)"), QMessageBox::ActionRole);
	box.addButton(QMessageBox::Cancel);
	box.exec();

	if(box.clickedButton() == btnInternal)
	{
		return QFileDialog::getSaveFileName(parent, title, internalDir, filter,
			nullptr, QFileDialog::DontUseNativeDialog);
	}
	else if(box.clickedButton() == btnExternal)
	{
		QString suggestedName = "map.vmap";
		// Extract suggested name from filter
		if(filter.contains("*.vmap"))
			suggestedName = "map.vmap";
		else if(filter.contains("*.vcmp"))
			suggestedName = "campaign.vcmp";
		else if(filter.contains("*.json"))
			suggestedName = "template.json";

		QString mimeType = "application/octet-stream";

		QString uri = nativeSaveFile(mimeType, suggestedName);
		if(uri.isEmpty())
			return {};

		outContentUri = uri;
		QString tempPath = QDir::tempPath() + "/vcmi_export_tmp";
		return tempPath;
	}

	return {};
}

bool AndroidFilePicker::writeFileToUri(const QString & localPath, const QString & contentUri)
{
	QAndroidJniObject activity = QtAndroid::androidActivity();
	if(!activity.isValid())
		return false;

	QAndroidJniObject jLocalPath = QAndroidJniObject::fromString(localPath);
	QAndroidJniObject jUri = QAndroidJniObject::fromString(contentUri);

	jboolean result = activity.callMethod<jboolean>("writeFileToUri",
		"(Ljava/lang/String;Ljava/lang/String;)Z",
		jLocalPath.object<jstring>(), jUri.object<jstring>());

	return result;
}

QString AndroidFilePicker::nativeOpenFile(const QString & mimeType)
{
	QAndroidJniObject activity = QtAndroid::androidActivity();
	if(!activity.isValid())
		return {};

	QAndroidJniObject jMimeType = QAndroidJniObject::fromString(mimeType);
	QAndroidJniObject result = activity.callObjectMethod("pickFileForOpen",
		"(Ljava/lang/String;)Ljava/lang/String;",
		jMimeType.object<jstring>());

	if(!result.isValid())
		return {};

	QString path = result.toString();
	return path.isEmpty() ? QString() : path;
}

QString AndroidFilePicker::nativeSaveFile(const QString & mimeType, const QString & suggestedName)
{
	QAndroidJniObject activity = QtAndroid::androidActivity();
	if(!activity.isValid())
		return {};

	QAndroidJniObject jMimeType = QAndroidJniObject::fromString(mimeType);
	QAndroidJniObject jName = QAndroidJniObject::fromString(suggestedName);
	QAndroidJniObject result = activity.callObjectMethod("pickFileForSave",
		"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
		jMimeType.object<jstring>(), jName.object<jstring>());

	if(!result.isValid())
		return {};

	QString uri = result.toString();
	return uri.isEmpty() ? QString() : uri;
}

#endif // VCMI_ANDROID
