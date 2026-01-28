#pragma once
#include "const.h"

struct SectionInfo {
	SectionInfo() = default;
	~SectionInfo() = default;
	
	SectionInfo(const SectionInfo& src)
	{
		_section_datas = src._section_datas;
	}

	SectionInfo& operator=(const SectionInfo& src)
	{
		if (&src == this) return *this;	//can't self copy

		_section_datas = src._section_datas;

		return *this;
	}

	std::map<std::string, std::string> _section_datas;

	std::string operator[](const std::string& key)
	{
		auto iter = _section_datas.find(key);
		if (iter != _section_datas.end())
		{
			return iter->second;
		}
		return "";
	}
};

class ConfigMgr {
public:
	static ConfigMgr& GetInstance() {
		static ConfigMgr instance;
		return instance;
	}
	
	~ConfigMgr() {
		_config_map.clear();
	}

	SectionInfo operator[](const std::string& section) {
		if (_config_map.find(section) == _config_map.end()) {
			return SectionInfo();
		}
		return _config_map[section];
	}

	ConfigMgr& operator=(const ConfigMgr& src) = delete;

	ConfigMgr(const ConfigMgr& src) = delete;

private:
	ConfigMgr();

private:

	// ´æ´¢sectionºÍkey-value¶ÔµÄmap  
	std::map<std::string, SectionInfo> _config_map;
};

