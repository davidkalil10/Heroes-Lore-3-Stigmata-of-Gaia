package com.davidbraga.heroeslore3;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

public final class ApkFileProvider extends ContentProvider {
    private static final String MIME_APK = "application/vnd.android.package-archive";

    @Override
    public boolean onCreate() {
        return true;
    }

    private File resolveApkFile(Uri uri) throws FileNotFoundException {
        Context ctx = getContext();
        if (ctx == null || uri == null) {
            throw new FileNotFoundException("Invalid provider context or URI");
        }
        String rawPath = uri.getPath();
        if (rawPath == null || rawPath.isEmpty()) {
            throw new FileNotFoundException("Empty APK path");
        }
        String rel = rawPath.startsWith("/") ? rawPath.substring(1) : rawPath;
        if (rel.isEmpty() || rel.contains("..")) {
            throw new FileNotFoundException("Invalid APK relative path");
        }

        try {
            File updatesRoot = new File(ctx.getFilesDir(), "data/updates").getCanonicalFile();
            File target = new File(updatesRoot, rel).getCanonicalFile();
            String prefix = updatesRoot.getPath() + File.separator;
            if (!target.getPath().startsWith(prefix) || !target.isFile()) {
                throw new FileNotFoundException("APK not found inside staged updates directory");
            }
            if (!target.getName().toLowerCase().endsWith(".apk")) {
                throw new FileNotFoundException("Only .apk payloads are served");
            }
            return target;
        } catch (IOException e) {
            throw new FileNotFoundException("Failed resolving APK path");
        }
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) {
            throw new SecurityException("Read-only provider");
        }
        return ParcelFileDescriptor.open(resolveApkFile(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return MIME_APK;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        try {
            File f = resolveApkFile(uri);
            String[] cols = (projection != null && projection.length > 0)
                    ? projection
                    : new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
            MatrixCursor mc = new MatrixCursor(cols, 1);
            Object[] row = new Object[cols.length];
            for (int i = 0; i < cols.length; ++i) {
                if (OpenableColumns.DISPLAY_NAME.equals(cols[i])) {
                    row[i] = f.getName();
                } else if (OpenableColumns.SIZE.equals(cols[i])) {
                    row[i] = f.length();
                }
            }
            mc.addRow(row);
            return mc;
        } catch (FileNotFoundException e) {
            return new MatrixCursor(new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}, 0);
        }
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("Read-only");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Read-only");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Read-only");
    }
}

