#ifdef SDL1_2
#include "common/Translations.h"
#include <climits>
#include <list>

#include "SDLCompat.h"
#ifdef LIN
#include "images.h"
#endif

#include "EventLoopSDL.h"
#include "defines.h"
#include "graphics.h"
#include "interface.h"

#include "common/Platform.h"
#include "common/tpt-minmax.h"
#include "interface/Engine.h"
#include "gui/dialogs/ConfirmPrompt.h"
#include "gui/game/PowderToy.h" // for the_game->DeFocus(), remove once all interfaces get modernized


SDL_Surface *sdl_scrn = nullptr;

bool resizable = false;
int pixelFilteringMode = 0;
bool fullscreen = false;
bool altFullscreen = false;
bool inOldInterface = false;

unsigned short sdl_mod;
std::string sdl_textinput = "";
int sdl_key, sdl_wheel;

// Irrelevant for Android (only thing that uses sdl 1.2)
int savedWindowX = 0;
int savedWindowY = 0;
void StartTextInput() {}
void StopTextInput() {}
void LoadWindowPosition() {}
void SaveWindowPosition() {}

void SDLInit()
{
	// Do nothing, Android port doesn't use pref path
}

int sdl_opened = 0;
int SDLOpen()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Initializing SDL: %s\n", SDL_GetError());
		return 0;
	}

	sdl_scrn = SDL_SetVideoMode(VIDXRES, VIDYRES, 32, SDL_SWSURFACE);
	if (!sdl_scrn)
	{
		fprintf(stderr, "Creating window: %s\n", SDL_GetError());
		return 0;
	}

	SDL_WM_SetCaption("The Powder Toy", "The Powder Toy");
	SDL_EnableUNICODE(1);

	// hide overlay buttons that just get in the way
	SDL_ANDROID_SetScreenKeyboardShown(0);
	//SDL_JoystickOpen(1);

	atexit(SDL_Quit);

	sdl_opened = 1;
	return 1;
}

void SDLSetScreen(bool resizable_, int pixelFilteringMode_, bool fullscreen_, bool altFullscreen_,
				  bool forceIntegerScaling_, bool canRecreateWindow)
{
	// Do nothing, this is for Android only and none of these options are relevant
}

void SDLBlit(pixel * vid)
{
	pixel *dst;
	if (SDL_MUSTLOCK(sdl_scrn))
		if (SDL_LockSurface(sdl_scrn) < 0)
			return;
	dst = (pixel *)sdl_scrn->pixels;
	for (int j = 0; j < VIDYRES; j++)
	{
		memcpy(dst, vid, VIDXRES * PIXELSIZE);
		dst += sdl_scrn->pitch / PIXELSIZE;
		vid += VIDXRES;
	}
	if (SDL_MUSTLOCK(sdl_scrn))
		SDL_UnlockSurface(sdl_scrn);
	SDL_UpdateRect(sdl_scrn, 0, 0, 0, 0);
}

unsigned int CalculateMousePosition(int *x, int *y) { return 0; }
int SDLGetModifiers()
{
	return SDL_GetModState();
}

Point lastMousePosition;

