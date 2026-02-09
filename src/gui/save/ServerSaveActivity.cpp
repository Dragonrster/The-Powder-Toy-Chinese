#include "ServerSaveActivity.h"
#include "graphics/Graphics.h"
#include "graphics/VideoBuffer.h"
#include "gui/interface/Label.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Button.h"
#include "gui/interface/Checkbox.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/SaveIDMessage.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/InformationMessage.h"
#include "client/Client.h"
#include "client/ThumbnailRendererTask.h"
#include "client/GameSave.h"
#include "client/http/UploadSaveRequest.h"
#include "tasks/Task.h"
#include "gui/Style.h"

class SaveUploadTask : public Task
{
	SaveInfo &save;

	void before() override
	{
	}

	void after() override
	{
	}

	bool doWork() override
	{
		notifyProgress(-1);
		auto uploadSaveRequest = std::make_unique<http::UploadSaveRequest>(save);
		uploadSaveRequest->Start();
		uploadSaveRequest->Wait();
		try
		{
			save.SetID(uploadSaveRequest->Finish());
		}
		catch (const http::RequestError &ex)
		{
			notifyError(ByteString(ex.what()).FromUtf8());
			return false;
		}
		return true;
	}

public:
	SaveUploadTask(SaveInfo &newSave) : save(newSave)
	{
	}
};

ServerSaveActivity::ServerSaveActivity(std::unique_ptr<SaveInfo> newSave, OnUploaded onUploaded_) :
	WindowActivity(ui::Point(-1, -1), ui::Point(440, 200)),
	thumbnailRenderer(nullptr),
	save(std::move(newSave)),
	onUploaded(onUploaded_),
	saveUploadTask(nullptr)
{
	titleLabel = new ui::Label(ui::Point(4, 5), ui::Point((Size.X / 2) - 8, 16), "");
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);
	CheckName(save->GetName()); // set titleLabel text

	ui::Label *previewLabel = new ui::Label(ui::Point((Size.X / 2) + 4, 5), ui::Point((Size.X / 2) - 8, 16), ByteString("预览").FromUtf8());
	previewLabel->SetTextColour(style::Colour::InformationTitle);
	previewLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	previewLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(previewLabel);

	nameField = new ui::Textbox(ui::Point(8, 25), ui::Point((Size.X / 2) - 16, 16), save->GetName(), ByteString("[沙盘名称]").FromUtf8());
	nameField->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	nameField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	nameField->SetActionCallback({ [this] { CheckName(nameField->GetText()); } });
	nameField->SetLimit(50);
	AddComponent(nameField);
	FocusComponent(nameField);

	descriptionField = new ui::Textbox(ui::Point(8, 65), ui::Point((Size.X / 2) - 16, Size.Y - (65 + 16 + 4)), save->GetDescription(), ByteString("[沙盘描述]").FromUtf8());
	descriptionField->SetMultiline(true);
	descriptionField->SetLimit(254);
	descriptionField->Appearance.VerticalAlign = ui::Appearance::AlignTop;
	descriptionField->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	AddComponent(descriptionField);

	publishedCheckbox = new ui::Checkbox(ui::Point(8, 45), ui::Point((Size.X/2)-80, 16), ByteString("公开").FromUtf8(), "");
	auto user = Client::Ref().GetAuthUser();
	if (!(user && user->Username == save->GetUserName()))
	{
		// Save is not owned by the user, disable by default
		publishedCheckbox->SetChecked(false);
	}
	else
	{
		// Save belongs to the current user, use published state already set
		publishedCheckbox->SetChecked(save->GetPublished());
	}
	AddComponent(publishedCheckbox);

	pausedCheckbox = new ui::Checkbox(ui::Point(130, 45), ui::Point(55, 16), ByteString("启动时暂停").FromUtf8(), "");
	pausedCheckbox->SetChecked(save->GetGameSave()->paused);
	AddComponent(pausedCheckbox);

	ui::Button *cancelButton = new ui::Button(ui::Point(0, Size.Y - 16), ui::Point((Size.X / 2) - 75, 16), ByteString("取消").FromUtf8());
	cancelButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	cancelButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	cancelButton->Appearance.BorderInactive = ui::Colour(200, 200, 200);
	cancelButton->SetActionCallback({[this]
									 {
										 Exit();
									 }});
	AddComponent(cancelButton);
	SetCancelButton(cancelButton);

	ui::Button *okayButton = new ui::Button(ui::Point((Size.X / 2) - 76, Size.Y - 16), ui::Point(76, 16), ByteString("保存").FromUtf8());
	okayButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	okayButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	okayButton->Appearance.TextInactive = style::Colour::InformationTitle;
	okayButton->SetActionCallback({[this]
								   {
									   Save();
								   }});
	AddComponent(okayButton);
	SetOkayButton(okayButton);

	ui::Button *PublishingInfoButton = new ui::Button(ui::Point((Size.X * 3 / 4) - 75, Size.Y - 42), ui::Point(150, 16), ByteString("发布须知").FromUtf8());
	PublishingInfoButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	PublishingInfoButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	PublishingInfoButton->Appearance.TextInactive = style::Colour::InformationTitle;
	PublishingInfoButton->SetActionCallback({[this]
											 {
												 ShowPublishingInfo();
											 }});
	AddComponent(PublishingInfoButton);

	ui::Button *RulesButton = new ui::Button(ui::Point((Size.X * 3 / 4) - 75, Size.Y - 22), ui::Point(150, 16), ByteString("沙盘上传须知").FromUtf8());
	RulesButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	RulesButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	RulesButton->Appearance.TextInactive = style::Colour::InformationTitle;
	RulesButton->SetActionCallback({[this]
									{
										ShowRules();
									}});
	AddComponent(RulesButton);

	if (save->GetGameSave())
	{
		thumbnailRenderer = new ThumbnailRendererTask(*save->GetGameSave(), Size / 2 - Vec2(16, 16), RendererSettings::decorationAntiClickbait, true);
		thumbnailRenderer->Start();
	}
}

