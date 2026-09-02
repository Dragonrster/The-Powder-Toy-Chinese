The Powder Toy - 中文版 (基于 100.1.400 SDL3)
==============================================

**这是基于 The Powder Toy 100.1.400 SDL3 版源码制作的完整中文汉化版源码包。**

获取最新版：[Powder Toy 官网](https://powdertoy.co.uk/Download.html) 或 [Steam](https://store.steampowered.com/app/1148350/The_Powder_Toy/)。

要使用保存等在线功能，需要[注册账号](https://powdertoy.co.uk/Register.html)。
你也可以访问[官方 TPT 论坛](https://powdertoy.co.uk/Discussions/Categories/Index.html)。

你是否想过炸掉什么东西？或者一直梦想着操作一座核电站？有没有想过开发自己的 CPU？The Powder Toy 能让你实现这些，甚至更多！

The Powder Toy 是一款免费的物理沙盒游戏，模拟气压、速度、热量、重力以及无数种物质之间的相互作用！游戏提供了各种建筑材料、液体、气体和电子元件，可以用来建造复杂的机器、枪支、炸弹、逼真的地形，几乎任何东西。然后你可以挖掘它们，观看酷炫的爆炸，添加复杂的电路，操控小人，或操作你的机器。你可以浏览和游玩社区创作的上千个存档，也可以上传自己的作品——欢迎你的创作！

游戏内置 Lua API——你可以自动化操作，甚至为游戏制作插件。The Powder Toy 是免费的，源码以 GNU 通用公共许可证发布，因此你可以自行修改游戏或帮助开发。

---

### 关于本汉化版

本源码包在上游 100.1.400 SDL3 源码基础上，完成了以下汉化改造：

- **UI 文本全面汉化**：菜单、按钮、提示、元素名称和描述等全部中文化（817 条中文字面量）
- **底层编码修复**：`String.h` 改用 `FromUtf8()` 正确解码 UTF-8 中文；`String.cpp` 修复四字节 UTF-8 解码（补全 `<<6` 位移）；`0x0F+RGB` 颜色控制序列原样保留
- **中文换行支持**：`TextWrapper.cpp` 新增 `IsCjkWrapBoundary()` 实现中文逐字换行
- **CJK 字体合并**：`font.bz2` 合并了 20,940 个 CJK 字形（zpix 像素字体，12px），覆盖 41,525 个码点
- **目录打开修复**（Android）：`PowderActivity.template.java` 迁移数据目录到 `Android/media/`，支持系统文件管理器直接访问；`OptionsView.cpp` 改用 `DefaultDdir()`
- **服务器错误信息汉化**：`LoginModel.cpp` 错误信息翻译为中文

构建说明
===========================================================================

详见 Wiki 的 [Powder Toy 开发帮助](https://powdertoy.co.uk/Wiki/W/Main_Page.html) 章节。

**快速构建（Android arm64）：**

```
cd /path/to/src
meson setup build-android --cross-file android/cross/aarch64-local.ini
cd build-android
ninja android/powder.unsigned.apk
```

产物为 `build-android/android/powder.unsigned.apk`，需自行签名后安装。

特别致谢
===========================================================================

* Stanislaw K Skowronek - 设计了最初的 Powder Toy
* Simon Robertshaw - 编写了网站，现任服务器所有者
* Skresanov Savely
* Pilihp64
* Catelite
* Victoria Hoyle
* Nathan Cousins
* jacksonmj
* Felix Wallin
* Lieuwe Mosch
* Anthony Boot
* Me4502
* MaksProg
* jacob1
* mniip
* LBPHacker

使用的库和其他资源
===========================================================================

* [bzip2](http://www.bzip.org/)
* [FFTW](http://fftw.org/)
* [JsonCpp](https://github.com/open-source-parsers/jsoncpp)
* [libcurl](https://curl.se/libcurl/)
* [libpng](http://www.libpng.org/pub/png/libpng.html)
* [Lua](https://www.lua.org/)
* [LuaJIT](https://luajit.org/)
* [Mallangche](https://github.com/JammPark/Mallangche)
* [mbedtls](https://www.trustedfirmware.org/projects/mbed-tls/)
* [SDL](https://libsdl.org/)

操作说明
===========================================================================

用鼠标选择元素，在画布上绘制，就像在画图软件里一样。剩下的就是边玩边学。

操作键位
===========================================================================

| 按键                     | 功能                                                              |
| -----------------------  | ----------------------------------------------------------------- |
| TAB                      | 切换圆形/方形/三角形笔刷                                          |
| 空格                     | 暂停                                                              |
| Q / Esc                  | 退出                                                              |
| Z                        | 放大镜                                                            |
| S                        | 保存图章（STK2 模式下配合 Ctrl 使用）                             |
| L                        | 加载最近保存的图章                                                |
| K                        | 图章库                                                            |
| 0-9                      | 设置显示模式                                                      |
| P / F2                   | 截图为 .png 文件                                                  |
| E                        | 打开元素搜索                                                      |
| F                        | 暂停并前进一帧                                                    |
| G                        | 增加网格尺寸                                                      |
| Shift + G                | 减小网格尺寸                                                      |
| H                        | 显示/隐藏 HUD                                                     |
| Ctrl + H / F1            | 显示介绍文本                                                      |
| D / F3                   | 调试模式（STK2 模式下配合 Ctrl 使用）                             |
| I                        | 反转气压和速度图                                                  |
| W                        | 循环切换重力模式（STK2 模式下配合 Ctrl 使用）                     |
| Y                        | 循环切换空气模式                                                  |
| Ctrl + E                 | 循环切换边缘模式                                                  |
| B                        | 进入装饰编辑器菜单                                                |
| Ctrl + B                 | 开关装饰效果                                                      |
| N                        | 开关牛顿引力                                                      |
| U                        | 开关环境热                                                        |
| Ctrl + I                 | 安装 Powder Toy，用于双击加载存档/图章                            |
| 反引号 `                 | 打开/关闭控制台                                                   |
| =                        | 重置气压和速度图                                                  |
| Ctrl + =                 | 重置电力                                                          |
| [                        | 减小笔刷尺寸                                                      |
| ]                        | 增大笔刷尺寸                                                      |
| Alt + [                  | 笔刷尺寸减 1                                                      |
| Alt + ]                  | 笔刷尺寸加 1                                                      |
| Ctrl + C/V/X             | 复制/粘贴/剪切                                                    |
| Ctrl + Z                 | 撤销                                                              |
| Ctrl + Y                 | 重做                                                              |
| Ctrl + 鼠标拖动          | 绘制矩形                                                          |
| Shift + 鼠标拖动         | 绘制直线                                                          |
| 鼠标中键                 | 吸取（采样）元素                                                  |
| Alt + 左键               | 吸取（采样）元素                                                  |
| 鼠标滚轮                 | 调整笔刷尺寸                                                      |
| Ctrl + 鼠标滚轮          | 调整笔刷垂直尺寸                                                  |
| Shift + 鼠标滚轮         | 调整笔刷水平尺寸                                                  |
| Shift + R                | 粘贴图章时水平镜像选中区域                                        |
| Ctrl + Shift + R         | 粘贴图章时垂直镜像选中区域                                        |
| R                        | 粘贴图章时逆时针旋转选中区域                                      |
| F11                      | 切换全屏                                                          |

命令行参数
---------------------------------------------------------------------------

| 命令                    | 说明                              | 示例                                         |
| ----------------------- | --------------------------------- | -------------------------------------------- |
| `scale:SIZE`            | 设置窗口缩放倍率                  | `scale:2`                                    |
| `kiosk`                 | 全屏模式                          |                                              |
| `proxy:SERVER[:PORT]`   | 使用代理服务器                    | `proxy:wwwcache.lancs.ac.uk:8080`            |
| `open FILE`             | 以图章或存档方式打开文件          |                                              |
| `ddir DIRECTORY`        | 用于保存图章和配置的目录          |                                              |
| `ptsave:SAVEID`         | 打开在线存档，用于 ptsave: URL    | `ptsave:2198`                                |
| `disable-network`       | 禁用网络连接                      |                                              |
| `disable-bluescreen`    | 禁用蓝屏处理                      |                                              |
| `redirect`              | 将输出重定向到 stdout.txt/stderr.txt |                                            |
| `console`               | 在 Windows 上将输出重定向到新控制台 |                                              |
| `cafile:CAFILE`         | 设置证书包路径                    | `cafile:/etc/ssl/certs/ca-certificates.crt`  |
| `capath:CAPATH`         | 设置证书目录路径                  | `capath:/etc/ssl/certs`                      |
