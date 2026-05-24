#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include <string>
#include <vector>
#include <map>
#include "json/json.h"

class Simulation;

class Translations
{
public:
	static Translations& Ref();

	bool LoadLanguageFile(const std::string& filepath);

	std::string Lookup(const std::string& section, const std::string& key) const;

	// Convenience: single-key lookup in "ui" section
	std::string Tr(const std::string& key) const;

	void ApplyElementOverrides(Simulation* sim);
	void ApplyWallOverrides();
	void ApplyMenuOverrides();

	std::string GetIntroText(const char* fallback) const;
	std::string QuickMenuName(const std::string& key) const;

	std::string CurrentLanguage() const;
	bool IsLoaded() const { return loaded_; }

private:
	Translations() = default;

	Json::Value root_;
	bool loaded_ = false;
	std::string langCode_;
	std::map<std::string, std::string> uiMap_;

	std::string FindI18nDir() const;
	static std::string ReadFile(const std::string& path);
	static const Json::Value* GetNested(const Json::Value& root,
	                                     const std::string& section,
	                                     const std::string& key);
};

// Short macro for UI string lookups — key IS the English fallback
#define TrUI(key) Translations::Ref().Tr(key)

#endif
