#include "PowderToySDL.h"
#include "SimulationConfig.h"
#include "WindowIcon.h"
#include "Config.h"
#include "gui/interface/Engine.h"
#include "graphics/Graphics.h"
#include "common/platform/Platform.h"
#include "common/clipboard/Clipboard.h"
#include "FrameSchedule.h"
#include <iostream>
#include <cstdarg>

// ---- 渲染诊断（临时）：写入当前工作目录 render-diag.log 并同时进 logcat ----
void TptDiagLog(const char *fmt, ...)
{
	static FILE *diagFile = nullptr;
	static bool tried = false;
	if (!tried)
	{
		tried = true;
		diagFile = fopen("render-diag.log", "w");
	}
	char buf[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (diagFile)
	{
		fputs(buf, diagFile);
		fputc('\n', diagFile);
		fflush(diagFile);
	}
	SDL_Log("TPTDIAG %s", buf);
}

int desktopWidth = 1280;
int desktopHeight = 1024;
SDL_Window *sdl_window = nullptr;
SDL_Renderer *sdl_renderer = nullptr;
SDL_Texture *sdl_texture = nullptr;
bool vsyncHint = false;
WindowFrameOps currentFrameOps;
bool momentumScroll = true;
bool showAvatars = true;
bool showLargeScreenDialog = false;
int mousex = 0;
int mousey = 0;
int mouseButton = 0;
bool mouseDown = false;
bool calculatedInitialMouse = false;
bool hasMouseMoved = false;
double correctedFrameTimeAvg = 0;
static bool prevContributesToFps = false;

static FrameSchedule tickSchedule;
static FrameSchedule drawSchedule;
static FrameSchedule clientTickSchedule;
static FrameSchedule fpsUpdateSchedule;

void StartTextInput()
{
	if (sdl_window)
		SDL_StartTextInput(sdl_window);
}

void StopTextInput()
{
	if (sdl_window)
		SDL_StopTextInput(sdl_window);
}

void SetTextInputRect(int x, int y, int w, int h)
{
	// SDL3: SDL_SetTextInputArea takes window coordinates
	SDL_Rect rect;
	float wx, wy, wwx, why;
	SDL_RenderCoordinatesToWindow(sdl_renderer, float(x), float(y), &wx, &wy);
	SDL_RenderCoordinatesToWindow(sdl_renderer, float(x + w), float(y + h), &wwx, &why);
	rect.x = int(wx);
	rect.y = int(wy);
	rect.w = int(wwx - wx);
	rect.h = int(why - wy);
	if (sdl_window)
		SDL_SetTextInputArea(sdl_window, &rect, 0);
}

void ClipboardPush(ByteString text)
{
	SDL_SetClipboardText(text.c_str());
}

ByteString ClipboardPull()
{
	return ByteString(SDL_GetClipboardText());
}

int GetModifiers()
{
	return SDL_GetModState();
}

unsigned int GetTicks()
{
	return (unsigned int)SDL_GetTicks();
}

uint64_t GetNowNs()
{
	return SDL_GetTicksNS();
}

static void CalculateMousePosition(int *x, int *y)
{
	float globalMx, globalMy;
	SDL_GetGlobalMouseState(&globalMx, &globalMy);
	int windowX, windowY;
	SDL_GetWindowPosition(sdl_window, &windowX, &windowY);

	if (x)
		*x = (int(globalMx) - windowX) / currentFrameOps.scale;
	if (y)
		*y = (int(globalMy) - windowY) / currentFrameOps.scale;
}

void blit(pixel *vid)
{
	static int blitCount = 0;
	bool diag = blitCount < 3;
	if (diag)
	{
		int ww = 0, wh = 0, pw = 0, ph = 0;
		SDL_GetWindowSize(sdl_window, &ww, &wh);
		SDL_GetWindowSizeInPixels(sdl_window, &pw, &ph);
		TptDiagLog("blit#%d win=%dx%d px=%dx%d flags=0x%llx renderer=%s tex=%p",
			blitCount, ww, wh, pw, ph,
			(unsigned long long)SDL_GetWindowFlags(sdl_window),
			sdl_renderer ? SDL_GetRendererName(sdl_renderer) : "(null)",
			(void *)sdl_texture);
		// 源像素：TPT 引擎到底画了东西没有
		int nonZero = 0;
		for (int i = 0; i < WINDOWW * WINDOWH; i += 37)
		{
			if (vid[i] & 0xFFFFFF)
				nonZero += 1;
		}
		TptDiagLog("blit#%d src: logical=%dx%d center=0x%08x sampledNonZero=%d",
			blitCount, WINDOWW, WINDOWH,
			unsigned(vid[(WINDOWH / 2) * WINDOWW + WINDOWW / 2]), nonZero);
		// 渲染输出尺寸与逻辑呈现映射
		int ow = 0, oh = 0;
		SDL_GetRenderOutputSize(sdl_renderer, &ow, &oh);
		int lw = 0, lh = 0;
		SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
		SDL_GetRenderLogicalPresentation(sdl_renderer, &lw, &lh, &mode);
		SDL_Rect vp{};
		SDL_GetRenderViewport(sdl_renderer, &vp);
		TptDiagLog("blit#%d output=%dx%d logicalPresentation=%dx%d mode=%d viewport=%d,%d %dx%d",
			blitCount, ow, oh, lw, lh, int(mode), vp.x, vp.y, vp.w, vp.h);
	}
	if (!SDL_UpdateTexture(sdl_texture, nullptr, vid, WINDOWW * sizeof (Uint32)) && diag)
		TptDiagLog("blit#%d SDL_UpdateTexture failed: %s", blitCount, SDL_GetError());
	// need to clear the renderer if there are black edges (fullscreen, or resizable window)
	if (currentFrameOps.fullscreen || currentFrameOps.resizable)
	{
		if (!SDL_RenderClear(sdl_renderer) && diag)
			TptDiagLog("blit#%d SDL_RenderClear failed: %s", blitCount, SDL_GetError());
	}
	if (!SDL_RenderTexture(sdl_renderer, sdl_texture, nullptr, nullptr) && diag)
		TptDiagLog("blit#%d SDL_RenderTexture failed: %s", blitCount, SDL_GetError());
	if (diag)
	{
		// 关键判定：GPU 帧缓冲里到底有没有画面（present 之前回读）
		SDL_Surface *shot = SDL_RenderReadPixels(sdl_renderer, nullptr);
		if (!shot)
		{
			TptDiagLog("blit#%d SDL_RenderReadPixels failed: %s", blitCount, SDL_GetError());
		}
		else
		{
			int nonZero = 0;
			Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
			for (int y = 0; y < shot->h; y += 7)
			{
				for (int x = 0; x < shot->w; x += 7)
				{
					Uint8 r, g, b, a;
					if (SDL_ReadSurfacePixel(shot, x, y, &r, &g, &b, &a) && (r || g || b))
						nonZero += 1;
				}
			}
			SDL_ReadSurfacePixel(shot, shot->w / 2, shot->h / 2, &cr, &cg, &cb, &ca);
			TptDiagLog("blit#%d readback: %dx%d fmt=%s center=(%d,%d,%d,%d) sampledNonZero=%d",
				blitCount, shot->w, shot->h,
				SDL_GetPixelFormatName(shot->format), cr, cg, cb, ca, nonZero);
			SDL_DestroySurface(shot);
		}
	}
	if (!SDL_RenderPresent(sdl_renderer) && diag)
		TptDiagLog("blit#%d SDL_RenderPresent failed: %s", blitCount, SDL_GetError());
	if (diag)
	{
		TptDiagLog("blit#%d done", blitCount);
	}
	blitCount += 1;
}

void UpdateRefreshRate()
{
	RefreshRate refreshRate;
	SDL_DisplayID displayIndex = SDL_GetDisplayForWindow(sdl_window);
	if (displayIndex)
	{
		const SDL_DisplayMode *displayMode = SDL_GetCurrentDisplayMode(displayIndex);
		if (displayMode && displayMode->refresh_rate)
		{
			refreshRate = RefreshRateQueried{ int(displayMode->refresh_rate) };
		}
	}
	ui::Engine::Ref().SetRefreshRate(refreshRate);
}

void SDLOpen()
{
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "Initializing SDL (video subsystem): %s\n", SDL_GetError());
		Platform::Exit(-1);
	}
	Clipboard::Init();

	SDLSetScreen();

	SDL_DisplayID displayIndex = SDL_GetDisplayForWindow(sdl_window);
	if (displayIndex)
	{
		SDL_Rect rect;
		if (SDL_GetDisplayUsableBounds(displayIndex, &rect))
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
	}
	UpdateRefreshRate();

	StopTextInput();
}
void SDLClose()
{
	if (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_OPENGL)
	{
		// * nvidia-460 egl registers callbacks with x11 that end up being called
		//   after egl is unloaded unless we grab it here and release it after
		//   sdl closes the display. this is an nvidia driver weirdness but
		//   technically an sdl bug. glfw has this fixed:
		//   https://github.com/glfw/glfw/commit/9e6c0c747be838d1f3dc38c2924a47a42416c081
		SDL_GL_LoadLibrary(nullptr);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_GL_UnloadLibrary();
	}
	SDL_Quit();
}

