package com.davidbraga.heroeslore3;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;

public class HeroesLore3Activity extends SDLActivity {
    private static final int MAX_HTTP_BODY = 64 * 1024 * 1024;
    private static final int MAX_REDIRECTS = 8;

    private static native boolean nativeDownloadProgress(long ctxPtr, long done, long total);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        applyImmersiveUi();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveUi();
        }
    }

    @SuppressWarnings("deprecation")
    private void applyImmersiveUi() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            if (lp.layoutInDisplayCutoutMode != WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES) {
                lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                getWindow().setAttributes(lp);
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController insetsController = getWindow().getInsetsController();
            if (insetsController != null) {
                insetsController.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                insetsController.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    private static boolean isRedirectCode(int code) {
        return code == HttpURLConnection.HTTP_MOVED_PERM
                || code == HttpURLConnection.HTTP_MOVED_TEMP
                || code == HttpURLConnection.HTTP_SEE_OTHER
                || code == 307
                || code == 308;
    }

    private static HttpURLConnection openConnectionWithRedirects(
            String method,
            String urlStr,
            String[] hdrKeys,
            String[] hdrVals,
            byte[] bodyBytes,
            int timeoutMs,
            String[] outFinalUrl) throws Exception {
        String currentUrl = urlStr;
        String currentMethod = (method == null || method.isEmpty()) ? "GET" : method;
        byte[] currentBody = bodyBytes;

        for (int hop = 0; hop <= MAX_REDIRECTS; ++hop) {
            URL url = new URL(currentUrl);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setInstanceFollowRedirects(false);
            conn.setConnectTimeout(Math.max(1000, timeoutMs));
            conn.setReadTimeout(Math.max(1000, timeoutMs));
            conn.setRequestMethod(currentMethod);

            if (hdrKeys != null && hdrVals != null) {
                int n = Math.min(hdrKeys.length, hdrVals.length);
                for (int i = 0; i < n; ++i) {
                    if (hdrKeys[i] != null && hdrVals[i] != null) {
                        conn.setRequestProperty(hdrKeys[i], hdrVals[i]);
                    }
                }
            }

            if (currentBody != null && currentBody.length > 0) {
                conn.setDoOutput(true);
                conn.setFixedLengthStreamingMode(currentBody.length);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(currentBody);
                }
            }

            int code = conn.getResponseCode();
            if (isRedirectCode(code)) {
                String loc = conn.getHeaderField("Location");
                conn.disconnect();
                if (loc == null || loc.isEmpty()) {
                    throw new IllegalStateException("Redirect without Location header");
                }
                URL next = new URL(url, loc);
                currentUrl = next.toExternalForm();
                if (code != 307 && code != 308) {
                    currentMethod = "GET";
                    currentBody = null;
                }
                continue;
            }
            outFinalUrl[0] = conn.getURL() != null ? conn.getURL().toExternalForm() : currentUrl;
            return conn;
        }
        throw new IllegalStateException("Too many HTTP redirects");
    }

    public static Object[] httpExecute(
            String method,
            String urlStr,
            String[] hdrKeys,
            String[] hdrVals,
            byte[] bodyBytes,
            int timeoutMs) {
        HttpURLConnection conn = null;
        try {
            String[] finalUrl = new String[]{urlStr};
            conn = openConnectionWithRedirects(method, urlStr, hdrKeys, hdrVals, bodyBytes, timeoutMs, finalUrl);
            int status = conn.getResponseCode();
            InputStream stream = (status >= 400) ? conn.getErrorStream() : conn.getInputStream();
            if (stream == null) {
                stream = conn.getInputStream();
            }

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            if (stream != null) {
                try (InputStream is = stream) {
                    byte[] buf = new byte[16384];
                    int total = 0;
                    int r;
                    while ((r = is.read(buf)) != -1) {
                        total += r;
                        if (total > MAX_HTTP_BODY) {
                            return new Object[]{-1, "Response too large", null};
                        }
                        baos.write(buf, 0, r);
                    }
                }
            }
            return new Object[]{status, finalUrl[0], baos.toByteArray()};
        } catch (Exception e) {
            String msg = e.getMessage();
            return new Object[]{-1, (msg != null && !msg.isEmpty()) ? msg : e.getClass().getSimpleName(), null};
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    public static Object[] downloadFile(
            String urlStr,
            String destPath,
            String userAgent,
            long maxBytes,
            long callbackCtx) {
        HttpURLConnection conn = null;
        File outFile = new File(destPath);
        try {
            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists()) {
                parent.mkdirs();
            }

            String ua = (userAgent != null && !userAgent.isEmpty()) ? userAgent : "HeroesLore3Recomp/1.0";
            String[] finalUrl = new String[]{urlStr};
            conn = openConnectionWithRedirects(
                    "GET", urlStr, new String[]{"User-Agent"}, new String[]{ua}, null, 30000, finalUrl);
            int status = conn.getResponseCode();
            if (status < 200 || status >= 300) {
                return new Object[]{Boolean.FALSE, "HTTP " + status, "", 0L};
            }

            long expectedTotal = conn.getContentLengthLong();
            if (expectedTotal < 0) {
                expectedTotal = 0L;
            }
            if (expectedTotal > maxBytes) {
                return new Object[]{Boolean.FALSE, "File exceeds safety limit", "", 0L};
            }

            MessageDigest md = MessageDigest.getInstance("SHA-256");
            long written = 0;
            try (InputStream is = conn.getInputStream();
                 FileOutputStream fos = new FileOutputStream(outFile, false)) {
                byte[] buf = new byte[32768];
                int r;
                while ((r = is.read(buf)) != -1) {
                    if (written + r > maxBytes) {
                        outFile.delete();
                        return new Object[]{Boolean.FALSE, "Download stream exceeds safety limit", "", 0L};
                    }
                    fos.write(buf, 0, r);
                    md.update(buf, 0, r);
                    written += r;
                    if (callbackCtx != 0L && !nativeDownloadProgress(callbackCtx, written, expectedTotal)) {
                        outFile.delete();
                        return new Object[]{Boolean.FALSE, "Download cancelled", "", 0L};
                    }
                }
                fos.getFD().sync();
            }

            byte[] digest = md.digest();
            StringBuilder hex = new StringBuilder(digest.length * 2);
            for (byte b : digest) {
                hex.append(String.format(Locale.US, "%02x", b & 0xff));
            }
            return new Object[]{Boolean.TRUE, finalUrl[0], hex.toString(), written};
        } catch (Exception e) {
            if (outFile.exists()) {
                outFile.delete();
            }
            String msg = e.getMessage();
            return new Object[]{
                    Boolean.FALSE,
                    (msg != null && !msg.isEmpty()) ? msg : e.getClass().getSimpleName(),
                    "",
                    0L
            };
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    public static void openUrl(String url) {
        Context ctx = SDL.getContext();
        if (ctx == null || url == null) {
            return;
        }
        if (!url.startsWith("https://") && !url.startsWith("http://")) {
            return;
        }
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ctx.startActivity(intent);
    }

    @SuppressWarnings("deprecation")
    public static boolean installApk(String apkPath) {
        Context ctx = SDL.getContext();
        if (ctx == null || apkPath == null || apkPath.isEmpty()) {
            return false;
        }
        try {
            File file = new File(apkPath).getCanonicalFile();
            File updatesRoot = new File(ctx.getFilesDir(), "data/updates").getCanonicalFile();
            String prefix = updatesRoot.getPath() + File.separator;
            if (!file.isFile() || !file.getPath().startsWith(prefix)) {
                return false;
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                    && !ctx.getPackageManager().canRequestPackageInstalls()) {
                Intent perm = new Intent(
                        Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                        Uri.parse("package:" + ctx.getPackageName()));
                perm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                ctx.startActivity(perm);
                return true;
            }

            String relName = file.getPath().substring(prefix.length()).replace('\\', '/');
            Uri contentUri = new Uri.Builder()
                    .scheme("content")
                    .authority(ctx.getPackageName() + ".apkprovider")
                    .appendEncodedPath(Uri.encode(relName, "/"))
                    .build();

            Intent install = new Intent(Intent.ACTION_INSTALL_PACKAGE);
            install.setDataAndType(contentUri, "application/vnd.android.package-archive");
            install.putExtra(Intent.EXTRA_NOT_UNKNOWN_SOURCE, true);
            install.putExtra(Intent.EXTRA_RETURN_RESULT, true);
            install.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(install);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}

