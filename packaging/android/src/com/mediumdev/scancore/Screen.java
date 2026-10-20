package com.mediumdev.scancore;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.OrientationEventListener;

// Turning the phone for the anime player's full screen (src/app/Platform.cpp,
// WatchPage.qml).
public final class Screen {
    private Screen() {}

    private static OrientationEventListener release;

    // "landscape": landscape whichever way the phone is held, even with
    // auto-rotate off. "portrait": portrait until the phone is held upright,
    // then free again, so turning it later still goes full screen. Anything
    // else: free (the system's rotation setting decides).
    public static void orient(final Activity activity, final String mode) {
        activity.runOnUiThread(() -> {
            if (release != null) {
                release.disable();
                release = null;
            }
            if ("landscape".equals(mode)) {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                return;
            }
            if (!"portrait".equals(mode)) {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                return;
            }
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            OrientationEventListener listener = new OrientationEventListener(activity) {
                @Override
                public void onOrientationChanged(int degrees) {
                    if (degrees == ORIENTATION_UNKNOWN || (degrees > 20 && degrees < 340)) return;
                    disable();
                    if (release == this) release = null;
                    activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                }
            };
            if (listener.canDetectOrientation()) {
                release = listener;
                listener.enable();
            } else {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
            }
        });
    }
}
