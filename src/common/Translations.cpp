#include "common/Translations.h"
#include "common/EmbeddedZhCN.h"
#include "common/Platform.h"
#include "simulation/Simulation.h"
#include "simulation/Element.h"
#include "simulation/WallNumbers.h"
#include "game/Menus.h"
#include <cstdio>
#include <cstring>

Translations& Translations::Ref()
{
	static Translations instance;
	return instance;
}

std::string Translations::ReadFile(const std::string& path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f)
	{
		printf("[i18n] ReadFile MISS: %s\n", path.c_str());
		return "";
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string buffer(size, '\0');
	fread(&buffer[0], 1, size, f);
	fclose(f);
	printf("[i18n] ReadFile OK: %s (%ld bytes)\n", path.c_str(), size);
	return buffer;
}

std::string Translations::FindI18nDir() const
{
	// Try sharedCwd (cross-session data), originalCwd (app dir), GetCwd()
	const char* candidates[] = {
		Platform::sharedCwd.c_str(),
		Platform::originalCwd.c_str(),
		nullptr,
	};
	for (auto* candidate : candidates)
	{
		std::string dir;
		if (candidate)
			dir = std::string(candidate) + "/i18n";
		else
			dir = Platform::GetCwd() + "/i18n";
		if (Platform::DirectoryExists(dir))
			return dir + "/";
	}
#ifdef ANDROID
	const char* home = getenv("HOME");
	if (home)
	{
		std::string dir = std::string(home) + "/i18n";
		if (Platform::DirectoryExists(dir))
			return dir + "/";
	}
#endif
	return "i18n/";
}

bool Translations::LoadLanguageFile(const std::string& filepath)
{
	std::string jsonText = ReadFile(filepath);
	if (jsonText.empty())
	{
		std::string i18nDir = FindI18nDir();
		jsonText = ReadFile(i18nDir + filepath);
	}
	// Fallback: try base paths directly (Android assets may be flat)
	if (jsonText.empty())
	{
		jsonText = ReadFile(Platform::sharedCwd + "/" + filepath);
	}
	if (jsonText.empty())
	{
		jsonText = ReadFile(Platform::originalCwd + "/" + filepath);
	}
	if (jsonText.empty())
	{
		const char* home = getenv("HOME");
		if (home)
			jsonText = ReadFile(std::string(home) + "/" + filepath);
	}
	if (jsonText.empty())
	{
		jsonText = ReadFile(Platform::GetCwd() + "/" + filepath);
	}
	// Fallback: use embedded Chinese translation compiled into binary
	if (jsonText.empty() && filepath == "zh_CN.json")
	{
		jsonText = EMBEDDED_ZH_CN;
		if (!jsonText.empty())
			printf("[i18n] Using embedded zh_CN translation (%zu bytes)\n", jsonText.size());
	}
	if (jsonText.empty())
		return false;

	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(jsonText.c_str(), jsonText.c_str() + jsonText.size(), root, false))
	{
		printf("[i18n] JSON parse failed!\n");
		return false;
	}

	root_ = root;
	loaded_ = true;

	// Pre-populate UI string map for fast lookup without jsoncpp traversal
	if (root_.isMember("ui"))
	{
		const Json::Value& ui = root_["ui"];
		Json::Value::Members members = ui.getMemberNames();
		for (const auto& key : members)
		{
			const Json::Value& val = ui[key];
			if (val.isString())
				uiMap_[key] = val.asString();
		}
	}

	if (root_.isMember("_meta") && root_["_meta"].isMember("language"))
		langCode_ = root_["_meta"]["language"].asString();

	printf("[i18n] Language loaded: %s, %zu UI strings\n", langCode_.c_str(), uiMap_.size());
	return true;
}

const Json::Value* Translations::GetNested(const Json::Value& root,
                                            const std::string& section,
                                            const std::string& key)
{
	if (!root.isMember(section))
		return nullptr;
	const Json::Value& sec = root[section];
	if (!sec.isMember(key))
		return nullptr;
	return &sec[key];
}

