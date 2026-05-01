#include "OptionsView.h"
#include "Format.h"
#include "OptionsController.h"
#include "OptionsModel.h"
#include "common/clipboard/Clipboard.h"
#include "common/platform/Platform.h"
#include "common/Localization.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "gui/Style.h"
#include "simulation/ElementDefs.h"
#include "simulation/SimulationSettings.h"
#include "simulation/Air.h"
#include "client/Client.h"
#include "gui/credits/Credits.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/InformationMessage.h"
#include "gui/interface/Button.h"
#include "gui/interface/Checkbox.h"
#include "gui/interface/DropDown.h"
#include "gui/interface/Engine.h"
#include "gui/interface/Label.h"
#include "gui/interface/Separator.h"
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
		// 顶部标题使用本地化文本
		titleLabel = new ui::Label(ui::Point(4, 1), ui::Point(Size.X-8, 22),
			Localization::Ref().Tr("options.title"));
		titleLabel->SetTextColour(style::Colour::InformationTitle);
		titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		autoWidth(titleLabel, 0);
		AddComponent(titleLabel);
	}

	auto *tmpSeparator = new ui::Separator(ui::Point(0, 22), ui::Point(Size.X, 1));
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
		return label;
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
		auto *separator = new ui::Separator(ui::Point(0, currentY), ui::Point(Size.X, 1));
		scrollPanel->AddChild(separator);
		currentY += 11;
	};

	heatSimulation = addCheckbox(0, Localization::Ref().Tr("options.heat_simulation"), Localization::Ref().Tr("options.heat_simulation.info"), [this] {
		c->SetHeatSimulation(heatSimulation->GetChecked());
	});
	newtonianGravity = addCheckbox(0, Localization::Ref().Tr("options.newtonian_gravity"), Localization::Ref().Tr("options.newtonian_gravity.info"), [this] {
		c->SetNewtonianGravity(newtonianGravity->GetChecked());
	});
	ambientHeatSimulation = addCheckbox(0, Localization::Ref().Tr("options.ambient_heat_simulation"), Localization::Ref().Tr("options.ambient_heat_simulation.info"), [this] {
		c->SetAmbientHeatSimulation(ambientHeatSimulation->GetChecked());
	});
	waterEqualisation = addCheckbox(0, Localization::Ref().Tr("options.water_equalisation"), Localization::Ref().Tr("options.water_equalisation.info"), [this] {
		c->SetWaterEqualisation(waterEqualisation->GetChecked());
	});
	airMode = addDropDown(Localization::Ref().Tr("options.air_mode"), {
		{ Localization::Ref().Tr("options.air.on"), AIR_ON },
		{ Localization::Ref().Tr("options.air.pressure_off"), AIR_PRESSUREOFF },
		{ Localization::Ref().Tr("options.air.velocity_off"), AIR_VELOCITYOFF },
		{ Localization::Ref().Tr("options.air.off"), AIR_OFF },
		{ Localization::Ref().Tr("options.air.no_update"), AIR_NOUPDATE },
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
		ambientAirTempPreview = new ui::Button(ui::Point(Size.X-31, currentY), ui::Point(16, 16), "", Localization::Ref().Tr("options.preview"));
		scrollPanel->AddChild(ambientAirTempPreview);
		auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X-105, 16), Localization::Ref().Tr("options.ambient_air_temperature"));
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		scrollPanel->AddChild(label);
		currentY += 20;
	}
	{ // Vorticity coefficient setting
		vorticityCoeff = new ui::Textbox(ui::Point(Size.X-95, currentY), ui::Point(80, 16));
		vorticityCoeff->SetActionCallback({ [this] {
			UpdateVorticityCoeff(vorticityCoeff->GetText(), false);
		} });
		vorticityCoeff->SetDefocusCallback({ [this] {
			UpdateVorticityCoeff(vorticityCoeff->GetText(), true);
		}});
		vorticityCoeff->SetLimit(9);
		scrollPanel->AddChild(vorticityCoeff);
		auto *label = new ui::Label(ui::Point(8, currentY), ui::Point(Size.X-105, 16), Localization::Ref().Tr("options.vorticity_confinement"));
		label->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
		label->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		scrollPanel->AddChild(label);
		currentY += 20;
	}
	convectionMode = addDropDown(Localization::Ref().Tr("options.convection_mode"), {
		{ Localization::Ref().Tr("options.convection.none"), AIRC_NONE },
		{ Localization::Ref().Tr("options.convection.legacy"), AIRC_LEGACY },
		{ Localization::Ref().Tr("options.convection.boussinesq"), AIRC_BOUSSINESQ },
	}, [this] {
		c->SetConvectionMode(convectionMode->GetOption().second);
	});
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
				ui::Label * tempLabel = new ui::Label(ui::Point(4, 1), ui::Point(Size.X - 8, 22), Localization::Ref().Tr("options.custom_gravity"));
				tempLabel->SetTextColour(style::Colour::InformationTitle);
				tempLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
				tempLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				AddComponent(tempLabel);

				auto * tempSeparator = new ui::Separator(ui::Point(0, 22), ui::Point(Size.X, 1));
				AddComponent(tempSeparator);

				labelValues = new ui::Label(ui::Point(0, (radius * 5 / 2) + 37), ui::Point(Size.X, 16), String::Build(Format::Precision(1), Localization::Ref().Tr("options.gravity.x"), x, Localization::Ref().Tr("options.gravity.y"), y, Localization::Ref().Tr("options.gravity.total"), std::hypot(x, y)));
				labelValues->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
				labelValues->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				AddComponent(labelValues);

				gravityDirection->SetValues(x, y);
				gravityDirection->SetUpdateCallback([this](float x, float y) {
					labelValues->SetText(String::Build(Format::Precision(1), Localization::Ref().Tr("options.gravity.x"), x, Localization::Ref().Tr("options.gravity.y"), y, Localization::Ref().Tr("options.gravity.total"), std::hypot(x, y)));
				});
				gravityDirection->SetSnapPoints(5, 5, 2);
				AddComponent(gravityDirection);

				ui::Button * okayButton = new ui::Button(ui::Point(0, Size.Y - 17), ui::Point(Size.X, 17), Localization::Ref().Tr("options.ok"));
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
	gravityMode = addDropDown(Localization::Ref().Tr("options.gravity_mode"), {
		{ Localization::Ref().Tr("options.gravity.vertical"), GRAV_VERTICAL },
		{ Localization::Ref().Tr("options.gravity.off"), GRAV_OFF },
		{ Localization::Ref().Tr("options.gravity.radial"), GRAV_RADIAL },
		{ Localization::Ref().Tr("options.gravity.custom"), GRAV_CUSTOM },
	}, [this] {
		c->SetGravityMode(gravityMode->GetOption().second);
		if (gravityMode->GetOption().second == 3)
		{
			new GravityWindow(ui::Point(-1, -1), 0.05f, 40, customGravityX, customGravityY, c);
		}
	});
	edgeMode = addDropDown(Localization::Ref().Tr("options.edge_mode"), {
		{ Localization::Ref().Tr("options.edge.void"), EDGE_VOID },
		{ Localization::Ref().Tr("options.edge.solid"), EDGE_SOLID },
		{ Localization::Ref().Tr("options.edge.loop"), EDGE_LOOP },
	}, [this] {
		c->SetEdgeMode(edgeMode->GetOption().second);
	});
	temperatureScale = addDropDown(Localization::Ref().Tr("options.temperature"), {
			{ Localization::Ref().Tr("options.temp.kelvin"), TEMPSCALE_KELVIN },
			{ Localization::Ref().Tr("options.temp.celsius"), TEMPSCALE_CELSIUS },
			{ Localization::Ref().Tr("options.temp.fahrenheit"), TEMPSCALE_FAHRENHEIT },
	}, [this] {
		c->SetTemperatureScale(TempScale(temperatureScale->GetOption().second));
	});
	if (FORCE_WINDOW_FRAME_OPS != forceWindowFrameOpsHandheld)
	{
		addSeparator();
		// 语言标签文字本地化
		String langLabel = Localization::Ref().Tr("options.language");
		// 与 Localization::LoadLanguage(index) 一致,按 ISO 639-1 排序
		language = addDropDown(langLabel, {
			{ "English",    0 },
			{ "简体中文",   1 },
			{ "繁體中文",   2 },
			{ "Deutsch",    3 },
			{ "Español",    4 },
			{ "Français",   5 },
			{ "Italiano",   6 },
			{ "日本語",      7 },
			{ "한국어",      8 },
			{ "Português",  9 },
			{ "Русский",   10 },
			{ "文言",      11 },
		}, [this] {
			c->SetLanguage(language->GetOption().second);
		});
  
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
			options.push_back({ Localization::Ref().Tr("options.scale.current"), currentScale });
		}
		scale = addDropDown(Localization::Ref().Tr("options.window_scale"), options, [this] {
			c->SetScale(scale->GetOption().second);
		});
	}
	if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
	{
		resizable = addCheckbox(0, Localization::Ref().Tr("options.resizable"), "", [this] {
			c->SetResizable(resizable->GetChecked());
		});
		fullscreen = addCheckbox(0, Localization::Ref().Tr("options.fullscreen"), "", [this] {
			c->SetFullscreen(fullscreen->GetChecked());
		});
		changeResolution = addCheckbox(1, Localization::Ref().Tr("options.change_resolution"), "", [this] {
			c->SetChangeResolution(changeResolution->GetChecked());
		});
		forceIntegerScaling = addCheckbox(1, Localization::Ref().Tr("options.force_integer_scaling"), "", [this] {
			c->SetForceIntegerScaling(forceIntegerScaling->GetChecked());
		});
	}
	blurryScaling = addCheckbox(0, Localization::Ref().Tr("options.blurry_scaling"), "", [this] {
		c->SetBlurryScaling(blurryScaling->GetChecked());
	});
	addSeparator();
	if (ALLOW_QUIT)
	{
		fastquit = addCheckbox(0, Localization::Ref().Tr("options.fast_quit"), Localization::Ref().Tr("options.fast_quit.info"), [this] {
			c->SetFastQuit(fastquit->GetChecked());
		});
		globalQuit = addCheckbox(0, Localization::Ref().Tr("options.global_quit"), Localization::Ref().Tr("options.global_quit.info"), [this] {
			c->SetGlobalQuit(globalQuit->GetChecked());
		});
	}
	showAvatars = addCheckbox(0, Localization::Ref().Tr("options.show_avatars"), Localization::Ref().Tr("options.show_avatars.info"), [this] {
		c->SetShowAvatars(showAvatars->GetChecked());
	});
	momentumScroll = addCheckbox(0, Localization::Ref().Tr("options.momentum_scroll"), Localization::Ref().Tr("options.momentum_scroll.info"), [this] {
		c->SetMomentumScroll(momentumScroll->GetChecked());
	});
	mouseClickRequired = addCheckbox(0, Localization::Ref().Tr("options.sticky_categories"), Localization::Ref().Tr("options.sticky_categories.info"), [this] {
		c->SetMouseClickrequired(mouseClickRequired->GetChecked());
	});
	includePressure = addCheckbox(0, Localization::Ref().Tr("options.include_pressure"), Localization::Ref().Tr("options.include_pressure.info"), [this] {
		c->SetIncludePressure(includePressure->GetChecked());
	});
	perfectCircle = addCheckbox(0, Localization::Ref().Tr("options.perfect_circle"), Localization::Ref().Tr("options.perfect_circle.info"), [this] {
		c->SetPerfectCircle(perfectCircle->GetChecked());
	});
	graveExitsConsole = addCheckbox(0, Localization::Ref().Tr("options.grave_exits_console"), Localization::Ref().Tr("options.grave_exits_console.info"), [this] {
		c->SetGraveExitsConsole(graveExitsConsole->GetChecked());
	});
	if constexpr (PLATFORM_CLIPBOARD)
	{
		auto indent = 0;
		nativeClipoard = addCheckbox(indent, Localization::Ref().Tr("options.platform_clipboard"), Localization::Ref().Tr("options.platform_clipboard.info"), [this] {
			c->SetNativeClipoard(nativeClipoard->GetChecked());
		});
		currentY -= 4; // temporarily undo the currentY += 4 at the end of addCheckbox
		if (auto extra = Clipboard::Explanation())
		{
			addLabel(indent, "\bg" + *extra);
		}
		currentY += 4; // and then undo the undo
	}
	threadedRendering = addCheckbox(0, Localization::Ref().Tr("options.threaded_rendering"), Localization::Ref().Tr("options.threaded_rendering.info"), [this] {
		c->SetThreadedRendering(threadedRendering->GetChecked());
	});
	decoSpace = addDropDown(Localization::Ref().Tr("options.deco_space"), {
		{ Localization::Ref().Tr("options.deco.srgb"), DECOSPACE_SRGB },
		{ Localization::Ref().Tr("options.deco.linear"), DECOSPACE_LINEAR },
		{ Localization::Ref().Tr("options.deco.gamma22"), DECOSPACE_GAMMA22 },
		{ Localization::Ref().Tr("options.deco.gamma18"), DECOSPACE_GAMMA18 },
	}, [this] {
		c->SetDecoSpace(decoSpace->GetOption().second);
	});

	currentY += 4;
	if constexpr (ALLOW_DATA_FOLDER)
	{
		auto *dataFolderButton = new ui::Button(ui::Point(10, currentY), ui::Point(90, 16), Localization::Ref().Tr("options.open_data_folder"));
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
		if constexpr (SHARED_DATA_FOLDER)
		{
			auto *migrationButton = new ui::Button(ui::Point(Size.X - 178, currentY), ui::Point(163, 16), Localization::Ref().Tr("options.migrate_data"));
			migrationButton->SetActionCallback({ [] {
				ByteString from = Platform::originalCwd;
				ByteString to = Platform::sharedCwd;
				new ConfirmPrompt(Localization::Ref().Tr("options.migrate_confirm_title"), Localization::Ref().Tr("options.migrate_confirm_body_1") + "\n\bt" + from.FromUtf8() + "\bw\n" + Localization::Ref().Tr("options.migrate_confirm_body_2") + "\n\bt" + to.FromUtf8() + "\bw\n\n" + Localization::Ref().Tr("options.migrate_confirm_body_3"), { [from, to]() {
					String ret = Client::Ref().DoMigration(from, to);
					new InformationMessage(Localization::Ref().Tr("options.migrate_complete"), ret, false);
				} });
			} });
			scrollPanel->AddChild(migrationButton);
		}
		currentY += 26;
	}
	String autoStartupRequestNote = Localization::Ref().Tr("options.startup_note");
	if (!IGNORE_UPDATES)
	{
		autoStartupRequestNote += Localization::Ref().Tr("options.startup_note_updates");
	}
	autoStartupRequest = addCheckbox(0, Localization::Ref().Tr("options.fetch_motd"), autoStartupRequestNote, [this] {
		auto checked = autoStartupRequest->GetChecked();
		if (checked)
		{
			Client::Ref().BeginStartupRequest();
		}
		c->SetAutoStartupRequest(checked);
	});
	auto *doStartupRequest = new ui::Button(ui::Point(10, currentY), ui::Point(90, 16), Localization::Ref().Tr("options.fetch_now"));
	doStartupRequest->SetActionCallback({ [] {
		Client::Ref().BeginStartupRequest();
	} });
	scrollPanel->AddChild(doStartupRequest);
	startupRequestStatus = addLabel(5, "");
	UpdateStartupRequestStatus();
	currentY += 13;
	redirectStd = addCheckbox(0, Localization::Ref().Tr("options.redirect_std"), Localization::Ref().Tr("options.redirect_std.info"), [this] {
		c->SetRedirectStd(redirectStd->GetChecked());
	});

	{
		addSeparator();

		auto *creditsButton = new ui::Button(ui::Point(10, currentY), ui::Point(90, 16), Localization::Ref().Tr("options.credits"));
		creditsButton->SetActionCallback({ [] {
			auto *credits = new Credits();
			ui::Engine::Ref().ShowWindow(credits);
		} });
		scrollPanel->AddChild(creditsButton);

		addLabel(5, Localization::Ref().Tr("options.credits_desc"));
		currentY += 13;
	}


	{
		ui::Button *ok = new ui::Button(ui::Point(0, Size.Y-16), ui::Point(Size.X, 16), Localization::Ref().Tr("options.ok"));
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
		ambientAirTempPreview->Appearance.BackgroundInactive = HeatToColour(airTemp, MIN_TEMP, MAX_TEMP).WithAlpha(0xFF);
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
	format::RenderTemperature(sb, airTemp, TempScale(temperatureScale->GetOption().second));
	ambientAirTemp->SetText(sb.Build());
}

