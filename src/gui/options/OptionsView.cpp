#include "OptionsView.h"
#include "Format.h"
#include "OptionsController.h"
#include "OptionsModel.h"
#include "common/clipboard/Clipboard.h"
#include "common/platform/Platform.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "gui/Style.h"
#include "simulation/ElementDefs.h"
#include "simulation/SimulationData.h"
#include "client/Client.h"
#include "prefs/GlobalPrefs.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/InformationMessage.h"
#include "gui/interface/Button.h"
#include "gui/interface/Checkbox.h"
#include "gui/interface/DropDown.h"
#include "gui/interface/Engine.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/DirectionSelector.h"
#include "PowderToySDL.h"
#include "Config.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <SDL.h>

OptionsView::OptionsView() : ui::Window(ui::Point(-1, -1), ui::Point(320, 340))
{
	auto autoWidth = [this](ui::Component *c, int extra) {
		c->Size.X = Size.X - c->Position.X - 12 - extra;
	};
	
	{
		auto *label = new ui::Label(ui::Point(4, 1), ui::Point(Size.X-8, 22), ByteString("设置").FromUtf8());
		label->SetTextColour(style::Colour::InformationTitle);
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		autoWidth(label, 0);
		AddComponent(label);
	}

	class Separator : public ui::Component
	{
		public:
		Separator(ui::Point position, ui::Point size) : Component(position, size){}
		virtual ~Separator(){}

		void Draw(const ui::Point& screenPos) override
		{
			GetGraphics()->BlendRect(RectSized(screenPos, Size), 0xFFFFFF_rgb .WithAlpha(180));
		}		
	};
	
	Separator *tmpSeparator = new Separator(ui::Point(0, 22), ui::Point(Size.X, 1));
	AddComponent(tmpSeparator);

	scrollPanel = new ui::ScrollPanel(ui::Point(1, 23), ui::Point(Size.X-2, Size.Y-39));
	
	AddComponent(scrollPanel);

	int currentY = 8;
	auto addLabel = [this, &currentY, &autoWidth](int indent, String text) {
		auto *label = new ui::Label(ui::Point(22 + indent * 15, currentY), ui::Point(1, 16), "");
		autoWidth(label, 0);
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		label->SetMultiline(true);
		label->SetText("\bg" + text); // stupid hack because autoWidth just changes Size.X and that doesn't update the text wrapper
		label->AutoHeight();
		scrollPanel->AddChild(label);
		currentY += label->Size.Y - 1;
	};
	auto addCheckbox = [this, &currentY, &autoWidth, &addLabel](int indent, String text, String info, std::function<void ()> action) {
		auto *checkbox = new ui::Checkbox(ui::Point(8 + indent * 15, currentY), ui::Point(1, 16), text, "");
		autoWidth(checkbox, 0);
		checkbox->SetActionCallback({ action });
		currentY += 14;
		if (info.size())
		{
			addLabel(indent, info);
		}
		currentY += 4;
		scrollPanel->AddChild(checkbox);
		return checkbox;
	};
	auto addDropDown = [this, &currentY, &autoWidth](String info, std::vector<std::pair<String, int>> options, std::function<void ()> action) {
		auto *dropDown = new ui::DropDown(ui::Point(Size.X - 95, currentY), ui::Point(80, 16));
		scrollPanel->AddChild(dropDown);
		for (auto &option : options)
		{
			dropDown->AddOption(option);
		}
		dropDown->SetActionCallback({ action });
		auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X - 96, 16), info);
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		scrollPanel->AddChild(label);
		autoWidth(label, 85);
		currentY += 20;
		return dropDown;
	};
	auto addSeparator = [this, &currentY]() {
		currentY += 6;
		auto *separator = new Separator(ui::Point(0, currentY), ui::Point(Size.X, 1));
		scrollPanel->AddChild(separator);
		currentY += 11;
	};

	heatSimulation = addCheckbox(0, ByteString("热模拟 \bg34.0版本后加入").FromUtf8(), ByteString("\bg 关闭此选项可能导致一些奇怪的问题").FromUtf8(), [this] {
		c->SetHeatSimulation(heatSimulation->GetChecked());
	});
	newtonianGravity = addCheckbox(0, ByteString("牛顿引力模拟 \bg48.0版本后加入").FromUtf8(), ByteString("\bg 可能会降低游戏运行的效能").FromUtf8(), [this] {
		c->SetNewtonianGravity(newtonianGravity->GetChecked());
	});
	ambientHeatSimulation = addCheckbox(0, ByteString("环境热模拟 \bg50.0版本后加入").FromUtf8(), ByteString("\bg 关闭此项可能导致一些沙盘不能正常运行").FromUtf8(), [this] {
		c->SetAmbientHeatSimulation(ambientHeatSimulation->GetChecked());
	});
	waterEqualisation = addCheckbox(0, ByteString("连通器模拟 \bg61.0版本后加入 ").FromUtf8(), ByteString("\bg 有大量液体存在时会降低游戏运行的效能").FromUtf8(), [this] {
		c->SetWaterEqualisation(waterEqualisation->GetChecked());
	});
	airMode = addDropDown(ByteString("空气模拟模式").FromUtf8(), {
		{ ByteString("开启").FromUtf8(), AIR_ON },
		{ ByteString("关闭压力").FromUtf8(), AIR_PRESSUREOFF },
		{ ByteString("关闭速度").FromUtf8(), AIR_VELOCITYOFF },
		{ ByteString("关闭").FromUtf8(), AIR_OFF },
		{ ByteString("更新停止").FromUtf8(), AIR_NOUPDATE },
	}, [this] {
		c->SetAirMode(airMode->GetOption().second);
	});
	{
		ambientAirTemp = new ui::Textbox(ui::Point(Size.X-95, currentY), ui::Point(60, 16));
		ambientAirTemp->SetActionCallback({ [this] {
			UpdateAirTemp(ambientAirTemp->GetText(), false);
		} });
		ambientAirTemp->SetDefocusCallback({ [this] {
			UpdateAirTemp(ambientAirTemp->GetText(), true);
		}});
		ambientAirTemp->SetLimit(9);
		scrollPanel->AddChild(ambientAirTemp);
		ambientAirTempPreview = new ui::Button(ui::Point(Size.X-31, currentY), ui::Point(16, 16), "", ByteString("预览").FromUtf8());
		scrollPanel->AddChild(ambientAirTempPreview);
		auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X-105, 16), ByteString("环境空气温度").FromUtf8());
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		scrollPanel->AddChild(label);
		currentY += 20;
	}
	class GravityWindow : public ui::Window
	{
		void OnTryExit(ExitMethod method) override
		{
			CloseActiveWindow();
			SelfDestruct();
		}

		void OnDraw() override
		{
			Graphics * g = GetGraphics();

			g->DrawFilledRect(RectSized(Position - Vec2{ 1, 1 }, Size + Vec2{ 2, 2 }), 0x000000_rgb);
			g->DrawRect(RectSized(Position, Size), 0xC8C8C8_rgb);
		}

		ui::DirectionSelector * gravityDirection;
		ui::Label * labelValues;

		OptionsController * c;

	public:
		GravityWindow(ui::Point position, float scale, int radius, float x, float y, OptionsController * c_):
			ui::Window(position, ui::Point((radius * 5 / 2) + 20, (radius * 5 / 2) + 75)),
			gravityDirection(new ui::DirectionSelector(ui::Point(10, 32), scale, radius, radius / 4, 2, 5)),
			c(c_)
			{
				ui::Label * tempLabel = new ui::Label(ui::Point(4, 1), ui::Point(Size.X - 8, 22),  ByteString("自定义引力").FromUtf8());
				tempLabel->SetTextColour(style::Colour::InformationTitle);
				tempLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
				tempLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				AddComponent(tempLabel);

				Separator * tempSeparator = new Separator(ui::Point(0, 22), ui::Point(Size.X, 1));
				AddComponent(tempSeparator);

				labelValues = new ui::Label(ui::Point(0, (radius * 5 / 2) + 37), ui::Point(Size.X, 16), String::Build(Format::Precision(1), "X:", x, " Y:", y, " Total:", std::hypot(x, y)));
				labelValues->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
				labelValues->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				AddComponent(labelValues);

				gravityDirection->SetValues(x, y);
				gravityDirection->SetUpdateCallback([this](float x, float y) {
					labelValues->SetText(String::Build(Format::Precision(1), "X:", x, " Y:", y, " Total:", std::hypot(x, y)));
				});
				gravityDirection->SetSnapPoints(5, 5, 2);
				AddComponent(gravityDirection);

				ui::Button * okayButton = new ui::Button(ui::Point(0, Size.Y - 17), ui::Point(Size.X, 17), "OK");
				okayButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
				okayButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				okayButton->Appearance.BorderInactive = ui::Colour(200, 200, 200);
				okayButton->SetActionCallback({ [this] {
					c->SetCustomGravityX(gravityDirection->GetXValue());
					c->SetCustomGravityY(gravityDirection->GetYValue());
					CloseActiveWindow();
					SelfDestruct();
				} });
				AddComponent(okayButton);
				SetOkayButton(okayButton);

				MakeActiveWindow();
			}
	};
	gravityMode = addDropDown(ByteString("重力模拟模式").FromUtf8(), {
		{ ByteString("竖直").FromUtf8(), GRAV_VERTICAL },
		{ ByteString("关闭").FromUtf8(), GRAV_OFF },
		{ ByteString("中心").FromUtf8(), GRAV_RADIAL },
		{ ByteString("自定义").FromUtf8(), GRAV_CUSTOM },
	}, [this] {
		c->SetGravityMode(gravityMode->GetOption().second);
		if (gravityMode->GetOption().second == 3)
		{
			new GravityWindow(ui::Point(-1, -1), 0.05f, 40, customGravityX, customGravityY, c);
		}
	});
	edgeMode = addDropDown(ByteString("边界模式").FromUtf8(), {
		{ ByteString("虚空").FromUtf8(), EDGE_VOID },
		{ ByteString("固体").FromUtf8(), EDGE_SOLID },
		{ ByteString("循环").FromUtf8(), EDGE_LOOP },
	}, [this] {
		c->SetEdgeMode(edgeMode->GetOption().second);
	});
	temperatureScale = addDropDown(ByteString("温度单位").FromUtf8(), {
		{ ByteString("开尔文").FromUtf8(), 0 },
		{ ByteString("摄氏度").FromUtf8(), 1 },
		{ ByteString("华氏度").FromUtf8(), 2 },
	}, [this] {
		c->SetTemperatureScale(temperatureScale->GetOption().second);
	});
	if (FORCE_WINDOW_FRAME_OPS != forceWindowFrameOpsHandheld)
	{
		addSeparator();
		std::vector<std::pair<String, int>> options;
		int currentScale = ui::Engine::Ref().GetScale();
		int scaleIndex = 1;
		bool currentScaleValid = false;
		do
		{
			if (currentScale == scaleIndex)
			{
				currentScaleValid = true;
			}
			options.push_back({ String::Build(scaleIndex), scaleIndex });
			scaleIndex += 1;
		}
		while (desktopWidth >= GetGraphics()->Size().X * scaleIndex && desktopHeight >= GetGraphics()->Size().Y * scaleIndex);
		if (!currentScaleValid)
		{
			options.push_back({ ByteString("当前").FromUtf8(), currentScale });
		}
		scale = addDropDown(ByteString("\bg缩放屏幕的窗口比例因子").FromUtf8(), options, [this] {
			c->SetScale(scale->GetOption().second);
		});
	}
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
	{
		resizable = addCheckbox(0, ByteString("可调整大小 \bg - 允许调整大小和最大化窗口").FromUtf8(), "", [this] {
			c->SetResizable(resizable->GetChecked());
		});
		fullscreen = addCheckbox(0, ByteString("全屏 \bg - 进入全屏模式").FromUtf8(), "", [this] {
			c->SetFullscreen(fullscreen->GetChecked());
		});
		changeResolution = addCheckbox(1, ByteString("设置最佳屏幕分辨率").FromUtf8(), "", [this] {
			c->SetChangeResolution(changeResolution->GetChecked());
		});
		forceIntegerScaling = addCheckbox(1, ByteString("强制整数倍缩放 \bg - 不再那么模糊").FromUtf8(), "", [this] {
			c->SetForceIntegerScaling(forceIntegerScaling->GetChecked());
		});
	}
	blurryScaling = addCheckbox(0, ByteString("模糊缩放 \bg - 启用此项加强在超大屏幕上效果").FromUtf8(), "", [this] {
		c->SetBlurryScaling(blurryScaling->GetChecked());
	});
	addSeparator();
	if (ALLOW_QUIT)
	{
		fastquit = addCheckbox(0, ByteString("快速退出").FromUtf8(), ByteString("点击关闭按钮时总是完全退出游戏").FromUtf8(), [this] {
			c->SetFastQuit(fastquit->GetChecked());
		});
	}
	showAvatars = addCheckbox(0, ByteString("显示头像").FromUtf8(), ByteString("禁用此项可减少使用的网络带宽").FromUtf8(), [this] {
		c->SetShowAvatars(showAvatars->GetChecked());
	});
	momentumScroll = addCheckbox(0, ByteString("加速/旧版滚动").FromUtf8(), ByteString("启用此项时将步进滚动改为加速").FromUtf8(), [this] {
		c->SetMomentumScroll(momentumScroll->GetChecked());
	});
	mouseClickRequired = addCheckbox(0, ByteString("置顶类别").FromUtf8(), ByteString("启用此项时将滑动切换类别改为点击").FromUtf8(), [this] {
		c->SetMouseClickrequired(mouseClickRequired->GetChecked());
	});
	includePressure = addCheckbox(0, ByteString("压力数据").FromUtf8(), ByteString("沙盘,Stamps,复制时保存压力数据").FromUtf8(), [this] {
		c->SetIncludePressure(includePressure->GetChecked());
	});
	perfectCircle = addCheckbox(0, ByteString("完美的圆").FromUtf8(), ByteString("由Notch创造的最完美的圆").FromUtf8(), [this] {
		c->SetPerfectCircle(perfectCircle->GetChecked());
	});
	graveExitsConsole = addCheckbox(0, "Key under Esc exits console", "Disable if that key is 0 on your keyboard", [this] {
		c->SetGraveExitsConsole(graveExitsConsole->GetChecked());
	});
	if constexpr (PLATFORM_CLIPBOARD)
	{
		auto indent = 0;
		nativeClipoard = addCheckbox(indent, ByteString("使用全局剪贴板").FromUtf8(), ByteString("允许跨TPT实例进行复制和粘贴").FromUtf8(), [this] {
			c->SetNativeClipoard(nativeClipoard->GetChecked());
		});
		currentY -= 4; // temporarily undo the currentY += 4 at the end of addCheckbox
		if (auto extra = Clipboard::Explanation())
		{
			addLabel(indent, "\bg" + *extra);
		}
		currentY += 4; // and then undo the undo
	}
	decoSpace = addDropDown(ByteString("\bg装饰工具使用的颜色空间").FromUtf8(), {
		{ "sRGB", DECOSPACE_SRGB },
		{ "Linear", DECOSPACE_LINEAR },
		{ "Gamma 2.2", DECOSPACE_GAMMA22 },
		{ "Gamma 1.8", DECOSPACE_GAMMA18 },
	}, [this] {
		c->SetDecoSpace(decoSpace->GetOption().second);
	});
	maxSounds = new ui::Textbox(ui::Point(Size.X-95, currentY), ui::Point(60, 16));
	maxSounds->SetDefocusCallback({ [this] {
		UpdateMaxSounds(maxSounds->GetText());
	}});
	maxSounds->SetLimit(3);
	scrollPanel->AddChild(maxSounds);
	auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X-105, 16), "Max sounds");
	label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	scrollPanel->AddChild(label);
	currentY += 20;

	currentY += 4;
	if (ALLOW_DATA_FOLDER)
	{
		auto *dataFolderButton = new ui::Button(ui::Point(10, currentY), ui::Point(90, 16), ByteString("打开数据目录").FromUtf8());
		dataFolderButton->SetActionCallback({ [] {
			ByteString cwd = Platform::GetCwd();
			if (!cwd.empty())
			{
				Platform::OpenURI(cwd);
			}
			else
			{
				std::cerr << "Cannot open data folder: Platform::GetCwd(...) failed" << std::endl;
			}
		} });
		scrollPanel->AddChild(dataFolderButton);
		auto *migrationButton = new ui::Button(ui::Point(Size.X - 178, currentY), ui::Point(163, 16), ByteString("迁移至用户共享数据目录").FromUtf8());
		migrationButton->SetActionCallback({ [] {
			ByteString from = Platform::originalCwd;
			ByteString to = Platform::sharedCwd;
			new ConfirmPrompt("Do Migration?", "This will migrate all stamps, saves, and scripts from\n\bt" + from.FromUtf8() + "\bw\nto the shared data directory at\n\bt" + to.FromUtf8() + "\bw\n\n" + "Files that already exist will not be overwritten.", { [from, to]() {
				String ret = Client::Ref().DoMigration(from, to);
				new InformationMessage("Migration Complete", ret, false);
			} });
		} });
		scrollPanel->AddChild(migrationButton);
		currentY += 26;
	}
	{
		ui::Button *ok = new ui::Button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), "OK");
		ok->SetActionCallback({ [this] {
			c->Exit();
		} });
		AddComponent(ok);
		SetCancelButton(ok);
		SetOkayButton(ok);
	}
	scrollPanel->InnerSize = ui::Point(Size.X, currentY);
}

