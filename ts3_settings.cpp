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

TS3Settings::TS3Settings(void) :
	settings(NULL),
	sqlIconPack(NULL),
	sqlSoundPack(NULL),
	sqlDefaultCaptureProfile(NULL)
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
	if(CheckAndHandle(sqlite3_step(statement))) return false;
	if(sqlite3_column_count(statement) != 1) return false;
	if(sqlite3_column_type(statement, 0) != SQLITE_TEXT) return false;
	result = std::string(reinterpret_cast<const char*>(
		sqlite3_column_text(statement, 0)
	));
	if(CheckAndHandle(sqlite3_reset(statement))) return false;
}

bool TS3Settings::OpenDatabase(std::string path)
{
	if(settings != NULL) CloseDatabase();

	if(CheckAndHandle(sqlite3_open(path.c_str(), &settings)))
		return false;
	
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Application WHERE key='IconPack'", 51, &sqlIconPack, NULL)))
		return false;
	
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Notifications WHERE key='SoundPack'", 54, &sqlSoundPack, NULL)))
		return false;
	
	if(CheckAndHandle(sqlite3_prepare_v2(settings, "SELECT value FROM Profiles WHERE key='DefaultCaptureProfile'", 61, &sqlDefaultCaptureProfile, NULL)))
		return false;

	return true;
}

void TS3Settings::CloseDatabase()
{
	sqlite3_finalize(sqlIconPack);
	sqlite3_finalize(sqlSoundPack);
	sqlite3_finalize(sqlDefaultCaptureProfile);
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
	return GetValueForStatement(sqlIconPack, result);
}

bool TS3Settings::GetSoundPack(std::string& result)
{
	return GetValueForStatement(sqlSoundPack, result);
}

bool TS3Settings::GetDefaultCaptureProfile(std::string& result)
{
	return GetValueForStatement(sqlDefaultCaptureProfile, result);
}

bool TS3Settings::GetPreProcessorData(std::string profile, std::string& result)
{
	sqlite3_stmt* sql;
	std::stringstream ss;
	ss << "SELECT value FROM Profiles WHERE key='Capture/" << profile << "/PreProcessing'";
	std::string query = ss.str();
	sqlite3_prepare_v2(settings, query.c_str(), query.length(), &sqlIconPack, NULL);
	bool ret = GetValueForStatement(sql, result);
	sqlite3_finalize(sql);
	return ret;
}
