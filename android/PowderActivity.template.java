package @APPID@;

import org.libsdl.app.SDLActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.os.Bundle;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.util.Enumeration;
import java.io.IOException;
import java.util.Base64;

public class PowderActivity extends SDLActivity
{
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		VirtualControls.attach(this);
	}

	public String getCertificateBundle()
	{
		String allPems = "";
		try {
			KeyStore ks = KeyStore.getInstance("AndroidCAStore");
			if (ks != null) {
				ks.load(null, null);
				Enumeration<String> aliases = ks.aliases();
				while (aliases.hasMoreElements()) {
					String alias = (String)aliases.nextElement();
					java.security.cert.X509Certificate cert = (java.security.cert.X509Certificate)ks.getCertificate(alias);
					allPems += "-----BEGIN CERTIFICATE-----\n" + Base64.getMimeEncoder().encodeToString(cert.getEncoded()) + "\n-----END CERTIFICATE-----\n";;
				}
			}
		} catch (IOException e) {
			e.printStackTrace();
			return "";
		} catch (KeyStoreException e) {
			e.printStackTrace();
			return "";
		} catch (NoSuchAlgorithmException e) {
			e.printStackTrace();
			return "";
		} catch (java.security.cert.CertificateException e) {
			e.printStackTrace();
			return "";
		}
		return allPems;
	}

	private void copyMissing(java.io.File source, java.io.File destination) throws IOException
	{
		if (!source.exists())
			return;
		if (source.isDirectory()) {
			if (!destination.exists() && !destination.mkdirs())
				throw new IOException("Cannot create " + destination);
			java.io.File[] children = source.listFiles();
			if (children != null) {
				for (java.io.File child : children)
					copyMissing(child, new java.io.File(destination, child.getName()));
			}
		} else if (!destination.exists()) {
			java.io.File parent = destination.getParentFile();
			if (parent != null && !parent.exists() && !parent.mkdirs())
				throw new IOException("Cannot create " + parent);
			try (java.io.FileInputStream input = new java.io.FileInputStream(source);
					java.io.FileOutputStream output = new java.io.FileOutputStream(destination)) {
				byte[] buffer = new byte[32768];
				for (int count; (count = input.read(buffer)) != -1; )
					output.write(buffer, 0, count);
			}
		}
	}

	public String getDefaultDdir()
	{
		java.io.File legacyDir = getExternalFilesDir(null);
		java.io.File mediaDir = null;
		try {
			java.io.File[] mediaDirs = getExternalMediaDirs();
			if (mediaDirs != null) {
				for (java.io.File candidate : mediaDirs) {
					if (candidate != null) {
						mediaDir = candidate;
						break;
					}
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
		}

		if (mediaDir == null || (!mediaDir.exists() && !mediaDir.mkdirs()))
			return legacyDir == null ? "" : legacyDir.getAbsolutePath();

		if (legacyDir != null && !legacyDir.equals(mediaDir)) {
			try {
				copyMissing(legacyDir, mediaDir);
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
		return mediaDir.getAbsolutePath();
	}

	public String openUri(final String uri)
	{
		try {
			Intent intent;
			if (uri.startsWith("/")) {
				java.io.File dir = new java.io.File(uri);
				if (!dir.exists() && !dir.mkdirs())
					return "Cannot create data directory: " + uri;

				// file:// and the non-standard resource/folder MIME type are rejected
				// by Android 7+ when the app targets a recent SDK. Convert the path
				// to an ExternalStorageProvider document URI instead.
				String root = Environment.getExternalStorageDirectory().getAbsolutePath();
				String relative = uri.startsWith(root + "/")
						? uri.substring(root.length() + 1) : "";
				Uri documentUri = DocumentsContract.buildDocumentUri(
						"com.android.externalstorage.documents", "primary:" + relative);

				try {
					intent = new Intent(Intent.ACTION_VIEW);
					intent.setDataAndType(documentUri, "vnd.android.document/directory");
					intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
							Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
							Intent.FLAG_ACTIVITY_NEW_TASK);
					startActivity(intent);
					return "";
				} catch (Exception directOpenFailed) {
					// Some vendor file managers do not implement directory ACTION_VIEW.
					// Fall back to the standard system folder picker at the same location.
					intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
					intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
							Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
							Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
							Intent.FLAG_ACTIVITY_NEW_TASK);
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
						intent.putExtra("android.provider.extra.INITIAL_URI", documentUri);
				}
			} else {
				intent = new Intent(Intent.ACTION_VIEW, Uri.parse(uri));
				intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
			}
			startActivity(intent);
			return "";
		} catch (Exception e) {
			e.printStackTrace();
			return e.toString();
		}
	}
}