int EventProcess(SDL_Event event, ui::Window * eventHandler)
{
	switch (event.type)
	{
	case SDL_QUIT:
		if (Engine::Ref().IsFastQuit())
			Engine::Ref().Shutdown();
		return 1;
	case SDL_KEYDOWN:
		if (SDL_GetModState() & KMOD_GUI)
			break;

		sdl_key = event.key.keysym.sym; // LEGACY
		sdl_mod = static_cast<unsigned short>(SDL_GetModState()); // LEGACY

		if (eventHandler)
		{
			eventHandler->DoKeyPress(event.key.keysym.sym, event.key.keysym.sym, false, sdl_mod&KMOD_SHIFT, sdl_mod&KMOD_CTRL, sdl_mod&KMOD_ALT);
			char c[2];
			sprintf(c, "%c", event.key.keysym.unicode);
			std::string text = c;
			eventHandler->DoTextInput(text.c_str());
		}

		if (eventHandler && event.key.keysym.sym == SDLK_ESCAPE && eventHandler->CanQuit())
			return 1;
		else if (Engine::Ref().IsGlobalQuit() && event.key.keysym.sym == 'q' && (sdl_mod & (KMOD_CTRL | KMOD_GUI)) && !(sdl_mod & KMOD_ALT))
		{
			bool wasConfirmed = false;
			// Two separate quit dialogs ... remove some day
			if (inOldInterface)
			{
				std::string _tr_quit_title = TrUI("You are about to quit");
				std::string _tr_quit_msg = TrUI("Are you sure you want to quit?");
				std::string _tr_quit_btn = TrUI("Quit");
				wasConfirmed = confirm_ui(vid_buf, _tr_quit_title.c_str(), _tr_quit_msg.c_str(), _tr_quit_btn.c_str());
			}
			else
			{
				auto prompt = new ConfirmPrompt(TrUI("You are about to quit"), TrUI("Are you sure you want to quit?"), TrUI("Quit"));
				prompt->SetCallback({ [&](bool confirmed) {
					wasConfirmed = confirmed;
				} });
				Engine::Ref().ShowWindow(prompt);
				MainLoop(true);
			}
			if (wasConfirmed)
			{
				Engine::Ref().Shutdown();
				return 1;
			}
		}
		break;
	case SDL_KEYUP:
		if (SDL_GetModState() & KMOD_GUI)
			break;

		sdl_mod = static_cast<unsigned short>(SDL_GetModState()); // LEGACY

		if (eventHandler)
			eventHandler->DoKeyRelease(event.key.keysym.sym, event.key.keysym.scancode, false, sdl_mod&KMOD_SHIFT, sdl_mod&KMOD_CTRL, sdl_mod&KMOD_ALT);
		break;
	case SDL_MOUSEBUTTONDOWN:
	{
		int mx = event.motion.x, my = event.motion.y;
		if (mx < 0 || my < 0 || mx >= VIDXRES || my >= VIDYRES)
			break;
		if (eventHandler)
		{
			if (event.button.button == SDL_BUTTON_WHEELUP)
				eventHandler->DoMouseWheel(mx, my, 1);
			else if (event.button.button == SDL_BUTTON_WHEELDOWN)
				eventHandler->DoMouseWheel(mx, my, -1);
			else
				eventHandler->DoMouseDown(mx, my, event.button.button);
		}
		lastMousePosition = Point(mx, my);
		break;
	}
	case SDL_MOUSEBUTTONUP:
	{
		int mx = event.motion.x, my = event.motion.y;
		if (eventHandler)
			eventHandler->DoMouseUp(mx, my, event.button.button);
		lastMousePosition = Point(mx, my);
		break;
	}
	case SDL_MOUSEMOTION:
	{
		int mx = event.motion.x, my = event.motion.y;
		if (eventHandler)
			eventHandler->DoMouseMove(mx, my, mx-lastMousePosition.X, my-lastMousePosition.Y);
		lastMousePosition = Point(mx, my);
		break;
	}
	//case SDL_VIDEORESIZE:
		// screen resize event, we are supposed to call SDL_SetVideoMode with the new size. Ignore this entirely and call it with the old size :)
		// if we don't do this, the touchscreen calibration variables won't ever be set properly
		//sdl_scrn = SDL_SetVideoMode(VIDXRES, VIDYRES, 32, SDL_SWSURFACE);
	}
	return 0;
}

uint32_t lastTick, drawingTimer;
void MainLoop(bool secondaryLoop)
{
	SDL_Event event;
	Engine &engine = Engine::Ref();
	bool isSecondaryEngineLoop = engine.IsSecondaryEngineLoop();
	if (secondaryLoop)
	{
		engine.ProcessWindowUpdates();
		engine.SetSecondaryEngineLoop(true);
	}
	int currentStackSize = engine.GetStackSize();
	while (engine.Running())
	{
		ui::Window * top = engine.GetTop();
		// weird thing done when entering old interfaces that use sdl_poll loops
		if (sendNewEvents)
		{
			int mx, my;
			SDL_GetMouseState(&mx, &my);
			mx /= engine.GetScale();
			my /= engine.GetScale();
			if (Point(mx, my) != lastMousePosition)
			{
				top->DoMouseMove(mx, my, mx-lastMousePosition.X, my-lastMousePosition.Y);
				lastMousePosition = Point(mx, my);
			}
			top->DoKeyRelease(0, 0, false, false, false, false);
			sendNewEvents = false;
		}
		top->UpdateComponents();

		sdl_textinput = "";
		while (SDL_PollEvent(&event))
		{
			int ret = EventProcess(event, top);
			if (ret)
				engine.CloseTop(ui::Escape);
		}

		uint32_t currentTick = SDL_GetTicks();
		top->DoTick(currentTick-lastTick, false);
		drawingTimer += currentTick - lastTick;
		lastTick = currentTick;

		int drawcap = engine.GetDrawingFrequency();
		if (!drawcap || drawingTimer > 1000.f/drawcap)
		{
			drawingTimer = 0;
			top->DoDraw(vid_buf, Point(XRES+BARSIZE, YRES+MENUSIZE), top->GetPosition());
			SDLBlit(vid_buf);
		}
		limit_fps();

		engine.ProcessWindowUpdates();
		if (secondaryLoop && engine.GetStackSize() < currentStackSize)
			break;
		inOldInterface = false;
	}

	if (secondaryLoop)
	{
		engine.SetSecondaryEngineLoop(isSecondaryEngineLoop);
	}
}