std::string Translations::Lookup(const std::string& section, const std::string& key) const
{
	if (!loaded_)
		return key;
	const Json::Value* found = GetNested(root_, section, key);
	if (!found)
		return key;
	if (!found->isString())
		return key;
	return found->asString();
}

std::string Translations::Tr(const std::string& key) const
{
	if (!loaded_)
		return key;
	auto it = uiMap_.find(key);
	if (it == uiMap_.end())
		return key;
	return it->second;
}

void Translations::ApplyElementOverrides(Simulation* sim)
{
	if (!loaded_ || !root_.isMember("elements"))
		return;
	const Json::Value& elems = root_["elements"];
	for (int t = 0; t < PT_NUM; t++)
	{
		if (!sim->elements[t].Enabled)
			continue;
		const std::string& id = sim->elements[t].Identifier;
		if (elems.isMember(id))
		{
			const Json::Value& e = elems[id];
			// Only override Description — keep original English Name in icons
			if (e.isMember("description"))
			{
				sim->elements[t].Description = e["description"].asString();
				sim->origElements[t].Description = e["description"].asString();
			}
		}
	}
}

void Translations::ApplyWallOverrides()
{
	if (!loaded_ || !root_.isMember("walls"))
		return;
	const Json::Value& walls = root_["walls"];
	for (int i = 0; i < WALLCOUNT; i++)
	{
		const std::string& id = wallTypes[i].identifier;
		if (walls.isMember(id))
		{
			const Json::Value& w = walls[id];
			if (w.isMember("name"))
				wallTypes[i].name = w["name"].asString();
			if (w.isMember("description"))
				wallTypes[i].descs = w["description"].asString();
		}
	}
}

void Translations::ApplyMenuOverrides()
{
	if (!loaded_ || !root_.isMember("menus"))
		return;
	const Json::Value& menus = root_["menus"];

	static const struct { int sc; const char* key; } map[] = {
		{ SC_WALL,      "SC_WALL" },
		{ SC_ELEC,      "SC_ELEC" },
		{ SC_POWERED,   "SC_POWERED" },
		{ SC_SENSOR,    "SC_SENSOR" },
		{ SC_FORCE,     "SC_FORCE" },
		{ SC_EXPLOSIVE, "SC_EXPLOSIVE" },
		{ SC_GAS,       "SC_GAS" },
		{ SC_LIQUID,    "SC_LIQUID" },
		{ SC_POWDERS,   "SC_POWDERS" },
		{ SC_SOLIDS,    "SC_SOLIDS" },
		{ SC_NUCLEAR,   "SC_NUCLEAR" },
		{ SC_SPECIAL,   "SC_SPECIAL" },
		{ SC_LIFE,      "SC_LIFE" },
		{ SC_TOOL,      "SC_TOOL" },
		{ SC_FAV,       "SC_FAV" },
		{ SC_DECO,      "SC_DECO" },
		{ SC_FAV2,      "SC_FAV2" },
		{ SC_HUD,       "SC_HUD" },
		{ SC_OTHER,     "SC_OTHER" },
		{ SC_SEARCH,    "SC_SEARCH" },
	};

	for (auto& m : map)
	{
		if (m.sc < SC_TOTAL && menuSections[m.sc] && menus.isMember(m.key))
			menuSections[m.sc]->name = menus[m.key].asString();
	}
}

std::string Translations::GetIntroText(const char* fallback) const
{
	if (!loaded_ || !root_.isMember("intro_text"))
		return fallback ? std::string(fallback) : "";
	return root_["intro_text"].asString();
}

std::string Translations::QuickMenuName(const std::string& key) const
{
	return Lookup("quickmenu", key);
}

std::string Translations::CurrentLanguage() const
{
	return langCode_;
}