void OptionsView::UpdateAmbientAirTempPreview(float airTemp, bool isValid)
{
	if (isValid)
	{
		ambientAirTempPreview->Appearance.BackgroundInactive = RGB<uint8_t>::Unpack(HeatToColour(airTemp)).WithAlpha(0xFF);
		ambientAirTempPreview->SetText("");
	}
	else
	{
		ambientAirTempPreview->Appearance.BackgroundInactive = ui::Colour(0, 0, 0);
		ambientAirTempPreview->SetText("?");
	}
	ambientAirTempPreview->Appearance.BackgroundHover = ambientAirTempPreview->Appearance.BackgroundInactive;
}

void OptionsView::AmbientAirTempToTextBox(float airTemp)
{
	StringBuilder sb;
	sb << Format::Precision(2);
	format::RenderTemperature(sb, airTemp, temperatureScale->GetOption().second);
	ambientAirTemp->SetText(sb.Build());
}

void OptionsView::UpdateAirTemp(String temp, bool isDefocus)
{
	// Parse air temp and determine validity
	float airTemp = 0;
	bool isValid;
	try
	{
		airTemp = format::StringToTemperature(temp, temperatureScale->GetOption().second);
		isValid = true;
	}
	catch (const std::exception &ex)
	{
		isValid = false;
	}

	// While defocusing, correct out of range temperatures and empty textboxes
	if (isDefocus)
	{
		if (temp.empty())
		{
			isValid = true;
			airTemp = float(R_TEMP) + 273.15f;
		}
		else if (!isValid)
			return;
		else if (airTemp < MIN_TEMP)
			airTemp = MIN_TEMP;
		else if (airTemp > MAX_TEMP)
			airTemp = MAX_TEMP;

		AmbientAirTempToTextBox(airTemp);
	}
	// Out of range temperatures are invalid, preview should go away
	else if (isValid && (airTemp < MIN_TEMP || airTemp > MAX_TEMP))
		isValid = false;

	// If valid, set temp
	if (isValid)
		c->SetAmbientAirTemperature(airTemp);

	UpdateAmbientAirTempPreview(airTemp, isValid);
}

