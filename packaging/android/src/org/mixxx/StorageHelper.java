package org.mixxx;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.provider.OpenableColumns;
import android.util.Log;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/// The storage volumes Android has mounted (internal shared storage, SD
/// cards, USB drives), for the FOLDER source of the library and the music
/// folder picker. See mixxx::androidStorageVolumes().
public class StorageHelper {
    private static final String TAG = "MixxxStorage";

    public static final String KIND_INTERNAL = "internal";
    public static final String KIND_SD = "sd";
    public static final String KIND_USB = "usb";

    /// One line per mounted volume: path, name and kind (internal, sd or
    /// usb), separated by tabs
    public static String[] volumes(Context context) {
        List<String> lines = new ArrayList<>();
        try {
            StorageManager manager = context.getSystemService(StorageManager.class);
            if (manager == null) {
                return new String[0];
            }
            for (StorageVolume volume : manager.getStorageVolumes()) {
                final String state = volume.getState();
                if (!Environment.MEDIA_MOUNTED.equals(state) &&
                        !Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
                    continue;
                }
                final File directory = directoryOf(volume);
                // Mounted for the system only (e.g. a USB drive only the
                // Files app may open, under /mnt/media_rw): useless to apps
                if (directory == null || !readableByApps(directory)) {
                    continue;
                }
                final String name = volume.getDescription(context);
                lines.add(directory.getAbsolutePath() + "\t" +
                        (name != null ? name.replace('\t', ' ') : directory.getName()) + "\t" +
                        kindOf(volume, name));
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to list the storage volumes", e);
        }
        return lines.toArray(new String[0]);
    }

    /// Whether Android has a removable volume that it mounted for apps or is
    /// about to mount (checking it, or not checked yet), e.g. a USB drive with
    /// a partition table. Mixxx must not read such a drive itself: taking the
    /// USB device from Android makes its check (fsck) fail and the drive is
    /// never mounted. A drive Android can't mount ends as "unmountable", or
    /// has no volume at all.
    public static boolean removableVolumePending(Context context) {
        try {
            StorageManager manager = context.getSystemService(StorageManager.class);
            if (manager == null) {
                return false;
            }
            for (StorageVolume volume : manager.getStorageVolumes()) {
                if (volume.isPrimary() || !volume.isRemovable()) {
                    continue;
                }
                final String state = volume.getState();
                if (Environment.MEDIA_CHECKING.equals(state) ||
                        Environment.MEDIA_UNMOUNTED.equals(state)) {
                    return true;
                }
                // Mounted, and readable by apps (not for the system only)
                if (Environment.MEDIA_MOUNTED.equals(state) ||
                        Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
                    final File directory = directoryOf(volume);
                    if (directory != null && readableByApps(directory)) {
                        return true;
                    }
                }
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to read the state of the storage volumes", e);
        }
        return false;
    }

    /// Whether a volume is shared with apps: under /storage, where Android
    /// puts the volumes apps use. A drive mounted only for the system (e.g.
    /// for its Files app) stays under /mnt/media_rw, which Mixxx's readers
    /// don't look at, even if it may be listed there.
    private static boolean readableByApps(File directory) {
        return directory.getAbsolutePath().startsWith("/storage/") && directory.list() != null;
    }

    private static File directoryOf(StorageVolume volume) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return volume.getDirectory();
        }
        // Android 10
        if (volume.isPrimary()) {
            return Environment.getExternalStorageDirectory();
        }
        final String uuid = volume.getUuid();
        return uuid != null ? new File("/storage/" + uuid) : null;
    }

    /// Android doesn't tell SD cards and USB drives apart, but names them
    /// e.g. "SD card" / "Tarjeta SD" and "USB drive" / "Unidad USB". A
    /// removable volume whose name doesn't say SD is taken as a USB drive.
    private static String kindOf(StorageVolume volume, String name) {
        if (volume.isPrimary() || !volume.isRemovable()) {
            return KIND_INTERNAL;
        }
        final String upper = name != null ? name.toUpperCase(Locale.ROOT) : "";
        if (upper.matches(".*\\b(MICRO)?SD\\b.*") && !upper.contains("USB")) {
            return KIND_SD;
        }
        return KIND_USB;
    }

    // [AI-GENERATED DISCLAIMER START: Autonomously generated by an AI agent]
    /// Resolves the original display filename (and extension via MIME fallback)
    /// for an Android Storage Access Framework content:// URI.
    public static String contentFileName(Context context, String uriString) {
        if (context == null || uriString == null || uriString.isEmpty()) {
            return "";
        }
        try {
            final Uri uri = Uri.parse(uriString);
            String displayName = null;
            try (Cursor cursor = context.getContentResolver().query(
                    uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    final int idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (idx >= 0) {
                        displayName = cursor.getString(idx);
                    }
                }
            }
            if (displayName == null || displayName.isEmpty()) {
                displayName = uri.getLastPathSegment();
            }
            if (displayName == null) {
                displayName = "";
            }
            if (!displayName.contains(".")) {
                final String mime = context.getContentResolver().getType(uri);
                if (mime != null) {
                    String ext = MimeTypeMap.getSingleton().getExtensionFromMimeType(mime);
                    if ((ext == null || ext.isEmpty()) && "audio/x-wav".equalsIgnoreCase(mime)) {
                        ext = "wav";
                    }
                    if (ext != null && !ext.isEmpty()) {
                        displayName = displayName + "." + ext;
                    }
                }
            }
            return displayName;
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to resolve content URI filename: " + uriString, e);
            return "";
        }
    }
    // [AI-GENERATED DISCLAIMER END: Autonomously generated by an AI agent]
}
