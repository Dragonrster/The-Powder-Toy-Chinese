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
	      "\bg按 \bo'F1'\bg 显示或隐藏此帮助文本.\n"
	      "\n"
	      "鼠标移至右侧元素栏中某一个类别可以显示该类下的所有元素\n"
	      "利用\bo鼠标左/右键\bg从菜单中选取需要的元素.\n"
	      "使用鼠标左/右键绘制\n"
	      "\n"
	      "使用\bo鼠标滚轮\bg,或 \bo'['\bg 和 \bo']'\bg 调整笔刷大小. 按 \boTab\bg 切换笔刷形状.\n"
	      "\bo鼠标中键\bg 或 \boAlt+单击\bg 可吸取(采样)粒子.\n"
	      "\boCtrl+C/V/X\bg 分别为复制,粘贴,剪切.\n"
	      "粘贴时,按 \bo'R'\bg 旋转, \boShift+R\bg 和 \boShift+Ctrl+R\bg 垂直或水平镜像.\n"
	      "\boShift+拖动\bg 绘制直线. \boShift+Alt+拖动\bg 绘制水平,垂直或对角线.\n"
	      "\boCtrl+拖动\bg 绘制填充矩形. \boCtrl+Alt+拖动\bg 绘制填充正方形. \boCtrl+Shift+单击\bg 填充封闭区域.\n"
	      "\n"
	      "\bo空格键\bg 暂停物理模拟. 按 \bo'F'\bg 单帧步进, \bo'F5'\bg 重新加载模拟.\n"
	      "\boCtrl+Z\bg 撤销, \boCtrl+Y\bg 或 \boCtrl+Shift+Z\bg 重做.\n"
	      "按 \bo'S'\bg 将窗口部分区域保存为'图章'. \bo'L'\bg 加载最近的图章, \bo'K'\bg 浏览已保存的图章库.\n"
	      "\n"
	      "按 \bo0-9\bg 选择显示模式.\n"
	      "按 \bo'H'\bg 开关HUD. 按 \bo'D'\bg 在HUD中开关调试模式.\n"
	      "按 \bo'Z'\bg 使用放大镜工具. 单击可固定放大区域. 用滚轮调整放大倍数.\n"
	      "按 \boCtrl+F\bg 在屏幕上高亮选中的元素.\n"
	      "\n";
	if constexpr (BETA)
	{
		sb << "\br这是一个 BETA 版本,您无法公开保存任何内容,也无法打开较旧版本中使用它创建的本地保存和贴图.\n"
		      "\br如果您计划发布任何保存,请使用发布版本.\n";
	}
	else
	{
		sb << "\bg如需使用保存等在线功能,需要在以下位置注册: \br" << SERVER << "/Register.html\n";
	}
	sb << "\n\bt" << VersionInfo();
	return sb.Build();
}
