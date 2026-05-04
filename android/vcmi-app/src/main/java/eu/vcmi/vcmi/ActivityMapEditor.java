package eu.vcmi.vcmi;

import android.net.Uri;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.DisplayMetrics;
import androidx.annotation.Nullable;

import eu.vcmi.vcmi.util.ActivityHelper;
import org.libsdl.app.SDL;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.util.Locale;

import eu.vcmi.vcmi.util.FileUtil;

public class ActivityMapEditor extends org.qtproject.qt5.android.bindings.QtActivity
{
	// Prevents prepareAndroid() from re-copying APK assets (launcher already did it)
	public boolean justLaunched = false;

	private static final int MIN_UI_HEIGHT_PX = 600;

	@Override
	public void onCreate(@Nullable final Bundle savedInstanceState)
	{
		// All env vars must be set before super.onCreate() which starts
		// the QtThread that calls native main()
		try
		{
			Os.setenv("VCMI_LAUNCH_MAP_EDITOR", "1", true);

			// Qt logical height = physical_px / (density * QT_SCALE_FACTOR)
			// We need logical height >= MIN_UI_HEIGHT_PX, so:
			// QT_SCALE_FACTOR <= physical_px / (density * MIN_UI_HEIGHT_PX)
			DisplayMetrics dm = getResources().getDisplayMetrics();
			int screenHeightPx = Math.min(dm.widthPixels, dm.heightPixels);
			double scale = Math.min(1.0, (double) screenHeightPx / (dm.density * MIN_UI_HEIGHT_PX));
			Os.setenv("QT_SCALE_FACTOR", String.format(Locale.US, "%.4f", scale), true);
		}
		catch (ErrnoException e)
		{
			throw new RuntimeException("Failed to set environment", e);
		}
		// NativeMethods.dataRoot() etc. use SDL.getContext() to resolve file paths
		SDL.setContext(this);
		super.onCreate(savedInstanceState);

		ActivityHelper.applyImmersiveFullscreen(this);
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus)
			ActivityHelper.applyImmersiveFullscreen(this);
	}

	@Override
	protected void onDestroy()
	{
		super.onDestroy();
		finishAffinity();
		System.exit(0);
	}

	/**
	 * Called from C++ (Qt thread) via JNI after saving to a local file.
	 * Copies the local file content into the content:// URI obtained from the SAF picker.
	 *
	 * @param localPath  absolute path to the locally saved file
	 * @param contentUri the content:// URI string to write into
	 */
	public void writeFileToUri(String localPath, String contentUri)
	{
		try
		{
			File localFile = new File(localPath);
			if (!localFile.exists() || localFile.length() == 0)
			{
				System.err.println("writeFileToUri: local file missing or empty: " + localPath);
				return;
			}
			Uri uri = Uri.parse(contentUri);
			OutputStream out = getContentResolver().openOutputStream(uri);
			if (out == null)
			{
				System.err.println("writeFileToUri: openOutputStream returned null for: " + contentUri);
				return;
			}
			try (InputStream in = new FileInputStream(localFile); OutputStream o = out)
			{
				FileUtil.copyStream(in, o);
			}
		}
		catch (Exception e)
		{
			e.printStackTrace();
		}
	}

	/**
	 * Called from C++ (Qt thread) via JNI.
	 * Copies a content:// URI into the app's cache directory and returns the local path.
	 * Used after the file picker delivers a URI so that Qt can work with a normal file path.
	 *
	 * @param uriString the content:// URI string to copy
	 * @return absolute path to the temp copy, or empty string on error
	 */
	public String copyUriToTempFile(String uriString)
	{
		try
		{
			Uri uri = Uri.parse(uriString);
			String displayName = FileUtil.getFilenameFromUri(uriString, this);
			if (displayName.isEmpty())
				displayName = "picked_file";
			File tempFile = new File(getCacheDir(), displayName);
			try (InputStream in = getContentResolver().openInputStream(uri);
				 OutputStream out = new FileOutputStream(tempFile))
			{
				FileUtil.copyStream(in, out);
			}
			return tempFile.getAbsolutePath();
		}
		catch (IOException e)
		{
			e.printStackTrace();
			return "";
		}
	}

}
