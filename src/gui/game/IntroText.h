#pragma once
#include "Config.h"
#include "SimulationConfig.h"
#include "common/String.h"

inline ByteString VersionInfo()
{
	ByteStringBuilder sb;
	sb << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1];
	if constexpr (!SNAPSHOT)
	{
		sb << "." << APP_VERSION.build;
	}
	sb << " " << IDENT;
	if constexpr (MOD)
	{
		sb << " MOD " << MOD_ID << " UPSTREAM " << UPSTREAM_VERSION.build;
	}
	if constexpr (SNAPSHOT)
	{
		sb << " SNAPSHOT " << APP_VERSION.build;
	}
	if constexpr (LUACONSOLE)
	{
		sb << " LUACONSOLE";
	}
	if constexpr (LATENTHEAT)
	{
		sb << " LATENTHEAT";
	}
	if constexpr (NOHTTP)
	{
		sb << " NOHTTP";
	}
	else if constexpr (ENFORCE_HTTPS)
	{
		sb << " HTTPS";
	}
	if constexpr (DEBUG)
	{
		sb << " DEBUG";
	}
	return sb.Build();
}

inline ByteString IntroText()
{
	ByteStringBuilder sb;
	sb << "\bl\bU" << APPNAME << "\bU - Version " << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << " - https://powdertoy.co.uk, irc.libera.chat #powder, https://tpt.io/discord\n"
	      "\n"
	      "\n"
		"\bgCtrl+C/V/X 複製,貼上,剪下\n"
		"滑鼠移至右側元素欄中某一個類別可以顯示該類下的所有元素\n"
		"利用滑鼠左/右鍵選取需要的元素\n"
		"使用滑鼠左/右鍵繪製\n"
		"使用Shift+拖動繪製直線\n"
		"使用Ctrl+拖動繪製矩形\n"
		"Ctrl+Shift+單擊填充封閉區域\n"//Ctrl+Shift+click will flood-fill a closed area.
		"使用滑鼠滾輪或'['and']'調整筆刷大小\n"
		"使用滑鼠中鍵或按住Alt使用吸管工具\n"
		"使用Ctrl+Z 撤銷\n"
		"\n\bo按住'Z'鍵開啟放大鏡,使用滑鼠滾輪調整倍數,滑鼠左鍵固定放大區域,固定後可直接在放大區域繪製\n"
		"按下空格鍵以暫停,按下'F'可以幀進\n"
		"按下'S'鍵選擇區域儲存到剪貼簿,按下'L'鍵載入最近儲存的物件,按下'K'鍵瀏覽剪貼簿\n"//Use 'S' to save parts of the window as 'stamps'. 'L' loads the most recent stamp, 'K' shows a library of stamps you saved
		"按下'P'鍵截圖,截圖將被儲存到遊戲目錄下\n"
		"按下'H'開啟/關閉HUD.按下'D'在HUD開啟/關閉除錯模式.\n"
		"\n"
		"貢獻者: \bgStanislaw K Skowronek (最初設計者),\n"
		"\bgSimon Robertshaw, Skresanov Savely, Pilihp64, Catelite, Victoria Hoyle, Nathan Cousins, jacksonmj,\n"
		"\bgFelix Wallin, Lieuwe Mosch, Anthony Boot, Me4502, MaksProg, jacob1, mniip, LBPHacker\n"
		"\n"
		"\bo漢化: \bgDragonRSTER\n"
		"\bo程式: \bgDragonRSTER Github:\br https://github.com/Dragonrster/The-Powder-Toy-Chinese\n"
		"\n";
	if constexpr (BETA)
	{
		sb << "\brThis is a BETA, you cannot save things publicly, nor open local saves and stamps made with it in older versions.\n"
		      "\brIf you are planning on publishing any saves, use the release version.\n";
	}
	else
	{
		sb << "\bg如需使用儲存等線上功能,需要在以下位置註冊: \brpowdertoy.co.uk/Register.html\n";
	}
	sb << "\n\bt" << VersionInfo();
	return sb.Build();
}
