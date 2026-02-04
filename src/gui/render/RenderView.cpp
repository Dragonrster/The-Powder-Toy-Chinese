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
    addPresetButton(1, IconVelocity,   ui::Point(-37, 6),  ByteString("气流渲染模式").FromUtf8());
    addPresetButton(2, IconPressure,   ui::Point(-37, 24), ByteString("气压渲染模式").FromUtf8());
    addPresetButton(3, IconPersistant, ui::Point(-76, 6),  ByteString("轨迹渲染模式").FromUtf8());
    addPresetButton(4, IconFire,       ui::Point(-76, 24), ByteString("标准渲染模式").FromUtf8());
    addPresetButton(5, IconBlob,       ui::Point(-115, 6), ByteString("模糊渲染模式").FromUtf8());
    addPresetButton(6, IconHeat,       ui::Point(-115, 24),ByteString("温度渲染模式").FromUtf8());
    addPresetButton(7, IconBlur,       ui::Point(-154, 6), ByteString("特效渲染模式").FromUtf8());
    addPresetButton(8, IconBasic,      ui::Point(-154, 24),ByteString("无特效渲染模式").FromUtf8());
    addPresetButton(9, IconGradient,   ui::Point(-193, 6), ByteString("热传导渲染模式").FromUtf8());
    addPresetButton(0, IconAltAir,     ui::Point(-193, 24),ByteString("速压混合渲染模式").FromUtf8());
    addPresetButton(10,IconLife,       ui::Point(-232, 6), ByteString("Life渲染模式").FromUtf8());
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
    addRenderModeCheckbox(RENDER_EFFE, IconEffect, ui::Point(1, 4),   ByteString("元素特殊闪光效果").FromUtf8());
    addRenderModeCheckbox(RENDER_FIRE, IconFire,   ui::Point(1, 22),  ByteString("气体火焰视觉效果").FromUtf8());
    addRenderModeCheckbox(RENDER_GLOW, IconGlow,   ui::Point(33, 4),  ByteString("元素发光效果").FromUtf8());
    addRenderModeCheckbox(RENDER_BLUR, IconBlur,   ui::Point(33, 22), ByteString("液体模糊效果").FromUtf8());
    addRenderModeCheckbox(RENDER_BLOB, IconBlob,   ui::Point(65, 4),  ByteString("模糊渲染").FromUtf8());
    addRenderModeCheckbox(RENDER_BASC, IconBasic,  ui::Point(65, 22), ByteString("基本渲染。关闭后会使绝大部分元素不可见").FromUtf8());
    addRenderModeCheckbox(RENDER_SPRK, IconEffect, ui::Point(97, 4),  ByteString("电脉冲发光视觉效果").FromUtf8());

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
    addDisplayModeCheckbox(DISPLAY_AIRC, IconAltAir, ui::Point(135, 4),   ByteString("渲染压力为红色和蓝色，速度为白色").FromUtf8());
    addDisplayModeCheckbox(DISPLAY_AIRP, IconPressure, ui::Point(135, 22), ByteString("渲染压力，红色为正，蓝色为负").FromUtf8());
    addDisplayModeCheckbox(DISPLAY_AIRV, IconVelocity, ui::Point(167, 4), ByteString("渲染气流的速度与强度，蓝色代表竖直速度，红色代表水平速度。正压力用绿色表示，负压力不渲染").FromUtf8());
    addDisplayModeCheckbox(DISPLAY_AIRH, IconHeat, ui::Point(167, 22),    ByteString("渲染空气温度").FromUtf8());
    line2 = 200;
    addDisplayModeCheckbox(DISPLAY_WARP, IconWarp, ui::Point(205, 22),    ByteString("引力透镜，牛顿引力可使光线弯曲").FromUtf8());
    addDisplayModeCheckbox(DISPLAY_EFFE, IconEffect, ui::Point(205, 4),   ByteString("允许固体移动渲染，和高级图形").FromUtf8());
    addDisplayModeCheckbox(DISPLAY_PERS, IconPersistant, ui::Point(237, 4),ByteString("渲染物质移动轨迹").FromUtf8());
    line3 = 270;
	// addDisplayModeCheckbox(DISPLAY_AIRC, IconAltAir    , ui::Point(135,  4), "Displays pressure as red and blue, and velocity as white");
	// addDisplayModeCheckbox(DISPLAY_AIRP, IconPressure  , ui::Point(135, 22), "Displays pressure, red is positive and blue is negative");
	// addDisplayModeCheckbox(DISPLAY_AIRV, IconVelocity  , ui::Point(167,  4), "Displays velocity and positive pressure: up/down adds blue, right/left adds red, still pressure adds green");
	// addDisplayModeCheckbox(DISPLAY_AIRH, IconHeat      , ui::Point(167, 22), "Displays the temperature of the air like heat display does");
	// addDisplayModeCheckbox(DISPLAY_AIRW, IconVort      , ui::Point(199,  4), "Displays vorticity, red is clockwise and blue is anticlockwise");
	// line2 = 232;
	// addDisplayModeCheckbox(DISPLAY_WARP, IconWarp      , ui::Point(237, 22), "Gravity lensing, Newtonian Gravity bends light with this on");
	// addDisplayModeCheckbox(DISPLAY_EFFE, IconEffect    , ui::Point(237,  4), "Enables moving solids, stickmen guns, and premium(tm) graphics");
	// addDisplayModeCheckbox(DISPLAY_PERS, IconPersistant, ui::Point(269,  4), "Element paths persist on the screen for a while");
	// line3 = 302;

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
	addColourModeCheckbox(COLOUR_HEAT, IconHeat    , ui::Point(275,  4), ByteString("渲染物质温度,低温深蓝色,高温粉红色").FromUtf8());
	addColourModeCheckbox(COLOUR_LIFE, IconLife    , ui::Point(275, 22), ByteString("灰度渲染物质life值大小").FromUtf8());
	addColourModeCheckbox(COLOUR_GRAD, IconGradient, ui::Point(307, 22), ByteString("轻微改变元素的颜色,渲染热量扩散效果").FromUtf8());
	addColourModeCheckbox(COLOUR_BASC, IconBasic   , ui::Point(307,  4), ByteString("轻微改变元素的颜色,渲染热量扩散效果").FromUtf8());
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
