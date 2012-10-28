#pragma once

#include "sqlite3.h"
#include <string>

class TS3Settings
{
private:
	sqlite3* settings;

	inline bool CheckAndHandle(int returnCode);
	bool GetValueForStatement(sqlite3_stmt* statement, std::string& result);
public:
	TS3Settings(void);
	~TS3Settings(void);

	bool OpenDatabase(std::string path);
	void CloseDatabase();

	static std::string GetValueFromData(std::string data, std::string key);

	/* Queries */
	bool GetIconPack(std::string& result);
	bool GetSoundPack(std::string& result);
	bool GetDefaultCaptureProfile(std::string& result);
	bool GetPreProcessorData(std::string profile, std::string& result);
};

