 #pragma once
 #include "common/String.h"
 #include <map>
 #include <string>
 
// 简单的全局本地化管理器：
// - 根据当前语言索引加载内嵌的 JSON 语言资源
// - 提供 Tr(key[, fallback]) 获取翻译
 class Localization
 {
 public:
 	static Localization &Ref();
 
 	// 设置当前语言索引（与 GlobalPrefs::Ref().Get("Language") 对应）
 	void SetLanguageIndex(int index);
 
	// 获取翻译：不存在则返回 fallback
	String Tr(const char *key, const char *fallback) const;
	// 获取翻译：不存在则返回空字符串
	String Tr(const char *key) const;
 
 private:
 	Localization() = default;
 	void LoadLanguage(int index);
 	void Clear();
 
 	int currentIndex{-1};
 	std::map<std::string, String> entries;
 };

