#include "Localization.h"
#include <cctype>
#include <span>
#include "lang_zh_CN_json.h"
#include "lang_en_US_json.h"
 
 Localization &Localization::Ref()
 {
 	static Localization inst;
 	return inst;
 }
 
 void Localization::Clear()
 {
 	entries.clear();
 }
 
// 非严格 JSON 解析器：只支持 {"k": "v", ...} 这种简单结构
static void ParseSimpleJsonKV(std::span<const char> text,
                               std::map<std::string, String> &out)
 {
 	size_t i = 0;
 	auto skipSpace = [&]() {
		while (i < text.size() && std::isspace(static_cast<unsigned char>(static_cast<unsigned char>(text[i]))))
 			++i;
 	};
 
 	skipSpace();
 	if (i < text.size() && text[i] == '{')
 		++i;
 
 	while (i < text.size())
 	{
 		skipSpace();
 		if (i < text.size() && text[i] == '}')
 			break;
 
 		// 解析 key
 		if (i >= text.size() || text[i] != '"')
 			break;
 		++i;
 		std::string key;
 		while (i < text.size() && text[i] != '"')
 		{
 			if (text[i] == '\\' && i + 1 < text.size())
 			{
 				++i;
 				key.push_back(text[i]);
 			}
 			else
 			{
 				key.push_back(text[i]);
 			}
 			++i;
 		}
 		if (i < text.size() && text[i] == '"')
 			++i;
 
 		skipSpace();
 		if (i >= text.size() || text[i] != ':')
 			break;
 		++i;
 
 		skipSpace();
 		if (i >= text.size() || text[i] != '"')
 			break;
 		++i;
 		std::string val;
 		while (i < text.size() && text[i] != '"')
 		{
 			if (text[i] == '\\' && i + 1 < text.size())
 			{
 				++i;
 				val.push_back(text[i]);
 			}
 			else
 			{
 				val.push_back(text[i]);
 			}
 			++i;
 		}
 		if (i < text.size() && text[i] == '"')
 			++i;
 
 		out[key] = ByteString(val).FromUtf8();
 
 		skipSpace();
 		if (i < text.size() && text[i] == ',')
 			++i;
 	}
 }
 
void Localization::LoadLanguage(int index)
{
	Clear();

	// 0: 简体中文, 1: 英文；直接使用编译期嵌入的资源
	std::span<const char> data;
	switch (index)
	{
	case 0:
		data = lang_zh_CN_json.AsCharSpan();
		break;
	case 1:
		data = lang_en_US_json.AsCharSpan();
		break;
	default:
		data = lang_zh_CN_json.AsCharSpan();
		break;
	}

	ParseSimpleJsonKV(data, entries);
 }
 
 void Localization::SetLanguageIndex(int index)
 {
 	if (index == currentIndex)
 		return;
 	currentIndex = index;
 	LoadLanguage(index);
 }
 
 String Localization::Tr(const char *key, const char *fallback) const
 {
 	auto it = entries.find(key);
 	if (it != entries.end())
 		return it->second;
 	return ByteString(fallback).FromUtf8();
 }

String Localization::Tr(const char *key) const
{
	auto it = entries.find(key);
	if (it != entries.end())
		return it->second;
	return String(); // 不要后备文本：缺失时返回空
}

