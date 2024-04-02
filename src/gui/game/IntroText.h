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
		"\bgCtrl+C/V/X 复制,粘贴,剪切\n"
		"鼠标移至右侧元素栏中某一个类别可以显示该类下的所有元素\n"
		"利用鼠标左/右键选取需要的元素\n"
		"使用鼠标左/右键绘制\n"
		"使用Shift+拖动绘制直线\n"
		"使用Ctrl+拖动绘制矩形\n"
		"Ctrl+Shift+单击填充封闭区域\n"//Ctrl+Shift+click will flood-fill a closed area.
		"使用鼠标滚轮或'['and']'调整笔刷大小\n"
		"使用鼠标中键或按住Alt使用吸管工具\n"
		"使用Ctrl+Z 撤销\n"
		"\n\bo按住'Z'键打开放大镜,使用鼠标滚轮调整倍数,鼠标左键固定放大区域,固定后可直接在放大区域绘制\n"
		"按下空格键以暂停,按下'F'可以帧进\n"
		"按下'S'键选择区域保存到剪贴板,按下'L'键加载最近保存的对象,按下'K'键浏览剪贴板\n"//Use 'S' to save parts of the window as 'stamps'. 'L' loads the most recent stamp, 'K' shows a library of stamps you saved
		"按下'P'键截图,截图将被保存到游戏目录下\n"
		"按下'H'打开/关闭HUD.按下'D'在HUD打开/关闭调试模式.\n"
		"\n"
		"贡献者: \bgStanislaw K Skowronek (最初设计者),\n"
		"\bgSimon Robertshaw, Skresanov Savely, cracker64, Catelite, Victoria Hoyle, Nathan Cousins, jacksonmj,\n"
		"\bgFelix Wallin, Lieuwe Mosch, Anthony Boot, Me4502, MaksProg, jacob1, mniip, LBPHacker\n"
		"\n"
		"\bo汉化: \bgDragonRSTER\n"
		"\bo程序: \bgDragonRSTER Github:\br https://github.com/Dragonrster/The-Powder-Toy-Chinese\n"
		"\n";
	if constexpr (BETA)
	{
		sb << "\brThis is a BETA, you cannot save things publicly, nor open local saves and stamps made with it in older versions.\n"
		      "\brIf you are planning on publishing any saves, use the release version.\n";
	}
	else
	{
		sb << "\bg如需使用保存等在线功能,需要在以下位置注册: \brhttps://powdertoy.co.uk/Register.html\n";
	}
	sb << "\n\bt" << VersionInfo();
	return sb.Build();
}
