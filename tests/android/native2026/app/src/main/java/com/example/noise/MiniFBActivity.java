package com.example.noise;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

/**
 * Minimal NativeActivity subclass that sets up a proper fullscreen + display-cutout
 * (notch) mode for all Android API levels supported by MiniFB (minSdk = 24).
 *
 * Why this is needed
 * ------------------
 * Android API 32+ changed the default handling of the display cutout (notch): by default
 * the system reserves space for it, so the native window reports its full pixel dimensions
 * but rendered content is shifted/clipped.  API 35+ forces edge-to-edge automatically
 * (works again), but API 32-34 needs an explicit opt-in from the app.
 *
 * This class addresses the problem programmatically, complementing the manifest theme
 * (Option A) which provides an early fallback before Java code runs.
 *
 * onWindowFocusChanged is also overridden because Android can temporarily restore system
 * bars (e.g. when the user swipes from an edge) and we want to hide them again.
 */
public class MiniFBActivity extends NativeActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setupFullscreen();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            setupFullscreen();
        }
    }

    private void setupFullscreen() {
        // -----------------------------------------------------------------------
        // Step 1: Opt-in to draw in the display cutout (notch) area.
        //
        // LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS (API 31) is the strongest opt-in:
        //   the window is allowed to extend into the cutout in all orientations.
        // For API 28-30 we use SHORT_EDGES (value 1), which covers portrait/landscape
        //   short-edge cutouts.  No cutout API existed before API 28.
        // -----------------------------------------------------------------------
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {           // API 31+
            WindowManager.LayoutParams params = getWindow().getAttributes();
            params.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            getWindow().setAttributes(params);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {   // API 28-30
            WindowManager.LayoutParams params = getWindow().getAttributes();
            params.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(params);
        }

        // -----------------------------------------------------------------------
        // Step 2: Hide system bars (status bar + navigation bar).
        //
        // API 30+ (Android 11): use the modern WindowInsetsController.
        // API 24-29: use the deprecated-but-functional setSystemUiVisibility flags.
        // -----------------------------------------------------------------------
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {           // API 30+
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                // BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE: bars reappear temporarily
                // on an inward swipe, then auto-hide again.
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            // noinspection deprecation
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN            |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION       |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY      |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN     |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            );
        }
    }
}
