/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2010-2012 Jules Blok
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

bool TS3Settings::GetValueForQuery(std::string query, std::string& result)
{
	// Prepare the statement
	sqlite3_stmt* sql;
	if(CheckAndHandle(sqlite3_prepare_v2(settings, query.c_str(), query.length(), &sql, NULL)))
		return false;

	// Get the value
	if(sqlite3_step(sql) != SQLITE_ROW) return false;
	if(sqlite3_column_count(sql) != 1) return false;
	if(sqlite3_column_type(sql, 0) != SQLITE_TEXT) return false;
	result = std::string(reinterpret_cast<const char*>(
		sqlite3_column_text(sql, 0)
	));

	// Finalize the statement
	if(CheckAndHandle(sqlite3_finalize(sql))) return false;

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
	return GetValueForQuery("SELECT value FROM Application WHERE key='IconPack'", result);
}

bool TS3Settings::GetSoundPack(std::string& result)
{
	return GetValueForQuery("SELECT value FROM Notifications WHERE key='SoundPack'", result);
}

bool TS3Settings::GetDefaultCaptureProfile(std::string& result)
{
	return GetValueForQuery("SELECT value FROM Profiles WHERE key='DefaultCaptureProfile'", result);
}

bool TS3Settings::GetPreProcessorData(std::string profile, std::string& result)
{
	std::stringstream ss;
	ss << "SELECT value FROM Profiles WHERE key='Capture/" << profile << "/PreProcessing'";
	return GetValueForQuery(ss.str(), result);
}