void OptionsView::UpdateMaxSounds(String sounds)
{
	int max = -1;
	try
	{
		max = std::clamp(sounds.ToNumber<int>(), 0, 999);
	}
	catch (const std::exception &e)
	{
		maxSounds->SetText(String::Build(c->GetMaxSounds()));
		return;
	}

	if (max < interfaceRng.between(0, 40))
	{
		int coins = GlobalPrefs::Ref().Get("Coins.coins", 0);
		if (coins < 20)
		{
			new ErrorMessage("Error", String::Build("This options requires 20 \xEE\x81\xAAowdercoins to change, but you only have ", coins));
			maxSounds->SetText(String::Build(c->GetMaxSounds()));
			return;
		}
		GlobalPrefs::Ref().Set("Coins.coins", coins - 20);
	}
	c->SetMaxSounds(max);
}

void OptionsView::NotifySettingsChanged(OptionsModel * sender)
{
	temperatureScale->SetOption(sender->GetTemperatureScale()); // has to happen before AmbientAirTempToTextBox is called
	heatSimulation->SetChecked(sender->GetHeatSimulation());
	ambientHeatSimulation->SetChecked(sender->GetAmbientHeatSimulation());
	newtonianGravity->SetChecked(sender->GetNewtonianGravity());
	waterEqualisation->SetChecked(sender->GetWaterEqualisation());
	airMode->SetOption(sender->GetAirMode());
	// Initialize air temp and preview only when the options menu is opened, and not when user is actively editing the textbox
	if (!ambientAirTemp->IsFocused())
	{
		float airTemp = sender->GetAmbientAirTemperature();
		UpdateAmbientAirTempPreview(airTemp, true);
		AmbientAirTempToTextBox(airTemp);
	}
	gravityMode->SetOption(sender->GetGravityMode());
	customGravityX = sender->GetCustomGravityX();
	customGravityY = sender->GetCustomGravityY();
	decoSpace->SetOption(sender->GetDecoSpace());
	edgeMode->SetOption(sender->GetEdgeMode());
	if (scale)
	{
		scale->SetOption(sender->GetScale());
	}
	if (resizable)
	{
		resizable->SetChecked(sender->GetResizable());
	}
	if (fullscreen)
	{
		fullscreen->SetChecked(sender->GetFullscreen());
	}
	if (changeResolution)
	{
		changeResolution->SetChecked(sender->GetChangeResolution());
	}
	if (forceIntegerScaling)
	{
		forceIntegerScaling->SetChecked(sender->GetForceIntegerScaling());
	}
	if (blurryScaling)
	{
		blurryScaling->SetChecked(sender->GetBlurryScaling());
	}
	if (fastquit)
	{
		fastquit->SetChecked(sender->GetFastQuit());
	}
	if (nativeClipoard)
	{
		nativeClipoard->SetChecked(sender->GetNativeClipoard());
	}
	showAvatars->SetChecked(sender->GetShowAvatars());
	mouseClickRequired->SetChecked(sender->GetMouseClickRequired());
	includePressure->SetChecked(sender->GetIncludePressure());
	perfectCircle->SetChecked(sender->GetPerfectCircle());
	graveExitsConsole->SetChecked(sender->GetGraveExitsConsole());
	momentumScroll->SetChecked(sender->GetMomentumScroll());
	maxSounds->SetText(String::Build(sender->GetMaxSounds()));
}

void OptionsView::AttachController(OptionsController * c_)
{
	c = c_;
}

void OptionsView::OnDraw()
{
	Graphics * g = GetGraphics();
	g->DrawFilledRect(RectSized(Position - Vec2{ 1, 1 }, Size + Vec2{ 2, 2 }), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);
}

void OptionsView::OnTryExit(ExitMethod method)
{
	c->Exit();
}
