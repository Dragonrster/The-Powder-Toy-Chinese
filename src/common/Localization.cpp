#include "Localization.h"
#include <cctype>
#include <cstring>
#include <span>
#include "lang_zh_CN_json.h"
#include "lang_en_US_json.h"

static void ParseSimpleJsonKV(std::span<const char> text,
                              std::map<std::string, String> &out);
 
 Localization &Localization::Ref()
 {
 	static Localization inst;
 	return inst;
 }

 Localization::Localization()
 {
 	LoadFallbackEnglish();
 }
 
 void Localization::Clear()
 {
 	entries.clear();
 }

 void Localization::LoadFallbackEnglish()
 {
 	if (!fallbackEn.empty())
 		return;
 	std::span<const char> data = lang_en_US_json.AsCharSpan();
 	ParseSimpleJsonKV(data, fallbackEn);
 }
 
// 将 JSON 中的 \b+字母 转为渲染器识别的退格符(0x08)+字母（与 C++ 里 "\bg" 等一致）
static void ConvertBackslashBFormatCodes(std::string &s)
{
	const char *formatLetters = "wgorlbtuU";
	for (size_t i = 0; i + 2 <= s.size(); )
	{
		if ((unsigned char)s[i] == 0x5C && (unsigned char)s[i + 1] == 0x62)
		{
			char c = s[i + 2];
			if (c && strchr(formatLetters, c))
			{
				s[i] = '\b';
				s[i + 1] = c;
				s.erase(i + 2, 1);
				i += 2;
				continue;
			}
		}
		++i;
	}
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
 				char next = text[i];
 				if (next == '\\')
 					key.push_back('\\');
 				else if (next == '"')
 					key.push_back('"');
 				else
 					{ key.push_back('\\'); key.push_back(next); }
 				++i;
 			}
 			else
 			{
 				key.push_back(text[i]);
 				++i;
 			}
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
 				char next = text[i];
 				if (next == '\\')
 					val.push_back('\\');
 				else if (next == '"')
 					val.push_back('"');
 				else if (next == 'n')
 					val.push_back('\n');
 				else if (next == 't')
 					val.push_back('\t');
 				else if (next == 'r')
 					val.push_back('\r');
 				else if (next == 'b' && i + 1 < text.size())
 				{
 					char code = text[i + 1];
 					if (code == 'w' || code == 'g' || code == 'o' || code == 'r' || code == 'l' || code == 'b' || code == 't' || code == 'u' || code == 'U')
 					{
 						val.push_back('\b');
 						val.push_back(code);
 						++i;
 					}
 					else
 						{ val.push_back('\\'); val.push_back(next); }
 				}
 				else
 					{ val.push_back('\\'); val.push_back(next); }
 				++i;
 			}
 			else
 			{
 				val.push_back(text[i]);
 				++i;
 			}
 		}
 		if (i < text.size() && text[i] == '"')
 			++i;
 
 		ConvertBackslashBFormatCodes(val);
 		out[key] = ByteString(val).FromUtf8();
 
 		skipSpace();
 		if (i < text.size() && text[i] == ',')
 			++i;
 	}
 }
 
void Localization::LoadLanguage(int index)
{
	LoadFallbackEnglish();
	Clear();

	// 0: 简体中文, 1: 英文；直接使用编译期嵌入的资源
	std::span<const char> data;
	switch (index)
	{
	case 0:
		data = lang_en_US_json.AsCharSpan();
		break;
	case 1:
		data = lang_zh_CN_json.AsCharSpan();
		break;
	default:
		data = lang_en_US_json.AsCharSpan();
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
 	auto itEn = fallbackEn.find(key);
 	if (itEn != fallbackEn.end())
 		return itEn->second;
 	if (fallback)
 		return ByteString(fallback).FromUtf8();
 	return String();
 }

