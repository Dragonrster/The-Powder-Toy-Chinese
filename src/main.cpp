/**
 * Powder Toy - Main source
 *
 * Copyright (c) 2008 - 2011 Stanislaw Skowronek.
 * Copyright (c) 2010 - 2011 Simon Robertshaw
 * Copyright (c) 2010 - 2011 Skresanov Savely
 * Copyright (c) 2010 - 2011 Victoria Hoyle
 * Copyright (c) 2010 - 2011 Nathan Cousins
 * Copyright (c) 2010 - 2011 cracker64
 * Copyright (c) 2011 jacksonmj
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/profile/ProfileViewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <math.h>
#include "EventLoopSDL.h"
#include "SDLCompat.h" // Required for windows versions to find SDL_Main
#include <bzlib.h>
#include <time.h>
#include <signal.h>
#include <list>
#include <sstream>

#ifdef WIN
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef X86_SSE
#include <xmmintrin.h>
#endif
#ifdef X86_SSE3
#include <pmmintrin.h>
#endif

#ifdef LIN
#include "images.h"
#endif
#include "defines.h"
#include "misc.h"
#include "font.h"
#include "powder.h"
#include "graphics.h"
#include "powdergraphics.h"
#include "interface.h"
#include "IntroText.h"
#include "md5.h"
#include "update.h"
#include "common/Translations.h"
#include "legacy_console.h"
#include "luaconsole.h"
#include "luascriptinterface.h"
#include "save_legacy.h"
#include "hud.h"
#include "benchmark.h"

#include "common/Platform.h"
#include "common/tpt-minmax.h"
#include "game/Authors.h"
#include "game/Brush.h"
#include "game/Menus.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "game/ToolTip.h"
#include "game/Request.h"
#include "game/RequestManager.h"
#include "game/Stamps.h"
#include "simulation/Simulation.h"
#include "simulation/SnapshotHistory.h"
#include "simulation/Tool.h"
#include "simulation/ToolNumbers.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/STKM.h"

// new interface stuff
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "graphics/FTText.h"
#include "interface/Engine.h"
#include "gui/dialogs/ErrorPrompt.h"
#include "gui/dialogs/InfoPrompt.h"
#include "gui/game/PowderToy.h"

pixel *vid_buf;

int ptsaveOpenID = 0;
int saveURIOpen = 0;
Save * saveDataOpen = NULL;

bool firstRun = false;
bool redirectStd = false;
bool showLargeScreenDialog = false;
int screenWidth = 0;
int screenHeight = 0;
int do_open = 0;
bool sys_pause = false;
bool legacy_enable = false; //Used to disable new features such as heat, will be set by save.
bool aheat_enable = false; //Ambient heat
bool decorations_enable = true;
bool hud_enable = true;
int active_menu = SC_POWDERS;
int last_active_menu = 0;
int last_fav_menu = SC_FAV;
int framerender = 0;
bool pretty_powder = false;
int finding = 0;
int foundParticles = 0;
int highesttemp = MAX_TEMP;
int lowesttemp = MIN_TEMP;
int heatmode = 0;
int secret_els = 0;
int tab_num = 1;
int num_tabs = 1;
bool show_tabs = false;
Tool* activeTools[3];
int autosave = 0;
bool explUnlocked = false;
int old_menu = 0;
bool doUpdates = true;
bool loadIncompatibleSaves = false;

int drawinfo = 0;
int elapsedTime = 0;
int currentTime = 0;
int totaltime = 0;
int totalafktime = 0;
int afktime = 0;
double totalfps = 0;
int frames = 0;
int prevafktime = 0;
int timesplayed = 0;

bool console_mode = false;
bool graveExitsConsole = true;
bool REPLACE_MODE = false;
bool SPECIFIC_DELETE = false;
int GRID_MODE;
int DEBUG_MODE;
int debug_flags = 0;
int debug_perf_istart = 1;
int debug_perf_iend = 0;
long debug_perf_frametime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_partitime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_time = 0;

int numCores = 4;

int core_count()
{
	int numCPU = 1;
#ifdef MT
#ifdef WIN
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	numCPU = sysinfo.dwNumberOfProcessors;
#else
#ifdef MACOSX
	numCPU = 4;
#else
	numCPU = sysconf( _SC_NPROCESSORS_ONLN );
#endif
#endif

	printf("Cpus: %d\n", numCPU);
	if (numCPU>1)
		printf("Multithreading enabled\n");
	else
		printf("Multithreading disabled\n");
#endif
	return numCPU;
}

void clear_sim()
{
	memset(bmap, 0, sizeof(bmap));
	globalSim->Clear();
	memset(emap, 0, sizeof(emap));
	ClearSigns();
	memset(parts, 0, sizeof(particle)*NPART);
	for (int i = 0; i < NPART-1; i++)
		parts[i].life = i+1;
	parts[NPART-1].life = -1;
	memset(pmap, 0, sizeof(pmap));
	memset(photons, 0, sizeof(photons));
	memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
	memset(fire_r, 0, sizeof(fire_r));
	memset(fire_g, 0, sizeof(fire_g));
	memset(fire_b, 0, sizeof(fire_b));
	memset(fire_alpha, 0, sizeof(fire_alpha));
	prepare_alpha(1.0f);
	finding &= 0x8;
#ifdef LUACONSOLE
	if (LuaCode)
	{
		free(LuaCode);
		LuaCode = NULL;
	}
#endif
	authors.clear();
}

void NewSim()
{
	globalSim->air->ClearTemporarySettings();
	clear_sim();
	clear_save_info();
	legacy_enable = 0;
	globalSim->gravityMode = GRAV_VERTICAL;
	globalSim->customGravityX = 0.0f;
	globalSim->customGravityY = 0.0f;
	globalSim->air->airMode = 0;
}


int clipboard_ready = 0;
void *clipboard_data = 0;
int clipboard_length = 0;

void tab_save(int num)
{
	FILE *f;
	int fileSize;
	char fileName[64];

	sprintf(fileName, "tabs" PATH_SEP "%d.stm", num);

	Json::Value tabInfo;
	tabInfo["type"] = "tab";
	tabInfo["username"] = svf_user;
	tabInfo["num"] = num;
	tabInfo["date"] = (Json::Value::UInt64)time(NULL);
	SaveAuthorInfo(&tabInfo);

	//build the tab
	Save *tab = globalSim->CreateSave(0, 0, XRES, YRES, true);
	tab->authors = tabInfo;
	try
	{
		tab->BuildSave();
	}
	catch (BuildException & e)
	{
		ErrorPrompt * error = new ErrorPrompt(TrUI("Could not create tab: ") + std::string(e.what()));
		Engine::Ref().ShowWindow(error);
		delete tab;
		return;
	}

#ifdef WIN
	_mkdir("tabs");
#else
	mkdir("tabs", 0755);
#endif

	//save the tab
	f = fopen(fileName, "wb");
	if (!f)
		return;
	fwrite(tab->GetSaveData(), tab->GetSaveSize(), 1, f);
	fclose(f);
	the_game->SetReloadPoint(tab);
	delete tab;

	//set the tab's name
	if (strlen(svf_name))
		sprintf(tabNames[num-1], "%s", svf_name);
	else if (strlen(svf_filename))
		sprintf(tabNames[num-1], "%s", svf_filename);
	else
		sprintf(tabNames[num-1], "Untitled Simulation %i", num);
	
	//set the tab's thumbnail
	if (tabThumbnails[num-1])
		free(tabThumbnails[num-1]);
	//the fillrect is to cover up the quickoptions so they aren't in the thumbnail. the &filesize is just a dummy int variable that's unused
	fillrect(vid_buf, XRES, 0, BARSIZE, YRES, 0, 0, 0, 255);
	tabThumbnails[num-1] = rescale_img(vid_buf, XRES+BARSIZE, YRES, &fileSize, &fileSize, 3);
}

int tab_load(int tabNum, bool del, bool showException)
{
	char *saveData;
	int saveSize;
	char fileName[64];

	sprintf(fileName, "tabs" PATH_SEP "%d.stm", tabNum);
	saveData = (char*)file_load(fileName, &saveSize);
	if (saveData)
	{
		// Prevent crash loops on startup
		if (del)
			remove(fileName);
		SnapshotHistory::TakeSnapshot(globalSim);

		Save *save = new Save(saveData, saveSize);
		bool ret = false;
		try
		{
			auto saveLoadData = globalSim->LoadSave(0, 0, save, 2);
			authors = saveLoadData.authors;
			the_game->SetReloadPoint(save);
			ret = true;
		}
		catch (ParseException & e)
		{
			if (showException)
				Engine::Ref().ShowWindow(new InfoPrompt(TrUI("Error loading save"), e.what()));
			else
				std::cout << "Error loading tab: " << e.what() << std::endl;
		}

		delete save;
		free(saveData);
		return ret;
	}
	return 0;
}

void thumb_cache_inval(char *id);

char *thumb_cache_id[THUMB_CACHE_SIZE];
void *thumb_cache_data[THUMB_CACHE_SIZE];
int thumb_cache_size[THUMB_CACHE_SIZE];
int thumb_cache_lru[THUMB_CACHE_SIZE];

void thumb_cache_inval(char *id)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return;
	free(thumb_cache_id[i]);
	free(thumb_cache_data[i]);
	thumb_cache_id[i] = NULL;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] > thumb_cache_lru[i])
			thumb_cache_lru[j]--;
}
void thumb_cache_add(char *id, void *thumb, int size)
{
	int i,m=-1,j=-1;
	thumb_cache_inval(id);
	if (!thumb || size == 0)
		return;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
	{
		if (!thumb_cache_id[i])
			break;
		if (thumb_cache_lru[i] > m)
		{
			m = thumb_cache_lru[i];
			j = i;
		}
	}
	if (i >= THUMB_CACHE_SIZE)
	{
		thumb_cache_inval(thumb_cache_id[j]);
		i = j;
	}
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		thumb_cache_lru[j] ++;
	thumb_cache_id[i] = mystrdup(id);
	thumb_cache_data[i] = malloc(size);
	memcpy(thumb_cache_data[i], thumb, size);
	thumb_cache_size[i] = size;
	thumb_cache_lru[i] = 0;
}
bool thumb_cache_find(char *id, void **thumb, int *size)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return false;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] < thumb_cache_lru[i])
			thumb_cache_lru[j]++;
	thumb_cache_lru[i] = 0;
	*thumb = malloc(thumb_cache_size[i]);
	*size = thumb_cache_size[i];
	memcpy(*thumb, thumb_cache_data[i], *size);
	return true;
}

char http_proxy_string[256] = "";

unsigned short last_major=0, last_minor=0, last_build=0, last_modbuild=0, update_flag=0;

#ifdef RENDERER
int main(int argc, char *argv[])
{
	pixel *vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	int load_size, i=0, j=0;
	void *load_data = file_load(argv[1], &load_size);
	unsigned char c[3];
	char ppmfilename[256], ptifilename[256], ptismallfilename[256];
	FILE *f;

	//init some new c++ stuff
	Simulation *mainSim = new Simulation();
	mainSim->InitElements();
	globalSim = mainSim;

	render_mode = Renderer::Ref().GetRenderModesRaw();
	display_mode = Renderer::Ref().GetDisplayModesRaw();
	Renderer::Ref().SetColorMode(COLOR_DEFAULT);
	init_display_modes();
	TRON_init_graphics();

	sys_pause = 1;
	parts = (particle*)calloc(sizeof(particle), NPART);
	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);
	clear_sim();

	prepare_alpha(CELL, 1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);

	sprintf(ppmfilename, "%s.ppm", argv[2]);
	sprintf(ptifilename, "%s.pti", argv[2]);
	sprintf(ptismallfilename, "%s-small.pti", argv[2]);

	if (load_data && load_size)
	{
		int parsestate = 0;
		//parsestate = parse_save(load_data, load_size, 1, 0, 0);
		parsestate = parse_save(load_data, load_size, 1, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);

		//decorations_enable = 0;
		render_before(vid_buf);
		render_after(vid_buf, vid_buf, Point(0,0));
		for (i=0; i<30; i++)
		{
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
			render_parts(vid_buf, Point(0,0));
			render_fire(vid_buf);
		}
		render_before(vid_buf);
		render_after(vid_buf, vid_buf, Point(0,0));

		if (parsestate>0)
		{
			//return 0;
			info_box(vid_buf, "Save file invalid or from newer version");
		}

		//Save PTi images
		char * datares = NULL, *scaled_buf;
		int res = 0, sw, sh;
		datares = (char*)ptif_pack(vid_buf, XRES, YRES, &res);
		if (datares!=NULL)
		{
			f=fopen(ptifilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		scaled_buf = (char*)resample_img(vid_buf, XRES, YRES, XRES/GRID_Z, YRES/GRID_Z);
		datares = (char*)ptif_pack((pixel*)scaled_buf, XRES/GRID_Z, YRES/GRID_Z, &res);
		if (datares!=NULL)
		{
			f=fopen(ptismallfilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		free(scaled_buf);
		//Save PPM image
		f=fopen(ppmfilename, "wb");
		fprintf(f,"P6\n%d %d\n255\n",XRES,YRES);
		for (j=0; j<YRES; j++)
		{
			for (i=0; i<XRES; i++)
			{
				c[0] = PIXR(vid_buf[i]);
				c[1] = PIXG(vid_buf[i]);
				c[2] = PIXB(vid_buf[i]);
				fwrite(c,3,1,f);
			}
			vid_buf+=XRES+BARSIZE;
		}
		fclose(f);
		
		return 1;
	}

	return 0;
}
#else

void SigHandler(int sig)
{
	signal(SIGSEGV, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGABRT, SIG_DFL);

	switch (sig)
	{
	case SIGSEGV:
		BlueScreen("Memory read/write error");
		break;
	case SIGFPE:
		BlueScreen("Floating point exception");
		break;
	case SIGILL:
		BlueScreen("Program execution exception");
		break;
	case SIGABRT:
		BlueScreen("Unexpected program abort");
		break;
	}
}

//int main(int argc, char *argv[])
//{
	pixel *part_vbuf; //Extra video buffer
	pixel *part_vbuf_store;
	int afk = 0, afkstart = 0, lastx = 1, lasty = 0; // afk tracking for stats
	bool mouseInZoom = false;
	int username_flash = 0, username_flash_t = 1;
	Request *sessionCheck = NULL;
#if !defined(DEBUG) && !defined(_DEBUG)
	int signal_hooks = 0;
#endif
	bool disableSignals = false;

bool sendNewEvents = false;
bool openConsole = false;
bool openSign = false;
bool openProp = false;
PowderToy *the_game;
int main(int argc, char *argv[])
{
#ifdef WIN
	_setmode(0, _O_BINARY);
	_setmode(1, _O_BINARY);
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
#endif

	SDLInit();

	bool benchmark_enable = false;

#ifdef X86_SSE
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef X86_SSE3
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

	// initialize hud with defaults before loading powder.pref
	HudDefaults();
	memcpy(currentHud,normalHud,sizeof(currentHud));

	Platform::originalCwd = Platform::GetCwd();

	bool usedDdir = false;
	for (int i = 1; i < argc; i++)
	{
		if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			int ret = Platform::ChangeDir(argv[i+1]);
			if (ret)
				std::cout << "Error, could not change directory\n";
			else
				Platform::sharedCwd = argv[i + 1];
			i++;
			usedDdir = true;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			//Prevent reading of any arguments after ptsave for security
			i++;
			argc = i+2;
			break;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			int openDataSize;
			char *openData = (char*)file_load(argv[i+1], &openDataSize);
			saveDataOpen = new Save(openData, openDataSize);
			free(openData);
			i++;
		}
	}
#ifndef ANDROID
	if (!usedDdir)
	{
		char *ddir = SDL_GetPrefPath(NULL, "The Powder Toy");
#ifdef WIN
		struct _stat s;
		if (_stat("powder.pref", &s) != 0)
#else
		struct stat s;
		if (stat("powder.pref", &s) != 0)
#endif
		{
			if (ddir)
				Platform::ChangeDir(ddir);
		}

		if (ddir)
		{
			Platform::sharedCwd = ddir;
			SDL_free(ddir);
		}
	}
#endif

	// initialize this first so simulation gets inited
	the_game = new PowderToy(); // you just lost

	vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	part_vbuf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE); //Extra video buffer
	part_vbuf_store = part_vbuf;
	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);

#ifdef MT
	numCores = core_count();
#endif
	currentBrush = new Brush(Point(5, 5), CIRCLE_BRUSH);

#ifdef ANDROID
	{
		const char *home = getenv("HOME");
		if (home)
		{
			std::string fontPath = std::string(home) + "/fontcjk.ttf";
			if (!FT_Init(fontPath.c_str(), 16))
				printf("FT_Init failed for %s\n", fontPath.c_str());
		}
	}
#elif defined(LIN)
	if (!FT_Init("fontcjk.ttf", 16) && !FT_Init("/usr/share/fonts/windows10/simhei.ttf", 16))
		printf("FT_Init failed, Chinese text rendering disabled\n");
#else
	FT_Init("fontcjk.ttf", 16);
#endif

	// Load translations (TPT_LANG env var overrides default zh_CN)
	{
		const char *langEnv = getenv("TPT_LANG");
		std::string langFile = langEnv ? (std::string(langEnv) + ".json") : "zh_CN.json";
		if (Translations::Ref().LoadLanguageFile(langFile))
		{
			Translations::Ref().ApplyElementOverrides(globalSim);
			Translations::Ref().ApplyWallOverrides();
			Translations::Ref().ApplyMenuOverrides();
			// Rebuild tool objects so ElementTool picks up translated Name/Description
			FillMenus();
		}
	}

#ifdef LUACONSOLE
	luacon_open();
#endif

	TRON_init_graphics();
	init_color_boxes();

	timesplayed = timesplayed + 1;
	if (DEBUG_MODE)
		memcpy(currentHud,debugHud,sizeof(currentHud));
	else
		memcpy(currentHud,normalHud,sizeof(currentHud));
	memset(parts, 0, sizeof(particle)*NPART);
	clear_sim();

	bool disableNetwork = false, doRedirectStd = redirectStd;
	for (int i = 1; i < argc; i++)
	{
		if (!strncmp(argv[i], "scale:", 6))
		{
			unsigned int scale = (argv[i][6]=='2') ? 2 : 1;
			Engine::Ref().SetScale(scale);
		}
		else if (!strncmp(argv[i], "proxy:", 6))
		{
			memset(http_proxy_string, 0, sizeof(http_proxy_string));
			strncpy(http_proxy_string, argv[i]+6, 255);
		}
		else if (!strncmp(argv[i], "nohud", 5))
		{
			hud_enable = false;
		}
		else if (!strncmp(argv[i], "kiosk", 5))
		{
			Engine::Ref().SetFullscreen(1);
			hud_enable = false;
		}
		else if (!strncmp(argv[i], "scripts", 8))
		{
			file_script = 1;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			int ci = 0, ns = 0, okay = 0, newprocess = 1;
			char * tempString = argv[i+1];
			int tempStringLength = strlen(argv[i+1])-7;
			int tempSaveID = 0;
			char tempNumberString[32];
			puts("Got ptsave");
			i++;
			tempNumberString[31] = 0;
			tempNumberString[0] = 0;
			if (!strncmp(tempString, "noopen:", 7))
				newprocess = 0;
			if ((!strncmp(tempString, "ptsave:", 7) && tempStringLength) || !newprocess)
			{
				puts("ptsave:// protocol");
				tempString+=7;
				while(tempString[ci] && ns<30 && ci<tempStringLength)
				{
					if(tempString[ci]>=48 && tempString[ci]<=57)
					{
						tempNumberString[ns++] = tempString[ci];
						tempNumberString[ns] = 0;
					}
					else if(tempString[ci]=='#')
					{
						okay = 1;
						break;
					}
					else
					{
						puts("ptsave: invalid save ID");
						break;
					}
					ci++;
				}
				if(!tempString[ci])
				{
					okay = 1;
				}
				if(okay)
				{
					tempSaveID = atoi(tempNumberString);
				}
			}
			if(tempSaveID > 0)
			{
#ifdef WIN
				if (newprocess)
				{
					HWND modWindow = FindWindow(NULL, "Jacob1's Mod");
					if (modWindow)
					{
						//tell the already open window to open the save instead
						SendMessage(modWindow, WM_USER+614, (WPARAM)NULL, (LPARAM)tempSaveID);
						exit(0);
					}
				}
#endif
				puts("Got ptsave:id");
				saveURIOpen = tempSaveID;
				UpdateToolTip(Translations::Ref().GetIntroText(introText), Point(16, 20), INTROTIP, 0);
			}
			break;
		}
		else if (!strcmp(argv[i], "benchmark"))
		{
			benchmark_enable = true;
			if (i+1<argc)
			{
				benchmark_file = argv[i+1];
				i++;
			}
		}
		else if (!strcmp(argv[i], "disable-bluescreen"))
		{
			disableSignals = true;
		}
		else if (!strcmp(argv[i], "disable-network"))
		{
			disableNetwork = true;
		}
		else if (!strcmp(argv[i], "redirect"))
		{
			doRedirectStd = true;
		}
	}

#ifndef NOHTTP
	if (!disableNetwork)
		RequestManager::Ref().Initialise(http_proxy_string);
#endif
	if (doRedirectStd)
	{
		freopen("stdout.log", "w", stdout);
		freopen("stderr.log", "w", stderr);
	}

	if (!SDLOpen())
	{
		Engine::Ref().SetScale(1);
		Engine::Ref().SetFullscreen(false);
		if (!SDLOpen())
			exit(1);
	}

	StopTextInput();

#ifndef ANDROID
	if (firstRun && Engine::Ref().GetScale() == 1)
	{
		int scale = Engine::Ref().GuessBestScale();
		if (scale > 1)
		{
			Engine::Ref().SetScale(scale);
			showLargeScreenDialog = true;
		}
	}
#endif
	save_presets();

	prepare_alpha(1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);

#ifdef LUACONSOLE
	std::string autorun_result;
	luacon_opencompat();
	if (Platform::FileExists("autorun.lua") && luacon_eval("dofile(\"autorun.lua\")", &autorun_result)) //Autorun lua script
	{
		luacon_log(luacon_geterror());
	}
	if (!autorun_result.empty())
	{
		luacon_log(autorun_result);
	}

	// Check if script manager isn't running (was not in autorun.lua) and run our version
	if (luacon_eval("MANAGER == nil", &autorun_result))
	{
		luacon_log(luacon_geterror());
	}
	else if (autorun_result == "true")
	{
		luacon_openscriptmanager();
	}

	// TPTMP through the script manager, use that version instead
	if (!Platform::FileExists("scripts/downloaded/2 LBPHacker-TPTMulti.lua") &&
			!Platform::FileExists("scripts/downloaded/2 cracker64-TPTMulti.lua"))
		luacon_openmultiplayer();
#endif
	luacon_openstickmancontrol();
	for (int i = 0; i < 10; i++)
	{
		sprintf(tabNames[i], "Untitled Simulation %i", i+1);
		tabThumbnails[i] = NULL;
	}
	if (tab_load(1, true, false))
	{
		char fn[64];
		for (int i = 2; i <= 9; i++)
		{
			sprintf(fn, "tabs" PATH_SEP "%d.stm", i);
			if (Platform::FileExists(fn))
				num_tabs++;
			else
				break;
		}
	}

	if (benchmark_enable)
	{
		benchmark_run();
		exit(0);
	}

	Stamps::Ref().Init();

	UpdateToolTip(Translations::Ref().GetIntroText(introText), Point(16, 20), INTROTIP, 10235);

	Engine::Ref().ShowWindow(the_game);
#if !defined(DEBUG) && !defined(_DEBUG)
	try
	{
		MainLoop();
	}
	catch (std::exception& e)
	{
		BlueScreen(("Unhandled c++ exception: " + std::string(e.what())).c_str());
	}
#else
	MainLoop();
#endif

	return 0;
}

	//while (!sdl_poll()) //the main loop
int main_loop_temp(int b, int bq, int sdl_key, int scan, int x, int y, bool shift, bool ctrl, bool alt)
{
	if (true)
	{
		//change mouse position while it is in a zoom window
		//tmpMouseInZoom is used later, so that it only interrupts drawing, not things like copying / saving stamps
		Point cursor = the_game->AdjustCoordinates(Point(x, y));
		int mx = cursor.X;
		int my = cursor.Y;
		bool tmpMouseInZoom = false;
		if (x >= 0 && y >= 0 && x < XRES && y < YRES)
			tmpMouseInZoom = (x != mx || y != my);

		if (globalSim->grav->IsEnabled() && (Renderer::Ref().GetDisplayMode() & DISPLAY_WARP))
		{
			part_vbuf = part_vbuf_store;
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
		} else {
			part_vbuf = vid_buf;
		}

		render_before(part_vbuf, globalSim);
		globalSim->Tick(); //update everything
		render_after(part_vbuf, vid_buf, globalSim, Point(mx, my));

		// draw preview of stamp
		if (the_game->GetStampState() == PowderToy::LOAD && !the_game->PlacingZoomWindow())
		{
			Point loadPos = the_game->GetStampPos();
			Point loadSize = the_game->GetStampSize();
			pixel *img = the_game->GetStampImg();
			if (img)
			{
				draw_image(vid_buf, img, loadPos.X, loadPos.Y, loadSize.X, loadSize.Y, 128);
				xor_rect(vid_buf, loadPos.X, loadPos.Y, loadSize.X, loadSize.Y);
			}
		}
		// draw dotted lines for selection
		// other modes besides LOAD and NONE are SAVE, CUT, and COPY, which all select an area like this
		else if (the_game->GetStampState() != PowderToy::NONE && !the_game->PlacingZoomWindow())
		{
			if (the_game->GetStampState() != PowderToy::LOAD && the_game->PlacedInitialStampCoordinate())
			{
				Point savePos = the_game->GetSavePos();
				Point saveSize = the_game->GetSaveSize();

				if (saveSize.X < 0)
				{
					savePos.X = savePos.X + saveSize.X - 1;
					saveSize.X = abs(saveSize.X) + 2;
				}
				if (saveSize.Y < 0)
				{
					savePos.Y = savePos.Y + saveSize.Y - 1;
					saveSize.Y = abs(saveSize.Y) + 2;
				}
				// dim the area not inside the selected area
				fillrect(vid_buf, -1, -1, savePos.X+1, YRES+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X-1, -1, saveSize.X+1, savePos.Y+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X-1, savePos.Y+saveSize.Y-1, saveSize.X+1, YRES-savePos.Y-saveSize.Y+1, 0, 0, 0, 100);
				fillrect(vid_buf, savePos.X+saveSize.X-1, -1, XRES-savePos.X-saveSize.X+1, YRES+1, 0, 0, 0, 100);
				// dotted rectangle
				xor_rect(vid_buf, savePos.X, savePos.Y, saveSize.X, saveSize.Y);
			}
			else
				fillrect(vid_buf, -1, -1, XRES+1, YRES+1, 0, 0, 0, 100);
		}

		// draw normal cursor
		if (!the_game->MouseClicksIgnored() && the_game->GetBrushEnable())
		{
			PowderToy::DrawState drawState = the_game->GetDrawState();
			Point initialDrawPoint = the_game->GetInitialDrawPoint();
			Point snappedCursor = the_game->AdjustCoordinates(Point(x, y));
			bool isMouseDown = the_game->IsMouseDown();
			if (alt)
			{
				if (drawState == PowderToy::LINE)
					snappedCursor = the_game->LineSnapCoords(initialDrawPoint, snappedCursor);
				else if (drawState == PowderToy::RECT)
					snappedCursor = the_game->RectSnapCoords(initialDrawPoint, snappedCursor);
			}
			initialDrawPoint = the_game->SnapCoordinatesWall(initialDrawPoint, snappedCursor);
			snappedCursor = the_game->SnapCoordinatesWall(snappedCursor, initialDrawPoint);
			if (drawState != PowderToy::RECT || !isMouseDown)
			{
				if (isMouseDown && alt)
					render_cursor(vid_buf, snappedCursor.X, snappedCursor.Y, activeTools[the_game->GetToolIndex()], currentBrush);
				else if ((x < XRES && y < YRES) || isMouseDown)
					render_cursor(vid_buf, mx, my, activeTools[the_game->GetToolIndex()], currentBrush);
			}

			if (isMouseDown)
			{
				if (drawState == PowderToy::LINE)
					xor_line(initialDrawPoint.X, initialDrawPoint.Y, snappedCursor.X, snappedCursor.Y, vid_buf);
				else if (drawState == PowderToy::RECT)
				{
					int width = snappedCursor.X-initialDrawPoint.X;
					int height = snappedCursor.Y-initialDrawPoint.Y;
					if (width < 0)
					{
						initialDrawPoint.X += width;
						width *= -1;
					}
					if (height < 0)
					{
						initialDrawPoint.Y += height;
						height *= -1;
					}
					xor_line(initialDrawPoint.X, initialDrawPoint.Y, initialDrawPoint.X+width, initialDrawPoint.Y, vid_buf);
					if (height > 0)
					{
						xor_line(initialDrawPoint.X, initialDrawPoint.Y+height, initialDrawPoint.X+width, initialDrawPoint.Y+height, vid_buf);
						if (height > 1)
						{
							xor_line(initialDrawPoint.X, initialDrawPoint.Y+1, initialDrawPoint.X, initialDrawPoint.Y+height-1, vid_buf);
							if (width > 0)
								xor_line(initialDrawPoint.X+width, initialDrawPoint.Y+1, initialDrawPoint.X+width, initialDrawPoint.Y+height-1, vid_buf);
						}
					}
				}
			}
		}
#ifdef LUACONSOLE
		auto ev = AfterSimDrawEvent();
		HandleEvent(LuaEvents::aftersimdraw, &ev);
#endif
		if (the_game->ZoomWindowShown())
			render_zoom(vid_buf);

		memset(vid_buf+((XRES+BARSIZE)*YRES), 0, (PIXELSIZE*(XRES+BARSIZE))*MENUSIZE);//clear menu areas
		clearrect(vid_buf, XRES, 1, BARSIZE, YRES-1);

		DrawToolTips();
		
		if(debug_flags)
		{
			PowderToy::DrawState drawState = the_game->GetDrawState();
			Point initialDrawPoint = the_game->AdjustCoordinates(the_game->GetInitialDrawPoint());
			Point snappedCursor = the_game->AdjustCoordinates(Point(x, y));
			if (alt)
			{
				if (drawState == PowderToy::LINE)
					snappedCursor = the_game->LineSnapCoords(initialDrawPoint, snappedCursor);
				else if (drawState == PowderToy::RECT)
					snappedCursor = the_game->RectSnapCoords(initialDrawPoint, snappedCursor);

			}
			draw_debug_info(vid_buf, globalSim, initialDrawPoint.X, initialDrawPoint.Y, x, y, snappedCursor.X, snappedCursor.Y);
		}

		if (saveDataOpen)
		{
			//Clear all settings and simulation data
			NewSim();
			UpdateToolTip(Translations::Ref().GetIntroText(introText), Point(16, 20), INTROTIP, 0);

			try
			{
				auto saveLoadData = globalSim->LoadSave(0, 0, saveDataOpen, 1);
				authors = saveLoadData.authors;
				the_game->SetReloadPoint(saveDataOpen);
			}
			catch (ParseException & e)
			{
				Engine::Ref().ShowWindow(new ErrorPrompt(TrUI("Unable to open save file: ") + std::string(e.what())));
			}
			delete saveDataOpen;
			saveDataOpen = NULL;
		}

		if(saveURIOpen)
		{
			char saveURIOpenString[512];
			sprintf(saveURIOpenString, "%d", saveURIOpen);
			open_ui(vid_buf, saveURIOpenString, NULL, 0);
			saveURIOpen = 0;
		}
		if (!ptsaveOpenID)
			ptsaveOpenID = Platform::CheckLoadedPtsave();
		if (ptsaveOpenID)
		{
			if (num_tabs < 22-GetNumMenus())
			{
				char ptsaveOpenString[512];
				int oldTabNum = tab_num;
				tab_save(tab_num);
				num_tabs++;
				tab_num = num_tabs;
				NewSim();
				sprintf(ptsaveOpenString, "%d", ptsaveOpenID);

				//if they didn't open it, don't make the new tab
				if (!open_ui(vid_buf, ptsaveOpenString, NULL, 0))
				{
					num_tabs--;
					tab_num = oldTabNum;
					tab_load(oldTabNum);
				}
				tab_save(tab_num);
			}
			ptsaveOpenID = 0;
		}

#ifdef TOUCHUI
		int hover = DrawMenusTouch(vid_buf, b, bq, x, y);
		if (hover >= 0)
		{
			if (y > 150)
				UpdateToolTip(menuSections[hover]->name, Point(menuStartPosition-5-textwidth(menuSections[hover]->name.c_str()), YRES-67), QTIP, -1);
			if (hover == SC_DECO && active_menu != SC_DECO)
				last_active_menu = active_menu;
			if (hover == SC_FAV)
			{
				if (last_fav_menu != active_menu)
				{
					active_menu = last_fav_menu;
					Platform::Vibrate(1);
				}
			}
			else
			{
				if (hover != active_menu)
				{
					active_menu = hover;
					Platform::Vibrate(1);
				}
			}
		}
		menu_ui_v3(vid_buf, active_menu, b, bq, x, y); //draw the elements in the current menu
#else
		if (!old_menu)
		{
			QuickoptionsMenu(vid_buf, b, bq, x, y);

			int hover = DrawMenus(vid_buf, active_menu, y);
			if (hover >= 0 && x>=menuStartPosition && x<XRES+BARSIZE-1)
			{
				UpdateToolTip(menuSections[hover]->name, Point(menuStartPosition-5-textwidth(menuSections[hover]->name.c_str()), std::min(((y-8)/16)*16+12, YRES-9)), QTIP, -1);
				bool clickRequired = menuSections[hover]->click || stickyCategories;
				if ((!clickRequired && !b) || (clickRequired && b && !bq))
				{
					if (menuSections[hover]->click && !menuSections[active_menu]->click)
						last_active_menu = active_menu;
					if (hover == SC_FAV)
						active_menu = last_fav_menu;
					else
						active_menu = hover;

					if (hover == SC_SEARCH)
					{
						element_search_ui(vid_buf, &activeTools[0], &activeTools[1], &activeTools[2]);
						active_menu = last_active_menu;
					}
				}
			}
			menu_ui_v3(vid_buf, active_menu, b, bq, x, y); //draw the elements in the current menu
		}
		else
			old_menu_v2(active_menu, x, y, b, bq);

		if (active_menu == SC_DECO)
			the_game->SetLegacyCodeDoesTextInput(true);
		else
			the_game->SetLegacyCodeDoesTextInput(false);
#endif

		//TODO: only when entering / exiting menu
		if (active_menu == SC_DECO && activeTools[0]->GetType() != DECO_TOOL)
		{
			the_game->SwapToDecoToolset();
		}
		else if (active_menu != SC_DECO && activeTools[0]->GetType() == DECO_TOOL)
		{
			the_game->SwapToRegularToolset();
			deco_disablestuff = 0;
		}
		if (deco_disablestuff)
			b = 0;

		if (update_flag)
		{
			info_box(vid_buf, "Finalizing update...");
			if (update_finish())
				Engine::Ref().ShowWindow(new ErrorPrompt(TrUI("Update failed - try downloading a new version.")));
			else
				Engine::Ref().ShowWindow(new InfoPrompt(TrUI("Update success"), TrUI("You have successfully updated Jacob1's Mod!")));
			update_flag = 0;
		}

		if (mx < XRES && my < YRES)
		{
			int signID = InsideSign(globalSim, mx, my, false);
			if (signID != -1 && signs[signID].GetType() != Sign::Normal && signs[signID].GetType() != Sign::Spark)
			{
				std::stringstream tooltip;
				switch (signs[signID].GetType())
				{
				case Sign::SaveLink:
					tooltip << "Go to save ID:" << signs[signID].GetLinkText();
					break;
				case Sign::ThreadLink:
					tooltip << "Open forum thread " << signs[signID].GetLinkText() << " in browser";
					break;
				case Sign::SearchLink:
					tooltip << "Search for " << signs[signID].GetLinkText();
					break;
				default:
					break;
				}
				UpdateToolTip(tooltip.str(), Point(16, YRES-24), TOOLTIP, -1);
			}
		}

		//mouse clicks ignored when placing zoom, or when loading / saving stamps
		if (the_game->MouseClicksIgnored())
		{
			// certain thing the main window handles need prevent stuff from being drawn to the screen
		}
		//there is a click
		else if (b)
		{
			UpdateToolTip(Translations::Ref().GetIntroText(introText), Point(16, 20), INTROTIP, 255);

			/*if (tmpMouseInZoom != mouseInZoom && !lm)
				b = lb = 0;
			else if ((y<YRES && x<XRES) || lb)// mouse is in playing field
			{
				activeToolID = ((b&1) || b == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				if (activeTools[0]->GetType() == DECO_TOOL && b == 4)
					activeTool = GetToolFromIdentifier("DEFAULT_DECOR_CLR");

				//for the click functions, lx and ly, are the positions of where the FIRST click happened.  mx,my are current mouse position.
				if (lb) //lb means you are holding mouse down
				{
					toolStrength = 1.0f;
					if (lm == 1)//line tool
					{
						if (sdl_mod & KMOD_ALT)
						{
							Point point1 = Point(lx, ly), point2 = Point(mx, my);
							Point diff = point2 - point1, line;
							if (abs(diff.X / 2) > abs(diff.Y)) // vertical
								line = point1 + Point(diff.X, 0);
							else if(abs(diff.X) < abs(diff.Y / 2)) // horizontal
								line = point1 + Point(0, diff.Y);
							else if(diff.X * diff.Y > 0) // NW-SE
								line = point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
							else // SW-NE
								line = point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
							line_x = line.X;
							line_y = line.Y;
						}
						else
						{
							line_x = mx;
							line_y = my;
						}
						//WIND tool works before you even draw the line while holding shift
						if (((ToolTool*)activeTool)->GetID() == TOOL_WIND)
						{
							for (int j = -currentBrush->GetRadius().Y; j <= currentBrush->GetRadius().Y; j++)
								for (int i = -currentBrush->GetRadius().X; i <= currentBrush->GetRadius().X; i++)
									if (lx+i>0 && ly+j>0 && lx+i<XRES && ly+j<YRES && currentBrush->IsInside(i, j))
									{
										vx[(ly+j)/CELL][(lx+i)/CELL] += (line_x-lx)*0.002f;
										vy[(ly+j)/CELL][(lx+i)/CELL] += (line_y-ly)*0.002f;
									}
						}
					}
					else if (lm == 2)//box tool
					{
						if (sdl_mod & KMOD_ALT)
						{
							Point point1 = Point(lx, ly), point2 = Point(mx, my);
							Point diff = point2 - point1, line;
							if (diff.X * diff.Y > 0) // NW-SE
								line = point1 + Point((diff.X + diff.Y)/2, (diff.X + diff.Y)/2);
							else // SW-NE
								line = point1 + Point((diff.X - diff.Y)/2, (diff.Y - diff.X)/2);
							line_x = line.X;
							line_y = line.Y;
						}
						else
						{
							line_x = mx;
							line_y = my;
						}
					}
					//flood fill
					else if (lm == 3)
					{
						if (!((sdl_mod&(KMOD_CTRL|KMOD_GUI)) && (sdl_mod&KMOD_SHIFT)))
							lb = 0;
						else
							activeTool->FloodFill(globalSim, currentBrush, Point(mx, my));
					}
					//while mouse is held down, this draws lines between previous and current positions
					else if (!lm)
					{
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & (KMOD_CTRL|KMOD_GUI))
							toolStrength = .1f;
						activeTool->DrawLine(globalSim, currentBrush, Point(lx, ly), Point(mx, my), true, toolStrength);
						lx = mx;
						ly = my;
					}
				}
				//it is the first click (don't start drawing if we are moving a sign though)
				else if (!bq && MSIGN == -1 && InsideSign(mx, my, sdl_mod&KMOD_CTRL) == -1)
				{
					toolStrength = 1.0f;
					lx = line_x = mx;
					ly = line_y = my;
					lb = b;
					lm = 0;
					//start line tool
					if ((sdl_mod & (KMOD_SHIFT)) && !(sdl_mod & (KMOD_CTRL|KMOD_GUI)))
					{
						lm = 1;//line
					}
					//start box tool
					else if ((sdl_mod & (KMOD_CTRL|KMOD_GUI)) && !(sdl_mod & (KMOD_SHIFT)))
					{
						lm = 2;//box
					}
					//flood fill
					else if ((sdl_mod & (KMOD_CTRL|KMOD_GUI)) && (sdl_mod & (KMOD_SHIFT)) && (((ToolTool*)activeTool)->GetID() == -1 || ((ToolTool*)activeTool)->GetID() == TOOL_PROP))
					{
						ctrlzSnapshot();
						activeTool->FloodFill(globalSim, currentBrush, Point(mx, my));
						lm = 3;
					}
					//sample
					else if (((sdl_mod & (KMOD_ALT)) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_GUI))) || b==SDL_BUTTON_MIDDLE)
					{
						activeTools[activeToolID] = activeTool->Sample(globalSim, Point(mx, my));
						lb = 0;
					}
					else //normal click, spawn element
					{
						//Copy state before drawing any particles (for undo)
						ctrlzSnapshot();

						//get tool strength, shift (or ctrl+shift) makes it faster and ctrl makes it slower
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & (KMOD_CTRL|KMOD_GUI))
							toolStrength = .1f;

						activeTool->DrawPoint(globalSim, currentBrush, Point(mx, my), toolStrength);
					}
				}
			}*/
		}
		else
		{
			/*if (lb)
			{
				activeToolID = ((lb&1) || lb == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				//lm is box/line tool (3 is floodfill)
				if (lm)
				{
					if (lm != 3)
					{
						ctrlzSnapshot();
						if (activeTools[0]->GetType() == DECO_TOOL && lb == 4)
							activeTool = GetToolFromIdentifier("DEFAULT_DECOR_CLR");

						if (lm == 1)
							activeTool->DrawLine(globalSim, currentBrush, Point(lx, ly), Point(line_x, line_y), false, 1.0f);
						else if (lm == 2)
							activeTool->DrawRect(globalSim, currentBrush, Point(lx, ly), Point(line_x, line_y));
						lm = 0;
					}
				}
				else
				{
					//plop tool (STKM, STKM2, FIGH)
					activeTool->Click(globalSim, Point(lx, ly));
				}
			}
			else if (bq)
			{

			}
			lb = 0;*/
		}
		mouseInZoom = tmpMouseInZoom;

		if (elapsedTime != currentTime)
		{
			frames = frames + 1;
			totalfps = totalfps + FPSB2;
		}
		if (lastx == x && lasty == y)
		{
			if (!afk)
			{
				afk = 1;
				afkstart = currentTime;
			}
		}
		else if (afk)
		{
			afk = 0;
			totalafktime = totalafktime + afktime;
			afktime = 0;
		}
		if (afk && currentTime - afkstart > 30000)
			afktime = currentTime - afkstart - 30000;
		lastx = x;
		lasty = y;

		if (autosave > 0 && frames%autosave == 0)
		{
			tab_save(1);
		}
		if (hud_enable)
		{
			SetLeftHudText(globalSim, FPSB2);
			SetRightHudText(globalSim, x, y);

			DrawHud(GetToolTipAlpha(INTROTIP), GetToolTipAlpha(QTIP));

			if (drawinfo)
				DrawRecordsInfo(globalSim);

			DrawLuaLogs();
		}

		//sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);

#if !defined(DEBUG) && !defined(_DEBUG)
		if (!signal_hooks && !disableSignals)
		{
			signal(SIGSEGV, SigHandler);
			signal(SIGFPE, SigHandler);
			signal(SIGILL, SigHandler);
			signal(SIGABRT, SigHandler);
			signal_hooks = 1;
		}
#endif
	}
	return true;
}

void main_end_hack()
{
	SaveWindowPosition();
	save_presets();
#ifdef LUACONSOLE
	luacon_close();
#endif
	FT_Shutdown();
#ifndef NOHTTP
	RequestManager::Ref().Shutdown();
#endif
	ClearMenusections();
	ClearSigns();
	delete currentBrush;
	for (int i = toolTips.size()-1; i >= 0; i--)
		delete toolTips[i];
	toolTips.clear();
	if (part_vbuf_store)
		free(part_vbuf_store);
	for (int i = 1; i < 10; i++)
	{
		char name[30] = {0};
		sprintf(name,"tabs%s%d.stm",PATH_SEP,i);
		remove(name);
	}
	Platform::DeleteDirectory("tabs");
}
#endif
