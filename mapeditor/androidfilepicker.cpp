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
#include <QDir>
#include <QEventLoop>
#include <QMessageBox>
#include <QRegExp>
#include <QWidget>

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QAndroidActivityResultReceiver>
#include <QtAndroid>

namespace
{
static const int REQ_OPEN_FILE = 200;
static const int REQ_SAVE_FILE = 201;

/// One-shot activity-result receiver that wakes up a QEventLoop.
/// Must remain alive until handleActivityResult() fires.
struct PickerReceiver : QAndroidActivityResultReceiver
{
	QString result;
	QEventLoop loop;

	void handleActivityResult(int /*receiverRequestCode*/, int resultCode,
		const QAndroidJniObject & data) override
	{
		static const jint RESULT_OK = -1; // Activity.RESULT_OK
		if(resultCode == RESULT_OK && data.isValid())
		{
			QAndroidJniObject uri = data.callObjectMethod(
				"getData", "()Landroid/net/Uri;");
			if(uri.isValid())
				result = uri.callObjectMethod("toString", "()Ljava/lang/String;").toString();
		}
		loop.quit();
	}
};
} // anonymous namespace

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
	auto * btnInternal = box.addButton(QObject::tr("Internal"), QMessageBox::AcceptRole);
	auto * btnExternal = box.addButton(QObject::tr("External"), QMessageBox::ActionRole);
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
		// Derive MIME type, suggested filename and temp-file extension from the
		// filter string (e.g. "PNG (*.png)" → ext="png", mime="image/png").
		QString ext;
		const int starDot = filter.indexOf("*.");
		if(starDot >= 0)
		{
			int end = filter.indexOf(QRegExp("[^a-zA-Z0-9]"), starDot + 2);
			ext = filter.mid(starDot + 2, end < 0 ? -1 : end - (starDot + 2)).toLower();
		}

		QString mimeType     = "application/octet-stream";
		QString suggestedName = ext.isEmpty() ? "file" : "export." + ext;
		if     (ext == "png")            mimeType = "image/png";
		else if(ext == "jpg" || ext == "jpeg") mimeType = "image/jpeg";
		else if(ext == "bmp")            mimeType = "image/bmp";
		else if(ext == "vmap")           suggestedName = "map.vmap";
		else if(ext == "vcmp")           suggestedName = "campaign.vcmp";
		else if(ext == "json")           { mimeType = "application/json"; suggestedName = "template.json"; }

		QString uri = nativeSaveFile(mimeType, suggestedName);
		if(uri.isEmpty())
			return {};
		outContentUri = uri;

		// Build a temp path with the correct extension so that callers like
		// QImage::save() can detect the format from the file name.
		QString tempDir = QDir::tempPath();
		QDir().mkpath(tempDir);
		return tempDir + "/vcmi_tmp." + (ext.isEmpty() ? "bin" : ext);
	}

	// Ask user: internal or external
	QMessageBox box(parent);
	box.setWindowTitle(title);
	box.setText(QObject::tr("Where do you want to save the file?"));
	auto * btnInternal = box.addButton(QObject::tr("Internal"), QMessageBox::AcceptRole);
	auto * btnExternal = box.addButton(QObject::tr("External"), QMessageBox::ActionRole);
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
		QDir().mkpath(QDir::tempPath());
		QString tempPath = QDir::tempPath() + "/vcmi_export_tmp";
		return tempPath;
	}

	return {};
}

void AndroidFilePicker::writeFileToUri(const QString & localPath, const QString & contentUri)
{
	QAndroidJniObject activity = QtAndroid::androidActivity();
	if(!activity.isValid())
		return;

	QAndroidJniObject jLocalPath = QAndroidJniObject::fromString(localPath);
	QAndroidJniObject jUri = QAndroidJniObject::fromString(contentUri);

	activity.callMethod<void>("writeFileToUri",
		"(Ljava/lang/String;Ljava/lang/String;)V",
		jLocalPath.object<jstring>(), jUri.object<jstring>());

	QAndroidJniEnvironment env;
	if(env->ExceptionCheck())
		env->ExceptionClear();
}

QString AndroidFilePicker::nativeOpenFile(const QString & mimeType)
{
	// Build ACTION_OPEN_DOCUMENT intent on the C++ side
	QAndroidJniObject intent("android/content/Intent",
		"(Ljava/lang/String;)V",
		QAndroidJniObject::fromString("android.intent.action.OPEN_DOCUMENT").object<jstring>());

	QAndroidJniObject categoryOpenable =
		QAndroidJniObject::fromString("android.intent.category.OPENABLE");
	intent.callMethod<void>("addCategory", "(Ljava/lang/String;)V",
		categoryOpenable.object<jstring>());

	QAndroidJniObject jMime = QAndroidJniObject::fromString(mimeType);
	intent.callMethod<void>("setType", "(Ljava/lang/String;)V",
		jMime.object<jstring>());

	PickerReceiver receiver;
	QtAndroid::startActivity(intent, REQ_OPEN_FILE, &receiver);
	receiver.loop.exec(); // wait without blocking the Java UI thread

	if(receiver.result.isEmpty())
		return {};

	// Ask Java to copy the content:// URI into a temp file and return the local path
	QAndroidJniObject activity = QtAndroid::androidActivity();
	QAndroidJniObject jUri = QAndroidJniObject::fromString(receiver.result);
	QAndroidJniObject localPath = activity.callObjectMethod(
		"copyUriToTempFile",
		"(Ljava/lang/String;)Ljava/lang/String;",
		jUri.object<jstring>());

	return (localPath.isValid() && !localPath.toString().isEmpty())
		? localPath.toString() : QString{};
}

QString AndroidFilePicker::nativeSaveFile(const QString & mimeType, const QString & suggestedName)
{
	// Build ACTION_CREATE_DOCUMENT intent on the C++ side
	QAndroidJniObject intent("android/content/Intent",
		"(Ljava/lang/String;)V",
		QAndroidJniObject::fromString("android.intent.action.CREATE_DOCUMENT").object<jstring>());

	QAndroidJniObject categoryOpenable =
		QAndroidJniObject::fromString("android.intent.category.OPENABLE");
	intent.callMethod<void>("addCategory", "(Ljava/lang/String;)V",
		categoryOpenable.object<jstring>());

	QAndroidJniObject jMime = QAndroidJniObject::fromString(mimeType);
	intent.callMethod<void>("setType", "(Ljava/lang/String;)V",
		jMime.object<jstring>());

	QAndroidJniObject jExtraTitle =
		QAndroidJniObject::fromString("android.intent.extra.TITLE");
	QAndroidJniObject jName = QAndroidJniObject::fromString(suggestedName);
	intent.callMethod<jobject>(
		"putExtra",
		"(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
		jExtraTitle.object<jstring>(), jName.object<jstring>());

	PickerReceiver receiver;
	QtAndroid::startActivity(intent, REQ_SAVE_FILE, &receiver);
	receiver.loop.exec();

	return receiver.result;
}

#endif // VCMI_ANDROID