ServerSaveActivity::ServerSaveActivity(std::unique_ptr<SaveInfo> newSave, bool saveNow, OnUploaded onUploaded_) :
	WindowActivity(ui::Point(-1, -1), ui::Point(200, 50)),
	thumbnailRenderer(nullptr),
	save(std::move(newSave)),
	onUploaded(onUploaded_),
	saveUploadTask(nullptr)
{
	ui::Label *titleLabel = new ui::Label(ui::Point(0, 0), Size, ByteString("正在保存到云端...").FromUtf8());
	titleLabel->SetTextColour(style::Colour::InformationTitle);
	titleLabel->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
	titleLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	AddComponent(titleLabel);

	AddAuthorInfo();

	saveUploadTask = new SaveUploadTask(*this->save);
	saveUploadTask->AddTaskListener(this);
	saveUploadTask->Start();
}

void ServerSaveActivity::NotifyDone(Task *task)
{
	if (!task->GetSuccess())
	{
		Exit();
		new ErrorMessage(ByteString("错误").FromUtf8(), task->GetError());
	}
	else
	{
		if (onUploaded)
		{
			onUploaded(std::move(save));
		}
		Exit();
	}
}

void ServerSaveActivity::Save()
{
	if (!nameField->GetText().length())
	{
		new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("必须为沙盘指定一个名称").FromUtf8());
		return;
	}
	auto user = Client::Ref().GetAuthUser();
	if (!(user && user->Username == save->GetUserName()) && publishedCheckbox->GetChecked())
	{
		new ConfirmPrompt(ByteString("发布").FromUtf8(), ByteString("此沙盘由").FromUtf8() + save->GetUserName().FromUtf8() + ByteString("发布,即将以自己的名义发布此沙盘:如没有获得授予,请取消选中发布框,否则继续").FromUtf8(), {[this]
																																																								  {
																																																									  saveUpload();
																																																								  }});
	}
	else
	{
		saveUpload();
	}
}

