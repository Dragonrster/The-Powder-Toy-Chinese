#include "TagsView.h"

#include "TagsController.h"
#include "TagsModel.h"
#include "TagsModelException.h"

#include "client/Client.h"
#include "client/SaveInfo.h"

#include "graphics/Graphics.h"

#include "gui/dialogues/ErrorMessage.h"
#include "gui/interface/Button.h"
#include "gui/interface/Textbox.h"
#include "gui/interface/Label.h"

#include <SDL.h>

TagsView::TagsView():
	ui::Window(ui::Point(-1, -1), ui::Point(195, 250))
{
	closeButton = new ui::Button(ui::Point(0, Size.Y-16), ui::Point(195, 16), ByteString("取消").FromUtf8());
	closeButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	closeButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	closeButton->SetActionCallback({ [this] { c->Exit(); } });
	AddComponent(closeButton);
	SetCancelButton(closeButton);

	tagInput = new ui::Textbox(ui::Point(8, Size.Y-40), ui::Point(Size.X-60, 16), "",  ByteString("[新标签]").FromUtf8());
	tagInput->Appearance.icon = IconTag;
	tagInput->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	tagInput->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	tagInput->SetLimit(16);
	AddComponent(tagInput);

	addButton = new ui::Button(ui::Point(tagInput->Position.X+tagInput->Size.X+4, tagInput->Position.Y), ui::Point(40, 16), "Add");
	addButton->Appearance.icon = IconAdd;
	addButton->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	addButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
	addButton->SetActionCallback({ [this] { addTag(); } });
	AddComponent(addButton);

	if (!Client::Ref().GetAuthUser().UserID)
		addButton->Enabled = false;

	title = new ui::Label(ui::Point(5, 5), ui::Point(185, 28),  ByteString("管理标签:\n\bg标签仅用于提升搜索效率").FromUtf8());
	title->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;
	title->Appearance.VerticalAlign = ui::Appearance::AlignTop;
	title->SetMultiline(true);
	AddComponent(title);
}

void TagsView::OnTick()
{
	c->Tick();
}

void TagsView::OnDraw()
{
	Graphics * g = GetGraphics();
	g->DrawFilledRect(RectSized(Position - Vec2{ 1, 1 }, Size + Vec2{ 2, 2 }), 0x000000_rgb);
	g->DrawRect(RectSized(Position, Size), 0xFFFFFF_rgb);
}

void TagsView::NotifyTagsChanged(TagsModel * sender)
{
	for (size_t i = 0; i < tags.size(); i++)
	{
		RemoveComponent(tags[i]);
		delete tags[i];
	}
	tags.clear();
	
	if(sender->GetSave())
	{
		std::list<ByteString> Tags = sender->GetSave()->GetTags();
		int i = 0;
		for (auto &tag : Tags)
		{
			ui::Label * tempLabel = new ui::Label(ui::Point(35, 35+(16*i)), ui::Point(120, 16), tag.FromUtf8());
			tempLabel->Appearance.HorizontalAlign = ui::Appearance::AlignLeft;			tempLabel->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
			tags.push_back(tempLabel);
			AddComponent(tempLabel);

			if(sender->GetSave()->GetUserName() == Client::Ref().GetAuthUser().Username || Client::Ref().GetAuthUser().UserElevation == User::ElevationAdmin || Client::Ref().GetAuthUser().UserElevation == User::ElevationMod)
			{
				ui::Button * tempButton = new ui::Button(ui::Point(15, 37+(16*i)), ui::Point(11, 12));
				tempButton->Appearance.icon = IconDelete;
				tempButton->Appearance.Border = ui::Border(0);
				tempButton->Appearance.Margin.Top += 2;
				tempButton->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
				tempButton->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
				tempButton->SetActionCallback({ [this, tag] {
					c->RemoveTag(tag);
				} });
				tags.push_back(tempButton);
				AddComponent(tempButton);
			}
			i++;
		}
	}
}

void TagsView::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (repeat)
		return;
	switch(key)
	{
	case SDLK_KP_ENTER:
	case SDLK_RETURN:
		if(IsFocused(tagInput))
		{
			addTag();
		}
		break;
	}
}

void TagsView::addTag()
{
	if (tagInput->GetText().length() < 4)
	{
		new ErrorMessage( ByteString("标签长度过短").FromUtf8(), ByteString("标签长度至少为4字节").FromUtf8());
		return;
	}
	c->AddTag(tagInput->GetText().ToUtf8());
	tagInput->SetText("");
}
