/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#include "ts3_settings.h"
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include "sqlite3.h"
#include <sstream>
#include <string>

TS3Settings::TS3Settings(void)
{
}

TS3Settings::~TS3Settings(void)
{
	CloseDatabase();
}

bool TS3Settings::CheckAndHandle(int returnCode)
{
	if(returnCode != SQLITE_OK) ts3Functions.logMessage(sqlite3_errmsg(settings), LogLevel_ERROR, "G-Key Plugin", 0);
	return returnCode != SQLITE_OK;
}

bool TS3Settings::GetValueForStatement(sqlite3_stmt* statement, std::string& result)
{
	if(sqlite3_step(statement) != SQLITE_ROW) return false;
	if(sqlite3_column_count(statement) != 1) return false;
	if(sqlite3_column_type(statement, 0) != SQLITE_TEXT) return false;
	result = std::string(reinterpret_cast<const char*>(
		sqlite3_column_text(statement, 0)
	));
	if(CheckAndHandle(sqlite3_reset(statement))) return false;
	return true;
}

bool TS3Settings::OpenDatabase(std::string path)
{
	if(settings != NULL) CloseDatabase();

	if(CheckAndHandle(sqlite3_open(path.c_str(), &settings)))
		return false;

	return true;
}

void TS3Settings::CloseDatabase()
{
	sqlite3_close(settings);
}

std::string TS3Settings::GetValueFromData(std::string data, std::string key)
{
	std::string result;
	std::stringstream ss(data);
	bool found = false;
	while(!ss.eof() && !found)
	{
		getline(ss, result, '=');
		found = result == key;
		getline(ss, result);
	}
	return found?result:std::string();
}

bool TS3Settings::GetIconPack(std::string& result)
{
	sqlite3_stmt* sql;
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Application WHERE key='IconPack'", 51, &sql, NULL)))
		return false;
	bool ret = GetValueForStatement(sqlIconPack, result);
	sqlite3_finalize(sql);
	return ret;
}

bool TS3Settings::GetSoundPack(std::string& result)
{
	sqlite3_stmt* sql;
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Notifications WHERE key='SoundPack'", 54, &sql, NULL)))
		return false;
	bool ret = GetValueForStatement(sqlSoundPack, result);
	sqlite3_finalize(sql);
	return ret;
}

bool TS3Settings::GetDefaultCaptureProfile(std::string& result)
{
	sqlite3_stmt* sql;
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Profiles WHERE key='DefaultCaptureProfile'", 61, &sql, NULL)))
		return false;
	bool ret = GetValueForStatement(sqlDefaultCaptureProfile, result);
	sqlite3_finalize(sql);
	return ret;
}

bool TS3Settings::GetPreProcessorData(std::string profile, std::string& result)
{
	sqlite3_stmt* sql;
	std::stringstream ss;
	ss << "SELECT value FROM Profiles WHERE key='Capture/" << profile << "/PreProcessing'";
	std::string query = ss.str();
	sqlite3_prepare_v2(settings, query.c_str(), query.length(), &sql, NULL);
	bool ret = GetValueForStatement(sql, result);
	sqlite3_finalize(sql);
	return ret;
}