void PushToClipboard(std::string text)
{
	SDL_SetClipboardText(text.c_str());
}

std::string PullFromClipboard()
{
	if (SDL_HasClipboardText())
	{
		char *data = SDL_GetClipboardText();
		if (!data)
			return "";
		std::string ret = data;
		SDL_free(data);
		return ret;
	}
	return "";
}

unsigned int GetTicks()
{
	return SDL_GetTicks();
}


void BlueScreen(const char * detailMessage)
{
	//std::string errorDetails = "Details: " + std::string(detailMessage);
	SDL_Event event;
	const char * errorHelp = "An unrecoverable fault has occurred, please report this to jacob1:\n"
		" https://powdertoy.co.uk/Discussions/Thread/View.html?Thread=11117\n"
		" OR the built in bug reporter.\n\n"
		"Note: TPT will now restart and reload your work";
	int positionX = (XRES+BARSIZE)/2-textwidth(errorHelp)/2-50, positionY = (YRES+MENUSIZE)/2-100;

	fillrect(vid_buf, -1, -1, XRES+BARSIZE+1, YRES+MENUSIZE+1, 17, 114, 169, 210);

	drawtext(vid_buf, positionX, positionY, "ERROR", 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY + 14, detailMessage, 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY  + 28, errorHelp, 255, 255, 255, 255);

	pixel* vid_buf2 = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	memcpy(vid_buf2, vid_buf, (XRES+BARSIZE)*(YRES+MENUSIZE)*PIXELSIZE);
	std::vector<Point> food;
	std::list<Point> tron;
	unsigned int tronSize = 5, score = 0;
	int tronDirection = 2;
	char scoreString[20];
	bool gameRunning = false, gameLost = false;
	for (int i = 0; i < 10; i++)
		food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
	for (unsigned int i = 0; i < tronSize; i++)
		tron.push_back(Point(20, 10+i));

	//Death loop
	while(1)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				Platform::DoRestart(true, true);
			}
			else if (event.type == SDL_KEYDOWN)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					if (tronDirection != 2)
						tronDirection = 0;
					break;
				case SDLK_RIGHT:
					if (tronDirection != 3)
						tronDirection = 1;
					break;
				case SDLK_DOWN:
					if (tronDirection != 0)
						tronDirection = 2;
					break;
				case SDLK_LEFT:
					if (tronDirection != 1)
						tronDirection = 3;
					break;
				default:
#ifdef ANDROID
					Platform::DoRestart(true, true);