void SDLSetScreen()
{
	auto newFrameOps = ui::Engine::Ref().windowFrameOps;
	auto newVsyncHint = false; // TODO: DrawLimitVsync
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsEmbedded)
	{
		newFrameOps.resizable = false;
		newFrameOps.fullscreen = false;
		newFrameOps.changeResolution = false;
		newFrameOps.forceIntegerScaling = false;
	}
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsHandheld)
	{
		newFrameOps.resizable = false;
		newFrameOps.fullscreen = true;
		newFrameOps.changeResolution = false;
		newFrameOps.forceIntegerScaling = false;
	}

	auto currentFrameOpsNorm = currentFrameOps.Normalize();
	auto newFrameOpsNorm = newFrameOps.Normalize();
	auto recreate = !sdl_window ||
	                // Recreate the window when toggling fullscreen, due to occasional issues
	                newFrameOpsNorm.fullscreen       != currentFrameOpsNorm.fullscreen       ||
	                // Also recreate it when enabling resizable windows, to fix bugs on windows,
	                //  see https://github.com/jacob1/The-Powder-Toy/issues/24
	                newFrameOpsNorm.resizable        != currentFrameOpsNorm.resizable        ||
	                newFrameOpsNorm.changeResolution != currentFrameOpsNorm.changeResolution ||
	                newFrameOpsNorm.blurryScaling    != currentFrameOpsNorm.blurryScaling    ||
	                newVsyncHint != vsyncHint;

	if (!(recreate ||
	      newFrameOpsNorm.scale               != currentFrameOpsNorm.scale               ||
	      newFrameOpsNorm.forceIntegerScaling != currentFrameOpsNorm.forceIntegerScaling))
	{
		return;
	}

	auto size = WINDOW * newFrameOpsNorm.scale;
	if (sdl_window && newFrameOpsNorm.resizable)
	{
		SDL_GetWindowSize(sdl_window, &size.X, &size.Y);
	}

	if (recreate)
	{
		if (sdl_texture)
		{
			SDL_DestroyTexture(sdl_texture);
			sdl_texture = nullptr;
		}
		if (sdl_renderer)
		{
			SDL_DestroyRenderer(sdl_renderer);
			sdl_renderer = nullptr;
		}
		if (sdl_window)
		{
			SaveWindowPosition();
			SDL_DestroyWindow(sdl_window);
			sdl_window = nullptr;
		}

		Uint64 flags = 0;
		bool vsyncOn = false;
		if (newFrameOpsNorm.fullscreen)
		{
			// SDL3 has a single SDL_WINDOW_FULLSCREEN flag; desktop vs. exclusive
			// fullscreen is chosen via SDL_SetWindowFullscreenMode (nullptr = desktop).
			flags = SDL_WINDOW_FULLSCREEN;
		}
		if (newFrameOpsNorm.resizable)
		{
			flags |= SDL_WINDOW_RESIZABLE;
		}
		if (vsyncHint)
		{
			vsyncOn = true;
		}
		sdl_window = SDL_CreateWindow(APPNAME, size.X, size.Y, flags);
		if (!sdl_window)
		{
			fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			Platform::Exit(-1);
		}
		SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		if (newFrameOpsNorm.fullscreen)
		{
			// nullptr mode = borderless desktop fullscreen; a real mode = exclusive/resolution-changing
			SDL_SetWindowFullscreenMode(sdl_window, nullptr);
		}
		if constexpr (SET_WINDOW_ICON)
		{
			WindowIcon(sdl_window);
		}
		SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
		sdl_renderer = SDL_CreateRenderer(sdl_window, nullptr);
		if (!sdl_renderer)
		{
			fprintf(stderr, "SDL_CreateRenderer failed; available renderers:\n");
			int num = SDL_GetNumRenderDrivers();
			for (int i = 0; i < num; ++i)
			{
				const char *name = SDL_GetRenderDriver(i);
				fprintf(stderr, " - %s\n", name ? name : "(null)");
			}
			Platform::Exit(-1);
		}
		SDL_SetRenderVSync(sdl_renderer, vsyncOn ? 1 : SDL_RENDERER_VSYNC_DISABLED);
		SDL_SetRenderLogicalPresentation(sdl_renderer, WINDOWW, WINDOWH,
			newFrameOpsNorm.forceIntegerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
		// SDL3 迁移要点：SDL2 的 SDL_CreateTexture 默认 blend mode 是 NONE，
		// SDL3 改成了 BLEND。TPT 的 pixel 格式最高字节恒为 0（从不写 alpha），
		// 因此在 SDL3 下整个画面会按 alpha=0 混合掉，表现为全黑。
		// 优先用 XRGB8888（alpha 位被忽略，恒不透明），失败再回退 ARGB8888。
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOWW, WINDOWH);
		if (!sdl_texture)
		{
			TptDiagLog("XRGB8888 texture unsupported (%s), falling back to ARGB8888", SDL_GetError());
			sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOWW, WINDOWH);
		}
		if (!sdl_texture)
		{
			fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
			Platform::Exit(-1);
		}
		// 关键修复：禁用混合，让纹理直接覆盖写入，与 SDL2 行为一致。
		SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_NONE);
		SDL_SetTextureScaleMode(sdl_texture, newFrameOpsNorm.blurryScaling ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
		SDL_RaiseWindow(sdl_window);
		Clipboard::RecreateWindow();
		{
			int ww = 0, wh = 0, pw = 0, ph = 0;
			SDL_GetWindowSize(sdl_window, &ww, &wh);
			SDL_GetWindowSizeInPixels(sdl_window, &pw, &ph);
			TptDiagLog("recreate done: requested=%dx%d win=%dx%d px=%dx%d flags=0x%llx renderer=%s",
				size.X, size.Y, ww, wh, pw, ph,
				(unsigned long long)SDL_GetWindowFlags(sdl_window),
				SDL_GetRendererName(sdl_renderer));
		}
	}
	SDL_SetRenderLogicalPresentation(sdl_renderer, WINDOWW, WINDOWH,
		newFrameOpsNorm.forceIntegerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
	if (sdl_texture)
	{
		SDL_SetTextureScaleMode(sdl_texture, newFrameOpsNorm.blurryScaling ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	}
	if (!(newFrameOpsNorm.resizable && SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_MAXIMIZED))
	{
		SDL_SetWindowSize(sdl_window, size.X, size.Y);
		LoadWindowPosition();
	}
	ApplyFpsLimit();
	if (newFrameOpsNorm.fullscreen)
	{
		SDL_RaiseWindow(sdl_window);
	}
	currentFrameOps = newFrameOps;
	vsyncHint = newVsyncHint;
}

static void EventProcess(const SDL_Event &event)
{
	auto &engine = ui::Engine::Ref();
	switch (event.type)
	{
	case SDL_EVENT_QUIT:
		if (ALLOW_QUIT && (engine.GetFastQuit() || engine.CloseWindow()))
		{
			engine.Exit();
		}
		break;
	case SDL_EVENT_KEY_DOWN:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		if (engine.GetGlobalQuit() && ALLOW_QUIT && !event.key.repeat && event.key.key == 'q' && (event.key.mod&SDL_KMOD_CTRL) && !(event.key.mod&SDL_KMOD_ALT))
			engine.ConfirmExit();
		else
			engine.onKeyPress(event.key.key, event.key.scancode, event.key.repeat, event.key.mod&SDL_KMOD_SHIFT, event.key.mod&SDL_KMOD_CTRL, event.key.mod&SDL_KMOD_ALT);
		break;
	case SDL_EVENT_KEY_UP:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onKeyRelease(event.key.key, event.key.scancode, event.key.repeat, event.key.mod&SDL_KMOD_SHIFT, event.key.mod&SDL_KMOD_CTRL, event.key.mod&SDL_KMOD_ALT);
		break;
	case SDL_EVENT_TEXT_INPUT:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onTextInput(ByteString(event.text.text).FromUtf8());
		break;
	case SDL_EVENT_TEXT_EDITING:
		if (SDL_GetModState() & SDL_KMOD_GUI)
		{
			break;
		}
		engine.onTextEditing(ByteString(event.edit.text).FromUtf8(), event.edit.start);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
	{
		// int x = event.wheel.x;
		int y = int(event.wheel.y);
		if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
		{
			// x *= -1;
			y *= -1;
		}

		engine.onMouseWheel(mousex, mousey, y); // TODO: pass x?
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		mousex = int(event.motion.x);
		mousey = int(event.motion.y);
		engine.onMouseMove(mousex, mousey);

		hasMouseMoved = true;
		break;
	case SDL_EVENT_DROP_FILE:
		engine.onFileDrop(event.drop.data);
		// SDL3 owns event.drop.data; do not free it.
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = int(event.button.x);
			mousey = int(event.button.y);
		}
		mouseButton = event.button.button;
		engine.onMouseDown(mousex, mousey, mouseButton);

		mouseDown = true;
		if constexpr (!DEBUG)
		{
			SDL_CaptureMouse(true);
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = int(event.button.x);
			mousey = int(event.button.y);
		}
		mouseButton = event.button.button;
		engine.onMouseUp(mousex, mousey, mouseButton);

		mouseDown = false;
		if constexpr (!DEBUG)
		{
			SDL_CaptureMouse(false);
		}
		break;
	case SDL_EVENT_WINDOW_SHOWN:
		if (!calculatedInitialMouse)
		{
			//initial mouse coords, sdl won't tell us this if mouse hasn't moved
			CalculateMousePosition(&mousex, &mousey);
			engine.initialMouse(mousex, mousey);
			engine.onMouseMove(mousex, mousey);
			calculatedInitialMouse = true;
		}
		break;
	case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
		UpdateRefreshRate();
		break;
	}
}

std::optional<uint64_t> EngineProcess()
{
	auto &engine = ui::Engine::Ref();

	{
		auto nowNs = GetNowNs();
		if (clientTickSchedule.HasElapsed(nowNs))
		{
			TickClient();
			clientTickSchedule.SetNow(nowNs);
		}
		clientTickSchedule.Arm(10);
		if (fpsUpdateSchedule.HasElapsed(nowNs))
		{
			engine.SetFps(1e9f / correctedFrameTimeAvg);
			fpsUpdateSchedule.SetNow(nowNs);
		}
		fpsUpdateSchedule.Arm(5);
	}

	if (showLargeScreenDialog)
	{
		showLargeScreenDialog = false;
		LargeScreenDialog();
	}
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		// 诊断：先记下转换前的原始坐标。
		float rawX = 0.f, rawY = 0.f;
		switch (event.type)
		{
		case SDL_EVENT_MOUSE_MOTION:
			rawX = event.motion.x; rawY = event.motion.y; break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			rawX = event.button.x; rawY = event.button.y; break;
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_MOTION:
			rawX = event.tfinger.x; rawY = event.tfinger.y; break;
		default:
			break;
		}
		// SDL3 迁移要点：SDL2 在设置了 SDL_RenderSetLogicalSize 后会自动把鼠标/触摸
		// 事件坐标换算成逻辑坐标；SDL3 移除了这个隐式行为，必须显式转换，否则事件
		// 带的是窗口像素坐标（Android 上是全屏尺寸），点击位置会完全错位。
		if (sdl_renderer)
		{
			SDL_ConvertEventToRenderCoordinates(sdl_renderer, &event);
		}
		{
			// 诊断：记录前若干个指针事件的原始与转换后坐标，确认映射正确。
			static int ptrDiagCount = 0;
			if (ptrDiagCount < 12 &&
				(event.type == SDL_EVENT_MOUSE_MOTION ||
				 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
				 event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
				 event.type == SDL_EVENT_FINGER_DOWN ||
				 event.type == SDL_EVENT_FINGER_UP ||
				 event.type == SDL_EVENT_FINGER_MOTION))
			{
				++ptrDiagCount;
				if (event.type == SDL_EVENT_MOUSE_MOTION)
					TptDiagLog("ptr#%d MOTION raw=(%.1f,%.1f) conv=(%.1f,%.1f) which=%u",
						ptrDiagCount, double(rawX), double(rawY), double(event.motion.x), double(event.motion.y), unsigned(event.motion.which));
				else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
					TptDiagLog("ptr#%d BUTTON%s raw=(%.1f,%.1f) conv=(%.1f,%.1f) btn=%u which=%u",
						ptrDiagCount, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "DOWN" : "UP",
						double(rawX), double(rawY), double(event.button.x), double(event.button.y),
						unsigned(event.button.button), unsigned(event.button.which));
				else
					TptDiagLog("ptr#%d FINGER type=%u raw=(%.3f,%.3f) conv=(%.1f,%.1f)",
						ptrDiagCount, unsigned(event.type), double(rawX), double(rawY),
						double(event.tfinger.x), double(event.tfinger.y));
			}
		}
		EventProcess(event);
	}


	std::optional<uint64_t> delay;
	auto nowNs = GetNowNs();
	auto effectiveDrawLimit = engine.GetEffectiveDrawCap();
	auto doDraw = !effectiveDrawLimit || drawSchedule.HasElapsed(nowNs);
	auto fpsLimit = ui::Engine::Ref().GetFpsLimit();
	auto doSimTick = true;
	if (std::holds_alternative<FpsLimitExplicit>(fpsLimit))
	{
		doSimTick = tickSchedule.HasElapsed(nowNs);
	}
	else if (std::holds_alternative<FpsLimitFollowDraw>(fpsLimit))
	{
		doSimTick = doDraw;
	}
	if (doDraw)
	{
		engine.Tick();
	}
	if (doSimTick)
	{
		auto thisContributesToFps = engine.GetContributesToFps();
		if (prevContributesToFps && thisContributesToFps)
		{
			auto correctedFrameTime = tickSchedule.GetFrameTime();
			correctedFrameTimeAvg = correctedFrameTimeAvg + (correctedFrameTime - correctedFrameTimeAvg) * 0.05;
		}
		prevContributesToFps = thisContributesToFps;
		engine.SimTick();
		tickSchedule.SetNow(nowNs);
	}
	if (doDraw)
	{
		engine.Draw();
		drawSchedule.SetNow(nowNs);
		SDLSetScreen();
		blit(engine.g->Data());
	}
	else
	{
		static int noDrawLogs = 0;
		if (noDrawLogs < 3)
		{
			TptDiagLog("EngineProcess: doDraw=false (drawLimit=%d)",
				effectiveDrawLimit ? int(*effectiveDrawLimit) : -1);
			noDrawLogs += 1;
		}
	}
	if (effectiveDrawLimit)
	{
		delay = drawSchedule.Arm(float(*effectiveDrawLimit)) / UINT64_C(1'000'000);
	}
	if (auto *fpsLimitExplicit = std::get_if<FpsLimitExplicit>(&fpsLimit))
	{
		auto simDelay = tickSchedule.Arm(fpsLimitExplicit->value) / UINT64_C(1'000'000);
		if (delay.has_value() && simDelay < *delay)
		{
			delay = simDelay;
		}
	}
	else if (std::holds_alternative<FpsLimitNone>(fpsLimit))
	{
		delay.reset();
	}
	return delay;
}