void ServerSaveActivity::AddAuthorInfo()
{
	Bson serverSaveInfo;
	serverSaveInfo["type"] = "save";
	serverSaveInfo["id"] = save->GetID();
	auto user = Client::Ref().GetAuthUser();
	serverSaveInfo["username"] = user ? user->Username : ByteString("");
	serverSaveInfo["title"] = save->GetName().ToUtf8();
	serverSaveInfo["description"] = save->GetDescription().ToUtf8();
	serverSaveInfo["published"] = (int)save->GetPublished();
	serverSaveInfo["date"] = int64_t(time(nullptr));
	Client::Ref().SaveAuthorInfo(serverSaveInfo);
	{
		auto gameSave = save->TakeGameSave();
		gameSave->authors = serverSaveInfo;
		save->SetGameSave(std::move(gameSave));
	}
}

void ServerSaveActivity::saveUpload()
{
	okayButton->Enabled = false;
	save->SetName(nameField->GetText());
	save->SetDescription(descriptionField->GetText());
	save->SetPublished(publishedCheckbox->GetChecked());
	auto user = Client::Ref().GetAuthUser();
	save->SetUserName(user ? user->Username : ByteString(""));
	save->SetID(0);
	{
		auto gameSave = save->TakeGameSave();
		gameSave->paused = pausedCheckbox->GetChecked();
		save->SetGameSave(std::move(gameSave));
	}
	AddAuthorInfo();
	uploadSaveRequest = std::make_unique<http::UploadSaveRequest>(*save);
	uploadSaveRequest->Start();
}

void ServerSaveActivity::Exit()
{
	WindowActivity::Exit();
}

void ServerSaveActivity::ShowPublishingInfo()
{
	// String info =
	// 	"In The Powder Toy, one can save simulations to their account in two privacy levels: Published and unpublished. You can choose which one by checking or unchecking the 'publish' checkbox. Saves are unpublished by default, so if you do not check publish nobody will be able to see your saves.\n"
	// 	"\n"
	// 	"\btPublished saves\bw will appear on the 'By Date' feed and will be seen by many people. These saves also contribute to your Average Score, which is displayed publicly on your profile page on the website. Publish saves that you want people to see so they can comment and vote on.\n"
	// 	"\btUnpublished saves\bw will not be shown on the 'By Date' feed. These will not contribute to your Average Score. They are not completely private though, as anyone who knows the save id will be able to view it. You can give the save id out to show specific people the save but not allow just everyone to see it.\n"
	// 	"\n"
	// 	"To quickly resave a save, open it and click the left side of the split resave button to \bt'Reupload the current simulation'\bw. If you want to change the description or change the published status, you can click the right side to \bt'Modify simulation properties'\bw. Note that you can't change the name of saves; this will create an entirely new save with no comments, votes, or tags; separate from the original.\n"
	// 	"You may want to publish an unpublished save after it is finished, or to unpublish some currently published ones. You can do this by opening the save, selecting the 'Modify simulation properties' button, and changing the published status there. You can also \btunpublish or delete saves\bw by selecting them in the 'my own' section of the browser and clicking either one of the buttons that appear on bottom.\n"
	// 	"If a save is under a week old and gains popularity fast, it will be automatically placed on the \btfront page\bw. Only published saves will be able to get here. Moderators can also choose to promote any save onto the front page, but this happens rarely. They can also demote any save from the front page that breaks a rule or they feel doesn't belong.\n"
	// 	"Once you make a save, you can resave it as many times as you want. A short previous \btsave history\bw is saved, just right click any save in the save browser and select 'View History' to view it. This is useful for when you accidentally save something you didn't mean to and want to go back to the old version.\n";

	new InformationMessage(ByteString("发布须知").FromUtf8(), ByteString("在TPT中，可以使用两种方式上传沙盘:\n公开和私人，通过选择(默认关闭)是否公开来设置，私人沙盘只能由本人或沙盘ID访问。\n\n\n\bt公开沙盘\bw会立即被使用按日期排布沙盘的人看到，公开沙盘后沙盘的评分将影响个人平均评分(显示在个人档案上)。公开的沙盘能被所有人评论和评分。\n\n\bt私人沙盘\bw不会出现在云沙盘更新中，这些沙盘也不会影响平均评分，尽管设置为私人，但别人仍然可以通过沙盘ID来访问他们。\n\n\n打开沙盘井点击沙盘名左侧的按钮\bt“重新上传当前沙盘”\bw，可以快速更改已经上传的沙盘，如果需更改沙盘描述或沙盘属性，可以点击右侧按钮\bt“修改沙盘属性”\bw，注意不能更改沙盘名称，这会重新创建一个全新的沙盘,如需发布一个设置为私人的沙盘，或将以前发布过的沙盘设置为私人，打开沙盘，选择“修改沙盘属性”按钮，在弹出的对话框中修改属性，在“个人沙盘”页面中选择所需要的沙盘来\bt删除沙盘或转为私人\bw。\n\n如果沙盘已经发布，并且短时间内拥有较高人气，它会自动出现在\bt首页(FP)\bw上。只有设置为公开的沙盘才有机会登上首页(FP)，版主同样有权限使某个沙盘在首页上显示，但这种情况非常少见，如果某沙盘被认为触犯了条例或不适合放在首页，版主同样有权限撤下它。\n\n沙盘发布后，可以不限次数的修改，服务器会保存\bt沙盘历史\bw，在沙盘浏览器中右键沙盘井选择“查看历史”就能找到它，这一功能可以帮助你找回以前的版本并修复错误。").FromUtf8(), true);
}