#endif
					break;
				}
				gameRunning = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				int mx = event.motion.x, my = event.motion.y;
				if (my < (YRES+MENUSIZE)/4)
				{
					if (tronDirection != 2)
						tronDirection = 0;
				}
				else if (my > YRES+MENUSIZE-(YRES+MENUSIZE)/4)
				{
					if (tronDirection != 0)
						tronDirection = 2;
				}
				else if (mx < (XRES+BARSIZE)/4)
				{
					if (tronDirection != 1)
						tronDirection = 3;
				}
				else if (mx > XRES+BARSIZE-(XRES+BARSIZE)/4)
				{
					if (tronDirection != 3)
						tronDirection = 1;
				}
				gameRunning = true;
			}
		}
		if (gameRunning && !gameLost)
		{
			Point tronHead = ((Point)tron.back());
			tronHead.X -= (tronDirection-2)%2;
			tronHead.Y += (tronDirection-1)%2;
			if (tronHead.X > 1 && tronHead.X < XRES+BARSIZE-2 && tronHead.Y > 1 && tronHead.Y < YRES+MENUSIZE-2)
				tron.push_back(tronHead);
			else
				gameLost = true;

			sprintf(scoreString, "Score: %i", score);
			drawtext(vid_buf, XRES-BARSIZE-50, 10, scoreString, 255, 255, 255, 255);
			for (std::vector<Point>::iterator iter = food.begin(); iter != food.end(); ++iter)
			{
				Point moo = (Point)*iter;
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)] = PIXRGB(255, 0, 0);
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)+1] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + moo.Y*(XRES+BARSIZE)-1] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + (moo.Y+1)*(XRES+BARSIZE)] = PIXRGB(100, 0, 0);
				vid_buf[moo.X + (moo.Y-1)*(XRES+BARSIZE)] = PIXRGB(100, 0, 0);
				if (tronHead.X >= moo.X - 3 && tronHead.X <= moo.X + 3 && tronHead.Y >= moo.Y - 3 && tronHead.Y <= moo.Y + 3)
				{
					food.erase(iter, iter+1);
					food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
					food.push_back(Point(rand()%(XRES+BARSIZE-6)+3, rand()%(YRES+MENUSIZE-6)+3));
					tronSize += 20;
					score++;
					break;
				}
			}
			if (!(rand()%20))
				tronSize++;
			int i = tronSize;
			for (std::list<Point>::iterator iter = tron.begin(); iter != tron.end(); ++iter)
			{
				Point point = *iter;
				vid_buf[point.X + point.Y*(XRES+BARSIZE)] = PIXRGB(0, 255-(int)(200.0f/tronSize*i), 0);
				i--;
				if (iter != --tron.end() && point == tronHead)
					gameLost = true;
			}
			while (tron.size() > tronSize)
				tron.pop_front();
		}
		SDLBlit(vid_buf);
		if (gameRunning && !gameLost)
			memcpy(vid_buf, vid_buf2, (XRES+BARSIZE)*(YRES+MENUSIZE)*PIXELSIZE);
		SDL_Delay(5);
	}
}

/*
 * Legacy functions only beyond this point
 */

int FPSwait = 0;
int SDLPoll()
{
	SDL_Event event;
	sdl_key=sdl_wheel=0;
	if (Engine::Ref().IsShutdown())
		return 1;

	if (!inOldInterface)
	{
		inOldInterface = true;
		the_game->OnDefocus();
	}
	sdl_textinput = "";
	while (SDL_PollEvent(&event))
	{
		int ret = EventProcess(event, nullptr);
		if (ret)
			return ret;
	}

	sdl_mod = static_cast<unsigned short>(SDL_GetModState());
	limit_fps();
	sendNewEvents = true;
	return 0;
}

int pastFPS = 0;
float FPSB2 = 60.0f;
double frameTimeAvg = 0.0, correctedFrameTimeAvg = 60.0;
void limit_fps()
{
	int frameTime = SDL_GetTicks() - currentTime;

	frameTimeAvg = frameTimeAvg * .8 + frameTime * .2;
	float limitFPS = Engine::Ref().GetFpsLimit();
	if (limitFPS > 2)
	{
		double offset = 1000.0 / limitFPS - frameTimeAvg;
		if (offset > 0)
			//millisleep((Uint32)(offset + 0.5));
			SDL_Delay((Uint32)(offset + 0.5));
	}

	int correctedFrameTime = SDL_GetTicks() - currentTime;
	correctedFrameTimeAvg = correctedFrameTimeAvg * 0.95 + correctedFrameTime * 0.05;
	elapsedTime = currentTime-pastFPS;
	if (elapsedTime >= 200)
	{
		if (!FPSwait)
			FPSB2 = 1000.0f/(float)correctedFrameTimeAvg;
		if (FPSwait > 0)
			FPSwait--;
		pastFPS = currentTime;
	}
	currentTime = SDL_GetTicks();
}

void sdl_blit(int unused1, int unused2, int unused3, int unused4, pixel *vid, int unused5)
{
	SDLBlit(vid);
}

int mouse_get_state(int *x, int *y)
{
	return SDL_GetMouseState(x, y);
}

int sdl_poll()
{
	return SDLPoll();
}
#endif
