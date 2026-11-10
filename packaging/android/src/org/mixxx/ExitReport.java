package org.mixxx;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/// Why Mixxx ended the last times (Android 11 and later): a crash, a kill by
/// Android for lack of memory... Users can send these files when Mixxx closes
/// by itself, since the log of Mixxx doesn't see a native crash or a kill.
///
/// Writes Mixxx/last-exit.txt, and for a native crash or a freeze the report
/// of Android as Mixxx/last-exit-trace.bin, once per new exit.
public final class ExitReport {
    private static final String TAG = "MixxxExitReport";
    private static final String PREFERENCES = "exit_report";
    private static final String LAST_TIMESTAMP = "last_timestamp";
    private static final int MAX_EXITS = 5;

    private ExitReport() {}

    public static void save(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        try {
            ActivityManager manager = context.getSystemService(ActivityManager.class);
            List<ApplicationExitInfo> exits =
                    manager.getHistoricalProcessExitReasons(null, 0, MAX_EXITS);
            if (exits.isEmpty()) {
                return;
            }
            SharedPreferences preferences =
                    context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
            final long newest = exits.get(0).getTimestamp();
            if (newest == preferences.getLong(LAST_TIMESTAMP, 0)) {
                return;
            }
            File dir = directory(context);
            SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT);
            try (PrintWriter out = new PrintWriter(new File(dir, "last-exit.txt"))) {
                out.println("Device: " + Build.MANUFACTURER + " " + Build.MODEL
                        + ", Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT
                        + ")");
                out.println("Hardware: " + Build.HARDWARE
                        + (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                                        ? ", " + Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL
                                        : ""));
                ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
                manager.getMemoryInfo(memory);
                out.println("RAM: " + memory.totalMem / (1024 * 1024) + " MB");
                for (ApplicationExitInfo exit : exits) {
                    out.println();
                    out.println(format.format(new Date(exit.getTimestamp())) + ": "
                            + reasonName(exit.getReason()) + ", status " + exit.getStatus()
                            + ", importance " + exit.getImportance());
                    out.println("  " + exit.getDescription());
                    out.println("  memory: pss " + exit.getPss() / 1024 + " MB, rss "
                            + exit.getRss() / 1024 + " MB");
                }
            }
            // The report of Android on the last native crash or freeze
            ApplicationExitInfo last = exits.get(0);
            if (last.getReason() == ApplicationExitInfo.REASON_CRASH_NATIVE
                    || last.getReason() == ApplicationExitInfo.REASON_ANR) {
                try (InputStream in = last.getTraceInputStream()) {
                    if (in != null) {
                        try (OutputStream out = new FileOutputStream(
                                     new File(dir, "last-exit-trace.bin"))) {
                            byte[] buffer = new byte[16384];
                            int read;
                            while ((read = in.read(buffer)) > 0) {
                                out.write(buffer, 0, read);
                            }
                        }
                    }
                }
            }
            preferences.edit().putLong(LAST_TIMESTAMP, newest).apply();
        } catch (Exception e) {
            Log.w(TAG, "Unable to save the exit report", e);
        }
    }

    /// Mixxx/ of the storage when Mixxx may write there, else its own folder
    private static File directory(Context context) {
        File shared = new File("/storage/emulated/0/Mixxx");
        if ((shared.isDirectory() || shared.mkdirs()) && shared.canWrite()) {
            return shared;
        }
        return context.getExternalFilesDir(null);
    }

    private static String reasonName(int reason) {
        switch (reason) {
        case ApplicationExitInfo.REASON_EXIT_SELF:
            return "closed by itself";
        case ApplicationExitInfo.REASON_SIGNALED:
            return "ended by a signal";
        case ApplicationExitInfo.REASON_LOW_MEMORY:
            return "killed by Android, low memory";
        case ApplicationExitInfo.REASON_CRASH:
            return "Java crash";
        case ApplicationExitInfo.REASON_CRASH_NATIVE:
            return "native crash";
        case ApplicationExitInfo.REASON_ANR:
            return "not responding (ANR)";
        case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE:
            return "initialization failure";
        case ApplicationExitInfo.REASON_PERMISSION_CHANGE:
            return "permission change";
        case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE:
            return "excessive resource usage";
        case ApplicationExitInfo.REASON_USER_REQUESTED:
            return "closed by the user";
        case ApplicationExitInfo.REASON_USER_STOPPED:
            return "stopped by the user";
        case ApplicationExitInfo.REASON_DEPENDENCY_DIED:
            return "dependency died";
        case ApplicationExitInfo.REASON_OTHER:
            return "other";
        default:
            return "unknown (" + reason + ")";
        }
    }
}
