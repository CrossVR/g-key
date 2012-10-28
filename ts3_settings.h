#pragma once

#include "sqlite3.h"
#include <string>
#include <vector>

class TS3Settings
{
private:
	sqlite3* settings;

	inline bool CheckAndLog(int returnCode);
	bool GetValueFromQuery(std::string query, std::string& result);
	bool GetValuesFromQuery(std::string query, std::vector<std::string>& result);
public:
	TS3Settings(void);
	~TS3Settings(void);

	bool OpenDatabase(std::string path);
	void CloseDatabase();

	static std::string GetValueFromData(std::string data, std::string key);

	/* Queries */
	bool GetIconPack(std::string& result);
	bool GetSoundPack(std::string& result);
	bool GetPreProcessorData(std::string profile, std::string& result);
	bool GetEnabledPlugins(std::vector<std::string>& result);
};

