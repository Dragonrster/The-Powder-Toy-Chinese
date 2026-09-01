package @APPID@;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import java.lang.reflect.Method;

public final class VirtualControls
{
	private static boolean ctrlDown = false, shiftDown = false, zDown = false;
	private static Method keyDown, keyUp, mouse;
	private static LinearLayout keyboardPanel;
	private static LinearLayout rightPanel, leftPanel;
	private static ViewGroup overlay;
	private static View handle;
	private static float offsetX = 0, offsetY = 0;
	private static float sx, sy;
	private static Activity staticActivity;

	private static int activeJoystickDir = -1;
	private static final int[] JOYSTICK_CODES = { 0x33, 0x2f, 0x1d, 0x20 };

	private VirtualControls() {}

	public static void attach(final Activity activity)
	{
		try
		{
			Class<?> sdl = Class.forName("org.libsdl.app.SDLActivity");
			keyDown = sdl.getMethod("onNativeKeyDown", Integer.TYPE);
			keyUp = sdl.getMethod("onNativeKeyUp", Integer.TYPE);
			mouse = sdl.getMethod("onNativeMouse", Integer.TYPE, Integer.TYPE, Float.TYPE, Float.TYPE, Boolean.TYPE);
			staticActivity = activity;

			View content = activity.findViewById(0x1020002);
			if (!(content instanceof ViewGroup)) return;

			overlay = new FrameLayout(activity);
			((ViewGroup) content).addView(overlay, new ViewGroup.LayoutParams(
					ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

			// === LEFT PANEL: joystick only ===
			leftPanel = new LinearLayout(activity);
			leftPanel.setOrientation(LinearLayout.VERTICAL);
			leftPanel.setGravity(Gravity.CENTER);
			int p5 = dp(5);
			leftPanel.setPadding(p5, p5, p5, p5);
			leftPanel.setBackground(bg(0xd7, 0x18, 0x1a, 0x1e));
			FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(dp(0x5a),
					ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.LEFT | Gravity.TOP);
			lp.leftMargin = dp(5); lp.topMargin = dp(6);
			overlay.addView(leftPanel, lp);
			leftPanel.addView(buildJoystick(), new LinearLayout.LayoutParams(dp(0x5a), dp(0x5a)));

			// === RIGHT PANEL: quick actions ===
			rightPanel = new LinearLayout(activity);
			rightPanel.setOrientation(LinearLayout.VERTICAL);
			rightPanel.setGravity(Gravity.CENTER);
			rightPanel.setPadding(p5, p5, p5, p5);
			rightPanel.setBackground(bg(0xd7, 0x18, 0x1a, 0x1e));
			FrameLayout.LayoutParams qp = new FrameLayout.LayoutParams(dp(0x5a),
					ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.RIGHT | Gravity.CENTER_VERTICAL);
			qp.rightMargin = dp(5);
			overlay.addView(rightPanel, qp);

			addAction(rightPanel, "Wheel +", new View.OnClickListener() {
				@Override public void onClick(View v) { wheel(5); }
			});
			addAction(rightPanel, "Wheel -", new View.OnClickListener() {
				@Override public void onClick(View v) { wheel(-5); }
			});
			addKey(rightPanel, "Esc", 0x6f);
			addKey(rightPanel, "Space", 0x3e);
			addToggle(rightPanel, "Ctrl", 0x71, 0);
			addToggle(rightPanel, "Shift", 0x3b, 1);
			addToggle(rightPanel, "Z", 0x36, 2);
			addAction(rightPanel, "Keys", new View.OnClickListener() {
				@Override public void onClick(View v) {
					boolean show = keyboardPanel.getVisibility() != View.VISIBLE;
					keyboardPanel.setVisibility(show ? View.VISIBLE : View.GONE);
				}
			});

			// === KEYBOARD (fixed, fully visible) ===
			keyboardPanel = buildKeyboard();
			keyboardPanel.setVisibility(View.GONE);
			FrameLayout.LayoutParams kp = new FrameLayout.LayoutParams(
					ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT,
					Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
			kp.leftMargin = dp(5); kp.rightMargin = dp(5); kp.bottomMargin = dp(5);
			overlay.addView(keyboardPanel, kp);

			// === HANDLE (toggle only, no drag) ===
			handle = button("X");
			handle.setContentDescription("Toggle virtual controls");
			FrameLayout.LayoutParams hp = new FrameLayout.LayoutParams(
					dp(0x2a), dp(0x2a), Gravity.TOP | Gravity.RIGHT);
			hp.topMargin = dp(6); hp.rightMargin = dp(6);
			overlay.addView(handle, hp);

			handle.setOnClickListener(new View.OnClickListener() {
				@Override public void onClick(View v) {
					boolean was = rightPanel.getVisibility() == View.VISIBLE;
					rightPanel.setVisibility(was ? View.GONE : View.VISIBLE);
					leftPanel.setVisibility(was ? View.GONE : View.VISIBLE);
					if (was) keyboardPanel.setVisibility(View.GONE);
					((Button)handle).setText(was ? "K" : "X");
				}
			});
		} catch (Throwable t) {}
	}

	// ========== KEYBOARD ==========
	private static LinearLayout buildKeyboard() {
		LinearLayout panel = new LinearLayout(staticActivity);
		panel.setOrientation(LinearLayout.VERTICAL);
		panel.setGravity(Gravity.CENTER_HORIZONTAL);
		panel.setBackground(bg(0xd7, 0x18, 0x1a, 0x1e));
		int p4 = dp(4); panel.setPadding(p4, p4, p4, p4);

		addRow(panel, split("1 2 3 4 5 6 7 8 9 0"),
			new int[]{0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x07}, true);
		addRow(panel, split("F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12"),
			new int[]{0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e}, true);
		addRow(panel, split("Q W E R T Y U I O P"), codes("QWERTYUIOP"), true);
		addRow(panel, split("A S D F G H J K L"), codes("ASDFGHJKL"), true);
		LinearLayout bot = addRow(panel, split("X C V B N M"), codes("XCVBNM"), false);
		addKey(bot, "Tab", 0x3d); addKey(bot, "Enter", 0x42);
		addKey(bot, "Bksp", 0x43); addKey(bot, "\u2190", 0x15);
		addKey(bot, "\u2191", 0x13); addKey(bot, "\u2193", 0x14); addKey(bot, "\u2192", 0x16);
		return panel;
	}
	private static String[] split(String s) { return s.split(" "); }

	// ========== JOYSTICK ==========
	private static View buildJoystick() {
		final View stick = new View(staticActivity);
		stick.setBackground(bg(0xe6, 0x37, 0x3b, 0x42));
		stick.setMinimumWidth(dp(0x5a)); stick.setMinimumHeight(dp(0x5a));
		stick.setOnTouchListener(new View.OnTouchListener() {
			@Override public boolean onTouch(View v, MotionEvent e) {
				float cx = v.getWidth() / 2f, cy = v.getHeight() / 2f;
				float dx = e.getX() - cx, dy = e.getY() - cy;
				int old = activeJoystickDir;
				switch (e.getAction()) {
					case MotionEvent.ACTION_DOWN:
					case MotionEvent.ACTION_MOVE: {
						int nd = -1;
						float th = Math.min(v.getWidth(), v.getHeight()) * 0.15f;
						if (Math.abs(dx) >= th || Math.abs(dy) >= th)
							nd = Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 3 : 2) : (dy > 0 ? 1 : 0);
						if (nd != old) {
							if (old != -1) invoke(keyUp, JOYSTICK_CODES[old]);
							if (nd != -1) invoke(keyDown, JOYSTICK_CODES[nd]);
							activeJoystickDir = nd;
						}
						return true;
					}
					case MotionEvent.ACTION_UP:
					case MotionEvent.ACTION_CANCEL:
						if (old != -1) invoke(keyUp, JOYSTICK_CODES[old]);
						activeJoystickDir = -1;
						return true;
				}
				return false;
			}
		});
		return stick;
	}

	// ========== KEY HELPERS ==========
	private static void addKey(LinearLayout panel, String label, int code) {
		Button b = button(label);
		panel.addView(b, buttonParams());
		setKey(b, code);
	}

	private static void setKey(final Button b, final int code) {
		final Handler h = new Handler(Looper.getMainLooper());
		final Runnable repeat = new Runnable() {
			@Override public void run() {
				invoke(keyDown, code);
				invoke(keyUp, code);
				h.postDelayed(this, 50);
			}
		};
		b.setOnTouchListener(new View.OnTouchListener() {
			@Override public boolean onTouch(View v, MotionEvent e) {
				switch (e.getAction()) {
					case MotionEvent.ACTION_DOWN:
						invoke(keyDown, code);
						h.postDelayed(repeat, 300);
						return true;
					case MotionEvent.ACTION_UP:
					case MotionEvent.ACTION_CANCEL:
						invoke(keyUp, code);
						h.removeCallbacks(repeat);
						return true;
				}
				return false;
			}
		});
		b.setClickable(true);
	}

	private static LinearLayout addRow(LinearLayout parent, String[] labels, int[] codes, boolean centered) {
		LinearLayout row = new LinearLayout(staticActivity);
		row.setOrientation(LinearLayout.HORIZONTAL);
		row.setGravity(Gravity.CENTER_HORIZONTAL);
		parent.addView(row, new LinearLayout.LayoutParams(centered ? ViewGroup.LayoutParams.MATCH_PARENT : ViewGroup.LayoutParams.WRAP_CONTENT, 0, 1f));
		for (int i = 0; i < labels.length; i++) addKey(row, labels[i], codes[i]);
		return row;
	}

	private static int[] codes(String s) {
		int[] r = new int[s.length()];
		for (int i = 0; i < s.length(); i++) r[i] = s.charAt(i) + 0x1d - 0x41;
		return r;
	}

	// ========== TOGGLE ==========
	private static void addToggle(LinearLayout panel, String label, int code, final int which) {
		final Button b = button(label);
		panel.addView(b, buttonParams());
		b.setOnClickListener(new View.OnClickListener() {
			@Override public void onClick(View v) {
				boolean now;
				if (which == 0) { now = !ctrlDown; ctrlDown = now; }
				else if (which == 1) { now = !shiftDown; shiftDown = now; }
				else { now = !zDown; zDown = now; }
				invoke(now ? keyDown : keyUp, code);
				b.setTextColor(now ? Color.rgb(0xff, 0xd2, 0x46) : 0xFFFFFFFF);
			}
		});
	}

	private static void addAction(LinearLayout panel, String label, View.OnClickListener l) {
		Button b = button(label);
		panel.addView(b, buttonParams());
		b.setOnClickListener(l);
	}

	// ========== UI HELPERS ==========
	private static Button button(String text) {
		Button b = new Button(staticActivity);
		b.setText(text);
		b.setTextColor(0xFFFFFFFF);
		b.setTextSize(11);
		b.setGravity(Gravity.CENTER);
		int p = dp(3);
		b.setPadding(p, 0, p, 0);
		b.setMinWidth(0); b.setMinHeight(0);
		b.setAllCaps(false);
		b.setBackground(bg(0xe6, 0x37, 0x3b, 0x42));
		if (Build.VERSION.SDK_INT >= 26) b.setTooltipText(text);
		return b;
	}
	private static LinearLayout.LayoutParams buttonParams() {
		LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(0x3a), dp(0x22));
		int m = dp(2); lp.setMargins(m, m, m, m);
		return lp;
	}
	private static GradientDrawable bg(int a, int r, int g, int b) {
		GradientDrawable gd = new GradientDrawable();
		gd.setColor(Color.argb(a, r, g, b));
		gd.setCornerRadius(8.0f);
		gd.setStroke(1, Color.argb(0xbe, 0x91, 0x96, 0xa0));
		return gd;
	}
	private static int dp(int value) {
		DisplayMetrics metrics = staticActivity.getResources().getDisplayMetrics();
		return (int) (value * metrics.density + 0.5f);
	}
	private static void tap(int code) { invoke(keyDown, code); invoke(keyUp, code); }
	private static void wheel(float delta) {
		try { mouse.invoke(null, 0, 8, 0.0f, delta, false); }
		catch (Throwable t) {}
	}
	private static void invoke(Method m, int code) {
		try { m.invoke(null, code); }
		catch (Throwable t) {}
	}
}