void ServerSaveActivity::ShowRules()
{
	String rules =
		"\boSection S：社交与社区规则\n"
		"\bw在与社区互动时，你需要遵守以下几条规则。这些规则由工作人员执行，任何违反规则的行为都可能被其他用户举报至我们注意。本节规则适用于上传的存档、评论区、论坛以及社区其他区域。\n"
		"\n"
		"\bt1. 尽量使用正确的语法。\bw 英语是社区的官方语言，但在地区性或文化相关的群组中不强制要求。如果你的英语不够好，建议使用 Google 翻译。\n"
		"\bt2. 禁止刷屏（spam）。\bw 这里没有统一的标准定义，但通常很容易判断。此外，以下行为被视为刷屏，可能会被隐藏或删除：\n"
		"- 就同一主题发布多个帖子。关于游戏反馈或建议的帖子请尽量合并成一个主题。\n"
		"- 通过回复来顶起很久以前的旧帖。这种行为我们称为“necroposting”或“挖坟”。旧帖内容可能已经过时（修复问题、建议等），建议针对当前情况发新帖。\n"
		"- 在帖子中使用“+1”或其他极短回复。没有必要反复顶帖，这会让真正有价值的回复难以被找到。建设性反馈欢迎使用回复，而“+1”按钮就是用来表示支持的。\n"
		"- 过长、无意义或乱码的评论。重复输入同一个字母、毫无意义的灌水等都属于此类。使用其他语言的正常评论除外。\n"
		"- 过度使用格式。全大写、加粗、斜体适度使用是可以的，但请不要整篇都用这些格式。\n"
		"\bt3. 尽量减少脏话使用。\bw 含有脏话的评论或存档有被删除的风险。这也包括其他语言的脏话。\n"
		"\bt4. 禁止上传色情、冒犯性或其他不适当的内容。\bw\n"
		"- 包括但不限于：性、毒品、种族歧视、过度政治内容，或任何冒犯、侮辱群体的内容。\n"
		"- 用其他语言提及这些话题同样禁止。不要试图绕过此规则。\n"
		"- 禁止发布违反此规则的链接或图片。这也包括个人资料中的链接或文字。\n"
		"\bt5. 禁止为与 The Powder Toy 无关的第三方游戏、网站或其他地方做广告。\bw\n"
		"  - 主要是为了防止有人到处宣传自己的游戏或产品。\n"
		"  - 未经授权或非官方的社区聚集地（如 Discord 等）也禁止宣传。\n"
		"\bt6. 禁止钓鱼/挑衅（trolling）。\bw 和某些规则一样，没有明确定义。反复钓鱼的用户被封禁的概率和封禁时长都会显著高于其他人。\n"
		"\bt7. 禁止冒充他人。\bw 禁止注册与社区内其他用户或其他在线社区用户名字故意相似的账号。\n"
		"\bt8. 禁止公开讨论版主决定或相关问题。\bw 如果你的账号被封禁或内容被移除有异议，请通过私信系统联系版主。其他情况下应避免公开讨论版主行为。\n"
		"\bt9. 禁止越权管理（backseat moderating）。\bw 只有版主才有权决定。用户不应威胁他人会被封禁或违反规则会有什么后果。如有疑虑或发现问题，建议通过“举报”按钮或网站私信系统报告。\n"
		"\bt10. 禁止公开支持违反常见法律的行为。\bw 适用哪国法律不明确，但以下为常见禁止内容（包括但不限于）：\n"
		"- 盗版软件、音乐等\n"
		"- 黑客行为 / 盗取账号\n"
		"- 盗窃 / 诈骗\n"
		"\bt11. 禁止跟踪或骚扰任何用户。\bw 近年来此类问题日益严重，常见形式包括：\n"
		"- “人肉搜索”（doxing）以查找他人住址或真实身份\n"
		"- 在对方明确表示不想联系时仍不断私信\n"
		"- 大量恶意点踩存档\n"
		"- 在他人内容（存档、论坛帖子等）下发表恶意或无意义评论\n"
		"- 煽动一群用户针对某人\n"
		"- 私人争吵或仇恨行为（如评论区吵架、制作仇恨存档等）\n"
		"- 基于宗教、种族等对人群的普遍歧视\n"
		"\n"
		"\boSection G：游戏内规则\n"
		"\bw本节规则主要针对游戏内的行为。虽然 Section S 的规则在游戏内同样适用，但以下规则更专注于游戏内社区互动。\n"
		"\bt1. 禁止冒领他人作品。\bw 包括直接重新上传他人存档或大量使用他人存档内容。允许衍生作品，但必须正确署名。除非原作者明确说明了不同的使用条款，否则默认必须注明原作者。是否为衍生作品取决于创新程度和原创比例。盗用存档将被取消发布或禁用。\n"
		"\bt2. 禁止自刷投票或投票作弊。\bw 指通过创建多个账号为自己或其他人的存档刷票。我们对此规则执行非常严格，成功申诉的案例极少。请确保不同账号不在同一网络环境下投票。所有小号将被永久封禁，主账号将被暂时封禁，受影响的存档将被禁用。\n"
		"\bt3. 不鼓励以任何形式求票。\bw 这样做会导致存档被取消发布直到问题解决。以下行为属于此类：\n"
		"- 带有暗示求赞或求踩的标志（如绿色箭头图案或直接求票文字）\n"
		"- 以投票数量为条件的噱头（如“100票我就做更好版本”），这属于刷票行为（vote farming），任何形式的刷票都不允许。\n"
		"- 以使用存档或其他理由为交换条件求票。\n"
		"\bt4. 禁止刷屏。\bw 如前所述，没有统一标准。以下为游戏内可能被视为刷屏的例子：\n"
		"- 短时间内上传或重复上传高度相似的存档。不要试图绕过系统增加曝光/投票量。包括上传几乎无意义的“垃圾”或“空白”存档，此类存档将被取消发布。\n"
		"- 上传纯文字存档（如公告、求助等）。论坛和评论区已为此类需求提供足够空间，此类存档将被移除出首页。\n"
		"- 上传纯艺术存档并非严格禁止，但可能被降级出首页。我们更希望看到元素多样化、富有创意的用法。缺乏这些因素（如纯装饰存档）通常会被降级。\n"
		"\bt5. 禁止上传色情或其他不适当内容。此类存档将被删除并可能导致封禁。\bw\n"
		"- 包括但不限于：性、毒品、种族歧视、过度政治内容，或任何冒犯、侮辱群体的内容。\n"
		"- 不要试图绕过此规则。任何直接或间接故意提及这些概念的行为都属于违规。\n"
		"- 用其他语言提及这些话题同样禁止。不要尝试绕过规则。\n"
		"- 禁止发布违反此规则的链接或图片。这也包括个人资料中的链接或文字。\n"
		"\bt6. 严格禁止图像描点（image plotting）。\bw 包括使用脚本或任何第三方工具自动生成存档。使用 CGI 等方式生成的存档将被删除，并可能导致封禁。\n"
		"\bt7. 尽量减少 logo 和标牌的使用。\bw 此类存档可能被移除出首页。限制内容包括：\n"
		"- 放置过多 logo\n"
		"- 无实际用途的标牌\n"
		"- 虚假更新或通知标牌\n"
		"- 链接无关存档\n"
		"\bt8. 禁止使用无关或不当的标签。\bw 标签仅用于优化搜索结果，通常应为单个词描述。长句、主观标签可能被删除。不当或冒犯性标签很可能导致封禁。\n"
		"\bt9. 禁止故意制造严重卡顿或崩溃的存档。\bw 如果多数用户反馈该存档导致崩溃或严重卡顿，则适用本规则。此类存档将被移除出首页或禁用。\n"
		"\bt10. 不要滥用举报系统。\bw 发送“烂存档”或乱七八糟的举报理由会浪费时间。除非涉及可能的规则违反或社区问题，否则请勿随意举报。如果认为存档确实违规或有社区危害，请放心举报！善意举报永远不会导致封禁。\n"
		"\bt11. 禁止要求将存档降级或移除出首页。\bw 除非存档违反规则，否则都会留在首页。艺术类存档也没有例外，请勿举报艺术存档。\n"
		"\n"
		"\boSection R：其他\n"
		"\bw版主可根据实际情况自行解释这些规则。不同规则的执行力度不同，有些规则执行较宽松。版主拥有最终解释权，但我们已尽力在此覆盖所有不受欢迎的行为。规则有更新时会在本帖发布通知。\n"
		"\n"
		"违反以上规则可能导致：删除帖子/评论、取消存档发布或禁用、将存档移除出首页，或在较严重情况下进行临时或永久封禁。我们有多种人工和自动手段执行这些规则。不同版主处理时的严厉程度和决定可能不完全一致。\n"
		"\n"
		"如果你对规则的界限有任何疑问，欢迎随时联系版主。";

	new InformationMessage(ByteString("沙盘上传须知").FromUtf8(), rules, true);
}