void OptionsView::VorticityCoeffToTextBox(float vorticity)
{
	StringBuilder sb;
	sb << Format::Precision(2) << vorticity;
	vorticityCoeff->SetText(sb.Build());
}

void OptionsView::UpdateStartupRequestStatus()
{
	switch (Client::Ref().GetStartupRequestStatus())
	{
	case Client::StartupRequestStatus::notYetDone:
		startupRequestStatus->SetText(Localization::Ref().Tr("options.startup.not_yet"));
		break;

	case Client::StartupRequestStatus::inProgress:
		startupRequestStatus->SetText(Localization::Ref().Tr("options.startup.in_progress"));
		break;

	case Client::StartupRequestStatus::succeeded:
		startupRequestStatus->SetText(String::Build(Localization::Ref().Tr("options.startup.ok_prefix"), Client::Ref().GetServerNotifications().size(), Localization::Ref().Tr("options.startup.notifications_fetched")));
		break;

	case Client::StartupRequestStatus::failed:
		{
			auto error = Client::Ref().GetStartupRequestError();
			if (!error)
			{
				error = "???";
			}
			startupRequestStatus->SetText(Localization::Ref().Tr("options.startup.failed") + error->FromUtf8());
		}
		break;
	}
}

void OptionsView::UpdateAirTemp(String temp, bool isDefocus)
{
	// Parse air temp and determine validity
	float airTemp = 0;
	bool isValid;
	try
	{
		airTemp = format::StringToTemperature(temp, TempScale(temperatureScale->GetOption().second));
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

void OptionsView::UpdateVorticityCoeff(String vort, bool isDefocus)
{
	// Parse vorticity and determine validity
	float vorticity = 0;
	bool isValid;
	try
	{
		vorticity = vort.ToNumber<float>();
		isValid = true;
	}
	catch (const std::exception &ex)
	{
		isValid = false;
	}

	// While defocusing, correct out of range vorticity and empty textboxes
	if (isDefocus)
	{
		if (vort.empty())
		{
			isValid = true;
			vorticity = 0.0f;
		}
		else if (!isValid)
			return;
		else if (vorticity < 0.0f)
			vorticity = 0.0f;
		else if (vorticity > 1.0f)
			vorticity = 1.0f;

		VorticityCoeffToTextBox(vorticity);
	}
	// Out of range vorticities are invalid, preview should go away
	else if (isValid && (vorticity < 0.0f || vorticity > 1.0f))
		isValid = false;

	// If valid, set vorticity
	if (isValid)
		c->SetVorticityCoeff(vorticity);
}

void OptionsView::NotifySettingsChanged(OptionsModel * sender)
{
	// 切换语言后刷新本窗口使用到的本地化文本
	if (titleLabel)
	{
		titleLabel->SetText(Localization::Ref().Tr("options.title"));
	}

	temperatureScale->SetOption(sender->GetTemperatureScale()); // has to happen before AmbientAirTempToTextBox is called
	heatSimulation->SetChecked(sender->GetHeatSimulation());
	ambientHeatSimulation->SetChecked(sender->GetAmbientHeatSimulation());
	language->SetOption(sender->GetLanguage());
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
	// Same for vorticity
	if (!vorticityCoeff->IsFocused())
	{
		VorticityCoeffToTextBox(sender->GetVorticityCoeff());
	}
	convectionMode->SetOption(sender->GetConvectionMode());
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
	if (globalQuit)
	{
		globalQuit->SetChecked(sender->GetGlobalQuit());
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
	threadedRendering->SetChecked(sender->GetThreadedRendering());
	momentumScroll->SetChecked(sender->GetMomentumScroll());
	redirectStd->SetChecked(sender->GetRedirectStd());
	autoStartupRequest->SetChecked(sender->GetAutoStartupRequest());
}

void OptionsView::AttachController(OptionsController * c_)
{
	c = c_;
}

void OptionsView::OnTick()
{
	UpdateStartupRequestStatus();
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
