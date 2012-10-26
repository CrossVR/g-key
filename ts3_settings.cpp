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

TS3Settings::TS3Settings(void)
{
}

TS3Settings::~TS3Settings(void)
{
}

int TS3Settings::OpenDatabase(char* path)
{
	int ret = sqlite3_open(path, &settings);
	if(ret) {
		ts3Functions.logMessage(sqlite3_errmsg(settings), LogLevel_ERROR, "G-Key Plugin", 0);
		sqlite3_close(settings);
	}
	return ret;
}


bool TS3Settings::GetValueFromData(char* data, char* key, char* buf, int bufSize)
{
	std::stringstream ss(data);
	bool found = false;
	while(!ss.eof() && !found)
	{
		ss.getline(buf, bufSize, '=');
		found = !strcmp(buf, key);
		ss.getline(buf, bufSize);
	}
	return found;
}