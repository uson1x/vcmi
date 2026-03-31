package eu.vcmi.vcmi;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import androidx.annotation.Nullable;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import org.libsdl.app.SDL;

import java.util.Locale;

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
			Os.setenv("VCMI_MAP_EDITOR", "1", true);

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

		applyImmersiveFullscreen();
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus)
			applyImmersiveFullscreen();
	}

	@Override
	protected void onDestroy()
	{
		super.onDestroy();
		finishAffinity();
		System.exit(0);
	}

	private void applyImmersiveFullscreen()
	{
		Window window = getWindow();
		View decorView = window.getDecorView();

		window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

		decorView.setSystemUiVisibility(
				View.SYSTEM_UI_FLAG_LAYOUT_STABLE
				| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
				| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_FULLSCREEN
				| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
		);

		WindowInsetsControllerCompat insetsController = WindowCompat.getInsetsController(window, decorView);
		if (insetsController != null)
		{
			insetsController.hide(WindowInsetsCompat.Type.systemBars());
			insetsController.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
		}
	}
}