void ServerSaveActivity::CheckName(String newname)
{
	auto user = Client::Ref().GetAuthUser();
	if (newname.length() && newname == save->GetName() && user && save->GetUserName() == user->Username)
		titleLabel->SetText(ByteString("修改沙盘属性:").FromUtf8());
	else
		titleLabel->SetText(ByteString("保存到云端").FromUtf8());
}

void ServerSaveActivity::OnTick()
{
	if (thumbnailRenderer)
	{
		thumbnailRenderer->Poll();
		if (thumbnailRenderer->GetDone())
		{
			thumbnail = thumbnailRenderer->Finish();
			thumbnailRenderer = nullptr;
		}
	}

	if (uploadSaveRequest && uploadSaveRequest->CheckDone())
	{
		okayButton->Enabled = true;
		try
		{
			save->SetID(uploadSaveRequest->Finish());
			Exit();
			new SaveIDMessage(save->GetID());
			if (onUploaded)
			{
				onUploaded(std::move(save));
			}
		}
		catch (const http::RequestError &ex)
		{
			new ErrorMessage(ByteString("错误").FromUtf8(), ByteString("上传失败:\n").FromUtf8() + ByteString(ex.what()).FromUtf8());
		}
		uploadSaveRequest.reset();
	}

	if (saveUploadTask)
		saveUploadTask->Poll();
}

void ServerSaveActivity::OnDraw()
{
	Graphics *g = GetGraphics();
	g->BlendRGBAImage(saveToServerImage->data(), RectSized(Vec2(-10, 0), saveToServerImage->Size()));
	g->DrawFilledRect(RectSized(Position, Size).Inset(-1), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);

	if (Size.X > 220)
		g->DrawLine(Position + Vec2(Size.X / 2 - 1, 0), Position + Vec2(Size.X / 2 - 1, Size.Y - 1), 0xFFFFFF_rgb);

	if (thumbnail)
	{
		auto rect = RectSized(Position + Vec2(Size.X / 2 + (Size.X / 2 - thumbnail->Size().X) / 2, 25), thumbnail->Size());
		g->BlendImage(thumbnail->Data(), 0xFF, rect);
		g->DrawRect(rect, 0xB4B4B4_rgb);
	}
}

ServerSaveActivity::~ServerSaveActivity()
{
	if (thumbnailRenderer)
	{
		thumbnailRenderer->Abandon();
	}
	delete saveUploadTask;
}
