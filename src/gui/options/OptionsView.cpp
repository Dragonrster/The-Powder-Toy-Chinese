#include "OptionsView.h"
#include "Format.h"
#include "OptionsController.h"
#include "OptionsModel.h"
#include "common/platform/Platform.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "gui/Style.h"
#include "simulation/ElementDefs.h"
#include "client/Client.h"
#include "gui/dialogues/ConfirmPrompt.h"
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
		auto *label = new ui::Label(ui::Point(4, 1), ui::Point(Size.X-8, 22), ByteString("設定").FromUtf8());
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
	auto addCheckbox = [this, &currentY, &autoWidth](int indent, String text, String info, std::function<void ()> action) {
		auto *checkbox = new ui::Checkbox(ui::Point(8 + indent * 15, currentY), ui::Point(1, 16), text, "");
		autoWidth(checkbox, 0);
		checkbox->SetActionCallback({ action });
		scrollPanel->AddChild(checkbox);
		currentY += 14;
		if (info.size())
		{
			auto *label = new ui::Label(ui::Point(22 + indent * 15, currentY), ui::Point(1, 16), "\bg" + info);
			autoWidth(label, 0);
			label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
			label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
			scrollPanel->AddChild(label);
			currentY += 14;
		}
		currentY += 4;
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

	heatSimulation = addCheckbox(0, ByteString("熱模擬 \bg34.0版本後加入").FromUtf8(), ByteString("\bg 關閉此選項可能導致一些奇怪的問題").FromUtf8(), [this] {
		c->SetHeatSimulation(heatSimulation->GetChecked());
	});
	newtonianGravity = addCheckbox(0, ByteString("牛頓引力模擬 \bg48.0版本後加入").FromUtf8(), ByteString("\bg 可能會降低遊戲執行的效能").FromUtf8(), [this] {
		c->SetNewtonianGravity(newtonianGravity->GetChecked());
	});
	ambientHeatSimulation = addCheckbox(0, ByteString("環境熱模擬 \bg50.0版本後加入").FromUtf8(), ByteString("\bg 關閉此項可能導致一些沙盤不能正常執行").FromUtf8(), [this] {
		c->SetAmbientHeatSimulation(ambientHeatSimulation->GetChecked());
	});
	waterEqualisation = addCheckbox(0, ByteString("連通器模擬 \bg61.0版本後加入 ").FromUtf8(), ByteString("\bg 有大量液體存在時會降低遊戲執行的效能").FromUtf8(), [this] {
		c->SetWaterEqualisation(waterEqualisation->GetChecked());
	});
	airMode = addDropDown(ByteString("空氣模擬模式").FromUtf8(), {
		{ ByteString("開啟").FromUtf8(), 0 },
		{ ByteString("關閉壓力").FromUtf8(), 1 },
		{ ByteString("關閉速度").FromUtf8(), 2 },
		{ ByteString("關閉").FromUtf8(), 3 },
		{ ByteString("更新停止").FromUtf8(), 4 },
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
		scrollPanel->AddChild(ambientAirTemp);
		ambientAirTempPreview = new ui::Button(ui::Point(Size.X-31, currentY), ui::Point(16, 16), "", ByteString("預覽").FromUtf8());
		scrollPanel->AddChild(ambientAirTempPreview);
		auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X-96, 16), ByteString("環境空氣溫度").FromUtf8());
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
				ui::Label * tempLabel = new ui::Label(ui::Point(4, 1), ui::Point(Size.X - 8, 22),  ByteString("自定義引力").FromUtf8());
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
	gravityMode = addDropDown(ByteString("重力模擬模式").FromUtf8(), {
		{ ByteString("豎直").FromUtf8(), 0 },
		{ ByteString("關閉").FromUtf8(), 1 },
		{ ByteString("中心").FromUtf8(), 2 },
		{ ByteString("自定義").FromUtf8(), 3 },
	}, [this] {
		c->SetGravityMode(gravityMode->GetOption().second);
		if (gravityMode->GetOption().second == 3)
		{
			new GravityWindow(ui::Point(-1, -1), 0.05f, 40, customGravityX, customGravityY, c);
		}
	});
	edgeMode = addDropDown(ByteString("邊界模式").FromUtf8(), {
		{ ByteString("虛空").FromUtf8(), 0 },
		{ ByteString("固體").FromUtf8(), 1 },
		{ ByteString("迴圈").FromUtf8(), 2 },
	}, [this] {
		c->SetEdgeMode(edgeMode->GetOption().second);
	});
	temperatureScale = addDropDown(ByteString("溫度單位").FromUtf8(), {
		{ ByteString("開爾文").FromUtf8(), 0 },
		{ ByteString("攝氏度").FromUtf8(), 1 },
		{ ByteString("華氏度").FromUtf8(), 2 },
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
			options.push_back({ ByteString("當前").FromUtf8(), currentScale });
		}
		scale = addDropDown(ByteString("\bg縮放螢幕的視窗比例因子").FromUtf8(), options, [this] {
			c->SetScale(scale->GetOption().second);
		});
	}
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
	{
		resizable = addCheckbox(0, ByteString("可調整大小 \bg - 允許調整大小和最大化視窗").FromUtf8(), "", [this] {
			c->SetResizable(resizable->GetChecked());
		});
		fullscreen = addCheckbox(0, ByteString("全屏 \bg - 進入全屏模式").FromUtf8(), "", [this] {
			c->SetFullscreen(fullscreen->GetChecked());
		});
		changeResolution = addCheckbox(1, ByteString("設定最佳螢幕解析度").FromUtf8(), "", [this] {
			c->SetChangeResolution(changeResolution->GetChecked());
		});
		forceIntegerScaling = addCheckbox(1, ByteString("強制整數倍縮放 \bg - 不再那麼模糊").FromUtf8(), "", [this] {
			c->SetForceIntegerScaling(forceIntegerScaling->GetChecked());
		});
	}
<<<<<<< Updated upstream
	blurryScaling = addCheckbox(0, "Blurry scaling \bg- more blurry, better on very big screens", "", [this] {
=======
	blurryScaling = addCheckbox(0, ByteString("模糊縮放 \bg - 啟用此項加強在超大螢幕上效果").FromUtf8(), "", [this] {
>>>>>>> Stashed changes
		c->SetBlurryScaling(blurryScaling->GetChecked());
	});
	addSeparator();
	if (ALLOW_QUIT)
	{
		fastquit = addCheckbox(0, ByteString("快速退出").FromUtf8(), ByteString("點選關閉按鈕時總是完全退出遊戲").FromUtf8(), [this] {
			c->SetFastQuit(fastquit->GetChecked());
		});
	}
	showAvatars = addCheckbox(0, ByteString("顯示頭像").FromUtf8(), ByteString("停用此項可減少使用的網路頻寬").FromUtf8(), [this] {
		c->SetShowAvatars(showAvatars->GetChecked());
	});
	momentumScroll = addCheckbox(0, ByteString("加速/舊版滾動").FromUtf8(), ByteString("啟用此項時將步進滾動改為加速").FromUtf8(), [this] {
		c->SetMomentumScroll(momentumScroll->GetChecked());
	});
	mouseClickRequired = addCheckbox(0, ByteString("置頂類別").FromUtf8(), ByteString("啟用此項時將滑動切換類別改為點選").FromUtf8(), [this] {
		c->SetMouseClickrequired(mouseClickRequired->GetChecked());
	});
	includePressure = addCheckbox(0, ByteString("壓力資料").FromUtf8(), ByteString("沙盤,Stamps,複製時儲存壓力資料").FromUtf8(), [this] {
		c->SetIncludePressure(includePressure->GetChecked());
	});
	perfectCircle = addCheckbox(0, ByteString("完美的圓").FromUtf8(), ByteString("由Notch創造的最完美的圓").FromUtf8(), [this] {
		c->SetPerfectCircle(perfectCircle->GetChecked());
	});
	graveExitsConsole = addCheckbox(0, "Key under Esc exits console", "Disable if that key is 0 on your keyboard", [this] {
		c->SetGraveExitsConsole(graveExitsConsole->GetChecked());
	});
	decoSpace = addDropDown(ByteString("\bg裝飾工具使用的顏色空間").FromUtf8(), {
		{ "sRGB", 0 },
		{ "Linear", 1 },
		{ "Gamma 2.2", 2 },
		{ "Gamma 1.8", 3 },
	}, [this] {
		c->SetDecoSpace(decoSpace->GetOption().second);
	});

	currentY += 4;
	if (ALLOW_DATA_FOLDER)
	{
		auto *dataFolderButton = new ui::Button(ui::Point(10, currentY), ui::Point(90, 16), ByteString("開啟資料目錄").FromUtf8());
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
		auto *migrationButton = new ui::Button(ui::Point(Size.X - 178, currentY), ui::Point(163, 16), ByteString("遷移至使用者共享資料目錄").FromUtf8());
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
	showAvatars->SetChecked(sender->GetShowAvatars());
	mouseClickRequired->SetChecked(sender->GetMouseClickRequired());
	includePressure->SetChecked(sender->GetIncludePressure());
	perfectCircle->SetChecked(sender->GetPerfectCircle());
	graveExitsConsole->SetChecked(sender->GetGraveExitsConsole());
	momentumScroll->SetChecked(sender->GetMomentumScroll());
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
