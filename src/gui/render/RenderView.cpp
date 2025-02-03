#include "RenderView.h"
#include "simulation/ElementGraphics.h"
#include "simulation/SimulationData.h"
#include "simulation/Simulation.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "RenderController.h"
#include "RenderModel.h"
#include "gui/interface/Checkbox.h"
#include "gui/interface/Button.h"
#include "gui/game/GameController.h"
#include "gui/game/GameView.h"

class ModeCheckbox : public ui::Checkbox
{
public:
	using ui::Checkbox::Checkbox;
	uint32_t mode;
};

RenderView::RenderView():
	ui::Window(ui::Point(0, 0), ui::Point(XRES, WINDOWH)),
	ren(nullptr),
	toolTip(""),
	isToolTipFadingIn(false)
{
	auto addPresetButton = [this](int index, Icon icon, ui::Point offset, String tooltip)
	{
		auto *presetButton = new ui::Button(ui::Point(XRES, YRES) + offset, ui::Point(30, 13), "", tooltip);
		presetButton->SetIcon(icon);
		presetButton->SetActionCallback({[this, index]
										 { c->LoadRenderPreset(index); }});
		AddComponent(presetButton);
	};
	addPresetButton(1, IconVelocity, ui::Point(-37, 6), ByteString("\u6c14\u6d41\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(2, IconPressure, ui::Point(-37, 24), ByteString("\u6c14\u538b\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(3, IconPersistant, ui::Point(-76, 6), ByteString("\u8f68\u8ff9\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(4, IconFire, ui::Point(-76, 24), ByteString("\u6807\u51c6\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(5, IconBlob, ui::Point(-115, 6), ByteString("\u6a21\u7cca\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(6, IconHeat, ui::Point(-115, 24), ByteString("\u6e29\u5ea6\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(7, IconBlur, ui::Point(-154, 6), ByteString("\u7279\u6548\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(8, IconBasic, ui::Point(-154, 24), ByteString("\u65e0\u7279\u6548\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(9, IconGradient, ui::Point(-193, 6), ByteString("\u70ed\u4f20\u5bfc\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(0, IconAltAir, ui::Point(-193, 24), ByteString("\u901f\u538b\u6df7\u5408\u663e\u793a\u6a21\u5f0f").FromUtf8());
	addPresetButton(10, IconLife, ui::Point(-232, 6), ByteString("Life\u663e\u793a\u6a21\u5f0f").FromUtf8());

	auto addRenderModeCheckbox = [this](unsigned int mode, Icon icon, ui::Point offset, String tooltip)
	{
		auto *renderModeCheckbox = new ModeCheckbox(ui::Point(0, YRES) + offset, ui::Point(30, 16), "", tooltip);
		renderModes.push_back(renderModeCheckbox);
		renderModeCheckbox->mode = mode;
		renderModeCheckbox->SetIcon(icon);
		renderModeCheckbox->SetActionCallback({ [this] {
			auto renderMode = CalculateRenderMode();
			c->SetRenderMode(renderMode);
		} });
		AddComponent(renderModeCheckbox);
	};
	addRenderModeCheckbox(RENDER_EFFE, IconEffect, ui::Point(1, 4), ByteString("\u5143\u7d20\u7279\u6b8a\u95ea\u5149\u6548\u679c").FromUtf8());
	addRenderModeCheckbox(RENDER_FIRE, IconFire, ui::Point(1, 22), ByteString("\u6c14\u4f53\u706b\u7130\u89c6\u89c9\u6548\u679c").FromUtf8());
	addRenderModeCheckbox(RENDER_GLOW, IconGlow, ui::Point(33, 4), ByteString("\u5143\u7d20\u53d1\u5149\u6548\u679c").FromUtf8());
	addRenderModeCheckbox(RENDER_BLUR, IconBlur, ui::Point(33, 22), ByteString("\u6db2\u4f53\u6a21\u7cca\u6548\u679c").FromUtf8());
	addRenderModeCheckbox(RENDER_BLOB, IconBlob, ui::Point(65, 4), ByteString("\u6a21\u7cca\u663e\u793a").FromUtf8());
	addRenderModeCheckbox(RENDER_BASC, IconBasic, ui::Point(65, 22), ByteString("\u57fa\u672c\u6e32\u67d3\u002e\u5173\u95ed\u540e\u4f1a\u4f7f\u7edd\u5927\u90e8\u5206\u5143\u7d20\u4e0d\u53ef\u89c1").FromUtf8());
	addRenderModeCheckbox(RENDER_SPRK, IconEffect, ui::Point(97, 4), ByteString("\u7535\u8109\u51b2\u53d1\u5149\u89c6\u89c9\u6548\u679c").FromUtf8());

	auto addDisplayModeCheckbox = [this](unsigned int mode, Icon icon, ui::Point offset, String tooltip)
	{
		auto *displayModeCheckbox = new ModeCheckbox(ui::Point(0, YRES) + offset, ui::Point(30, 16), "", tooltip);
		displayModes.push_back(displayModeCheckbox);
		displayModeCheckbox->mode = mode;
		displayModeCheckbox->SetIcon(icon);
		displayModeCheckbox->SetActionCallback({ [this, displayModeCheckbox] {
			auto displayMode = c->GetDisplayMode();
			// Air display modes are mutually exclusive
			if (displayModeCheckbox->mode & DISPLAY_AIR)
			{
				displayMode &= ~DISPLAY_AIR;
			}
			if (displayModeCheckbox->GetChecked())
			{
				displayMode |= displayModeCheckbox->mode;
			}
			else
			{
				displayMode &= ~displayModeCheckbox->mode;
			}
			c->SetDisplayMode(displayMode);
		} });
		AddComponent(displayModeCheckbox);
	};
	line1 = 130;
	addDisplayModeCheckbox(DISPLAY_AIRC, IconAltAir, ui::Point(135, 4), ByteString("\u663e\u793a\u538b\u529b\u4e3a\u7ea2\u8272\u548c\u84dd\u8272\u002c\u901f\u5ea6\u4e3a\u767d\u8272").FromUtf8());
	addDisplayModeCheckbox(DISPLAY_AIRP, IconPressure, ui::Point(135, 22), ByteString("\u663e\u793a\u538b\u529b\u002c\u7ea2\u8272\u4e3a\u6b63\u002c\u84dd\u8272\u4e3a\u8d1f").FromUtf8());
	addDisplayModeCheckbox(DISPLAY_AIRV, IconVelocity, ui::Point(167, 4), ByteString("\u663e\u793a\u6c14\u6d41\u7684\u901f\u5ea6\u4e0e\u5f3a\u5ea6\u002c\u84dd\u8272\u4ee3\u8868\u7ad6\u76f4\u901f\u5ea6\u002c\u7ea2\u8272\u4ee3\u8868\u6c34\u5e73\u901f\u5ea6\u002e\u6b63\u538b\u529b\u7528\u7eff\u8272\u8868\u793a\u002c\u8d1f\u538b\u529b\u4e0d\u663e").FromUtf8());
	addDisplayModeCheckbox(DISPLAY_AIRH, IconHeat, ui::Point(167, 22), ByteString("\u663e\u793a\u7a7a\u6c14\u6e29\u5ea6").FromUtf8());
	line2 = 200;
	addDisplayModeCheckbox(DISPLAY_WARP, IconWarp, ui::Point(205, 22), ByteString("\u5f15\u529b\u900f\u955c\u002c\u725b\u987f\u5f15\u529b\u53ef\u4f7f\u5149\u7ebf\u5f2f\u66f2").FromUtf8());
	addDisplayModeCheckbox(DISPLAY_EFFE, IconEffect, ui::Point(205, 4), ByteString("\u5141\u8bb8\u56fa\u4f53\u79fb\u52a8\u663e\u793a\u002c\u548c\u9ad8\u7ea7\u56fe\u5f62").FromUtf8());
	addDisplayModeCheckbox(DISPLAY_PERS, IconPersistant, ui::Point(237, 4), ByteString("\u663e\u793a\u7269\u8d28\u79fb\u52a8\u8f68\u8ff9").FromUtf8());
	line3 = 270;

	auto addColourModeCheckbox = [this](unsigned int mode, Icon icon, ui::Point offset, String tooltip)
	{
		auto *colourModeCheckbox = new ModeCheckbox(ui::Point(0, YRES) + offset, ui::Point(30, 16), "", tooltip);
		colourModes.push_back(colourModeCheckbox);
		colourModeCheckbox->mode = mode;
		colourModeCheckbox->SetIcon(icon);
		colourModeCheckbox->SetActionCallback({ [this, colourModeCheckbox] {
			auto colorMode = c->GetColorMode();
			// exception: looks like an independent set of settings but behaves more like an index
			if (colourModeCheckbox->GetChecked())
			{
				colorMode = colourModeCheckbox->mode;
			}
			else
			{
				colorMode = 0;
			}
			c->SetColorMode(colorMode);
		} });
		AddComponent(colourModeCheckbox);
	};
	addColourModeCheckbox(COLOUR_HEAT, IconHeat, ui::Point(275, 4), ByteString("\u663e\u793a\u7269\u8d28\u6e29\u5ea6\u002c\u4f4e\u6e29\u6df1\u84dd\u8272\u002c\u9ad8\u6e29\u7c89\u7ea2\u8272").FromUtf8());
	addColourModeCheckbox(COLOUR_LIFE, IconLife, ui::Point(275, 22), ByteString("\u7070\u5ea6\u663e\u793a\u7269\u8d28\u006c\u0069\u0066\u0065\u503c\u5927\u5c0f").FromUtf8());
	addColourModeCheckbox(COLOUR_GRAD, IconGradient, ui::Point(307, 22), ByteString("\u8f7b\u5fae\u6539\u53d8\u5143\u7d20\u7684\u989c\u8272\u002c\u663e\u793a\u70ed\u91cf\u6269\u6563\u6548\u679c").FromUtf8());
	addColourModeCheckbox(COLOUR_BASC, IconBasic, ui::Point(307, 4), ByteString("\u6ca1\u6709\u4efb\u4f55\u7279\u6b8a\u6548\u679c\u002c\u4ec5\u505a\u8986\u76d6\u663e\u793a").FromUtf8());
	line4 = 340;
}

uint32_t RenderView::CalculateRenderMode()
{
	uint32_t renderMode = 0;
	for (auto &checkbox : renderModes)
	{
		if (checkbox->GetChecked())
			renderMode |= checkbox->mode;
	}

	return renderMode;
}

void RenderView::OnMouseDown(int x, int y, unsigned button)
{
	if (x > XRES || y < YRES)
		c->Exit();
}

void RenderView::OnTryExit(ExitMethod method)
{
	c->Exit();
}

void RenderView::NotifyRendererChanged(RenderModel *sender)
{
	ren = sender->GetRenderer();
	rendererSettings = sender->GetRendererSettings();
}

void RenderView::NotifySimulationChanged(RenderModel * sender)
{
	sim = sender->GetSimulation();
}

void RenderView::NotifyRenderChanged(RenderModel *sender)
{
	for (size_t i = 0; i < renderModes.size(); i++)
	{
		// Compares bitmasks at the moment, this means that "Point" is always on when other options that depend on it are, this might confuse some users, TODO: get the full list and compare that?
		auto renderMode = renderModes[i]->mode;
		renderModes[i]->SetChecked(renderMode == (sender->GetRenderMode() & renderMode));
	}
}

void RenderView::NotifyDisplayChanged(RenderModel *sender)
{
	for (size_t i = 0; i < displayModes.size(); i++)
	{
		auto displayMode = displayModes[i]->mode;
		displayModes[i]->SetChecked(displayMode == (sender->GetDisplayMode() & displayMode));
	}
}

void RenderView::NotifyColourChanged(RenderModel *sender)
{
	for (size_t i = 0; i < colourModes.size(); i++)
	{
		auto colorMode = colourModes[i]->mode;
		colourModes[i]->SetChecked(colorMode == sender->GetColorMode());
	}
}

void RenderView::OnDraw()
{
	Graphics * g = GetGraphics();
	g->DrawFilledRect(WINDOW.OriginRect(), 0x000000_rgb);
	auto *view = GameController::Ref().GetView();
	view->PauseRendererThread();
	ren->ApplySettings(*rendererSettings);
	view->RenderSimulation(*sim, true);
	view->AfterSimDraw(*sim);
	for (auto y = 0; y < YRES; ++y)
	{
		auto &video = ren->GetVideo();
		std::copy_n(video.data() + video.Size().X * y, video.Size().X, g->Data() + g->Size().X * y);
	}
	g->DrawLine({ 0, YRES }, { XRES-1, YRES }, 0xC8C8C8_rgb);
	g->DrawLine({ line1, YRES }, { line1, WINDOWH }, 0xC8C8C8_rgb);
	g->DrawLine({ line2, YRES }, { line2, WINDOWH }, 0xC8C8C8_rgb);
	g->DrawLine({ line3, YRES }, { line3, WINDOWH }, 0xC8C8C8_rgb);
	g->DrawLine({ line4, YRES }, { line4, WINDOWH }, 0xC8C8C8_rgb);
	g->DrawLine({ XRES, 0 }, { XRES, WINDOWH }, 0xFFFFFF_rgb);
	if(toolTipPresence && toolTip.length())
	{
		g->BlendText({ 6, Size.Y-MENUSIZE-12 }, toolTip, 0xFFFFFF_rgb .WithAlpha(toolTipPresence>51?255:toolTipPresence*5));
	}
}

void RenderView::OnTick()
{
	if (isToolTipFadingIn)
	{
		isToolTipFadingIn = false;
		toolTipPresence.SetTarget(120);
	}
	else
	{
		toolTipPresence.SetTarget(0);
	}
}

void RenderView::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (repeat)
		return;
	if (shift && key == '1')
		c->LoadRenderPreset(10);
	else if (key >= '0' && key <= '9')
	{
		c->LoadRenderPreset(key - '0');
	}
}

void RenderView::ToolTip(ui::Point senderPosition, String toolTip)
{
	this->toolTip = toolTip;
	this->isToolTipFadingIn = true;
}

RenderView::~RenderView()
{
}
