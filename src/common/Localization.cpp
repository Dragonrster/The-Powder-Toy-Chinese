#include "Localization.h"
#include <cctype>
#include <cstring>
#include <span>
// 顺序与 LoadLanguage(index) 一致：en, zh-CN, zh-TW, de, es, fr, it, ja, ko, pt, ru, lzh
#include "lang_en_US_json.h"
#include "lang_zh_CN_json.h"
#include "lang_zh_TW_json.h"
#include "lang_de_DE_json.h"
#include "lang_es_ES_json.h"
#include "lang_fr_FR_json.h"
#include "lang_it_IT_json.h"
#include "lang_ja_JP_json.h"
#include "lang_ko_KR_json.h"
#include "lang_pt_BR_json.h"
#include "lang_ru_RU_json.h"
#include "lang_lzh_json.h"

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

	// 0: English, 1: 中文, 2: 繁體中文, 3: Deutsch, 4: Español, 5: Français, 6: Italiano, 7: 日本語, 8: 한국어, 9: Português, 10: Русский, 11: 文言
	std::span<const char> data;
	switch (index)
	{
	case 0:  data = lang_en_US_json.AsCharSpan(); break;
	case 1:  data = lang_zh_CN_json.AsCharSpan(); break;
	case 2:  data = lang_zh_TW_json.AsCharSpan(); break;
	case 3:  data = lang_de_DE_json.AsCharSpan(); break;
	case 4:  data = lang_es_ES_json.AsCharSpan(); break;
	case 5:  data = lang_fr_FR_json.AsCharSpan(); break;
	case 6:  data = lang_it_IT_json.AsCharSpan(); break;
	case 7:  data = lang_ja_JP_json.AsCharSpan(); break;
	case 8:  data = lang_ko_KR_json.AsCharSpan(); break;
	case 9:  data = lang_pt_BR_json.AsCharSpan(); break;
	case 10: data = lang_ru_RU_json.AsCharSpan(); break;
	case 11: data = lang_lzh_json.AsCharSpan(); break;
	default: data = lang_en_US_json.AsCharSpan(); break;
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

