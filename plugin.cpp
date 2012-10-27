/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#include <TlHelp32.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include "gkey_functions.h"
#include "ts3_settings.h"

#include <sstream>
#include <string>

struct TS3Functions ts3Functions;
GKeyFunctions gkeyFunctions;
TS3Settings ts3Settings;

#define PLUGIN_API_VERSION 19

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define REQUESTCLIENTMOVERETURNCODES_SLOTS 5

#define PLUGIN_THREAD_TIMEOUT 1000

#define TIMER_MSEC 10000

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];

// Plugin values
char* pluginID = NULL;
bool pluginRunning = false;

// Error codes
enum PluginError
{
	PLUGIN_ERROR_NONE = 0,
	PLUGIN_ERROR_HOOK_FAILED,
	PLUGIN_ERROR_READ_FAILED,
	PLUGIN_ERROR_NOT_FOUND
};

// Thread handles
static HANDLE hDebugThread = NULL;

// Mutex handles
static HANDLE hMutex = NULL;

// PTT Delay Timer
static HANDLE hPttDelayTimer = (HANDLE)NULL;
static LARGE_INTEGER dueTime;

/*********************************** Plugin callbacks ************************************/

VOID CALLBACK PTTDelayCallback(LPVOID lpArgToCompletionRoutine,DWORD dwTimerLowValue,DWORD dwTimerHighValue)
{
	// Acquire the mutex
	WaitForSingleObject(hMutex, PLUGIN_THREAD_TIMEOUT);

	// Turn off PTT
	gkeyFunctions.SetPushToTalk(gkeyFunctions.GetActiveServerConnectionHandlerID(), false);
	
	// Release the mutex
	ReleaseMutex(hMutex);
}

/*********************************** Plugin functions ************************************/

bool SetInfoIcon()
{
	// Find the icon pack
	std::string iconPack;
	if(!ts3Settings.GetIconPack(iconPack)) return false;

	// Find the path to the skin
	char path[MAX_PATH];
	ts3Functions.getResourcesPath(path, MAX_PATH);

	// Build and commit the path
	std::stringstream ss(path);
	ss << '/' << iconPack << "/16x16_message_info.png";
	gkeyFunctions.infoIcon = ss.str();

	return true;
}

bool SetErrorSound()
{
	// Find the sound pack
	std::string soundPack;
	if(!ts3Settings.GetSoundPack(soundPack)) return false;

	// Find the path to the soundpack
	char path[MAX_PATH];
	ts3Functions.getResourcesPath(path, MAX_PATH);
	std::stringstream ss(path);
	ss << "sound/" << soundPack;

	// Build the path to the config file
	std::string tmp = ss.str();
	ss << "/settings.ini";
	std::string config = ss.str();
	ss.str(tmp);

	// Parse the config file for the sound file
	char file[MAX_PATH];
	int size = GetPrivateProfileString("soundfiles", "SERVER_ERROR", NULL, file, MAX_PATH, config.c_str());
	if(size == 0) return false;

	// Filter out the filename: play("file.wav")
	*(strrchr(file, '\"')) = NULL;
	ss << '/' << strchr(file, '\"')+1;

	// Commit the path
	gkeyFunctions.errorSound = ss.str();

	return true;
}

bool PTTDelay()
{
	// Get default capture profile and preprocessor data
	std::string profile;
	std::string data;
	if(!ts3Settings.GetDefaultCaptureProfile(profile)) return false;
	if(!ts3Settings.GetPreProcessorData(profile, data)) return false;

	if(ts3Settings.GetValueFromData(data, "delay_ptt") != "true") return false;

	int msecs = atoi(ts3Settings.GetValueFromData(data, "delay_ptt_msecs").c_str());

	if(msecs > 0)
	{
		dueTime.QuadPart = -(msecs * TIMER_MSEC);
		SetWaitableTimer(hPttDelayTimer, &dueTime, 0, PTTDelayCallback, NULL, FALSE);
		return true;
	}

	return false;
}

void ParseCommand(char* cmd, char* arg)
{
	unsigned int error;

	// Acquire the mutex
	if(WaitForSingleObject(hMutex, PLUGIN_THREAD_TIMEOUT) != WAIT_OBJECT_0)
	{
		ts3Functions.logMessage("Timeout while waiting for mutex", LogLevel_WARNING, "G-Key Plugin", 0);
		return;
	}

	// Get the active server
	uint64 scHandlerID = gkeyFunctions.GetActiveServerConnectionHandlerID();
	if(scHandlerID == NULL)
	{
		ts3Functions.logMessage("Failed to get an active server, falling back to current server", LogLevel_DEBUG, "G-Key Plugin", 0);
		scHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
		if(scHandlerID == NULL) return;
	}

	int status;
	if((error = ts3Functions.getConnectionStatus(scHandlerID, &status)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving connection status:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	/***** Communication *****/
	if(!strcmp(cmd, "TS3_PTT_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			CancelWaitableTimer(hPttDelayTimer);
			gkeyFunctions.SetPushToTalk(scHandlerID, true);
		}
	}
	else if(!strcmp(cmd, "TS3_PTT_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			if(!PTTDelay()) // If query failed
				gkeyFunctions.SetPushToTalk(scHandlerID, false);
		}
	}
	else if(!strcmp(cmd, "TS3_PTT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			if(gkeyFunctions.pttActive) CancelWaitableTimer(hPttDelayTimer);
			gkeyFunctions.SetPushToTalk(scHandlerID, !gkeyFunctions.pttActive);
		}
	}
	else if(!strcmp(cmd, "TS3_VAD_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetVoiceActivation(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_VAD_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetVoiceActivation(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_VAD_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetVoiceActivation(scHandlerID, !gkeyFunctions.vadActive);
	}
	else if(!strcmp(cmd, "TS3_CT_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetContinuousTransmission(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_CT_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetContinuousTransmission(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_CT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetContinuousTransmission(scHandlerID, !gkeyFunctions.inputActive);
	}
	else if(!strcmp(cmd, "TS3_INPUT_MUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetInputMute(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_INPUT_UNMUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetInputMute(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_INPUT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			int muted;
			ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, &muted);
			gkeyFunctions.SetInputMute(scHandlerID, !muted);
		}
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_MUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetOutputMute(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_UNMUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetOutputMute(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			int muted;
			ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, &muted);
			gkeyFunctions.SetOutputMute(scHandlerID, !muted);
		}
	}
	/***** Server interaction *****/
	else if(!strcmp(cmd, "TS3_AWAY_ZZZ"))
	{
		gkeyFunctions.SetAway(scHandlerID, true, arg);
	}
	else if(!strcmp(cmd, "TS3_AWAY_NONE"))
	{
		gkeyFunctions.SetAway(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_AWAY_TOGGLE"))
	{
		int away;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
		gkeyFunctions.SetAway(scHandlerID, !away, arg);
	}
	else if(!strcmp(cmd, "TS3_GLOBALAWAY_ZZZ"))
	{
		gkeyFunctions.SetGlobalAway(true, arg);
	}
	else if(!strcmp(cmd, "TS3_GLOBALAWAY_NONE"))
	{
		gkeyFunctions.SetGlobalAway(false);
	}
	else if(!strcmp(cmd, "TS3_GLOBALAWAY_TOGGLE"))
	{
		int away;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
		gkeyFunctions.SetGlobalAway(!away, arg);
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVER"))
	{
		if(arg != NULL && *arg != (char)NULL)
		{
			uint64 handle = gkeyFunctions.GetServerHandleByVariable(arg, VIRTUALSERVER_NAME);
			if(handle != (uint64)NULL && handle != scHandlerID)
			{
				CancelWaitableTimer(hPttDelayTimer);
				gkeyFunctions.SetActiveServer(handle);
			}
			else gkeyFunctions.ErrorMessage(scHandlerID, "Server not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVERID"))
	{
		if(arg != NULL && *arg != (char)NULL)
		{
			uint64 handle = gkeyFunctions.GetServerHandleByVariable(arg, VIRTUALSERVER_UNIQUE_IDENTIFIER);
			if(handle != (uint64)NULL)
			{
				CancelWaitableTimer(hPttDelayTimer);
				gkeyFunctions.SetActiveServer(handle);
			}
			else gkeyFunctions.ErrorMessage(scHandlerID, "Server not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVERIP"))
	{
		if(arg != NULL && *arg != (char)NULL)
		{
			uint64 handle = gkeyFunctions.GetServerHandleByVariable(arg, VIRTUALSERVER_IP);
			if(handle != (uint64)NULL)
			{
				CancelWaitableTimer(hPttDelayTimer);
				gkeyFunctions.SetActiveServer(handle);
			}
			else gkeyFunctions.ErrorMessage(scHandlerID, "Server not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_CURRENT"))
	{
		uint64 handle = ts3Functions.getCurrentServerConnectionHandlerID();
		if(handle != (uint64)NULL)
		{
			CancelWaitableTimer(hPttDelayTimer);
			gkeyFunctions.SetActiveServer(handle);
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Server not found");
	}
	else if(!strcmp(cmd, "TS3_SERVER_NEXT"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		gkeyFunctions.SetNextActiveServer(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_SERVER_PREV"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		gkeyFunctions.SetPrevActiveServer(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHANNEL"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = gkeyFunctions.GetChannelIDFromPath(scHandlerID, arg);
			if(id == (uint64)NULL) id = gkeyFunctions.GetChannelIDByVariable(scHandlerID, arg, CHANNEL_NAME);
			if(id != (uint64)NULL) gkeyFunctions.JoinChannel(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Channel not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHANNELID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) gkeyFunctions.JoinChannel(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Channel not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANNEL_NEXT"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.JoinNextChannel(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_CHANNEL_PREV"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.JoinPrevChannel(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) gkeyFunctions.ServerKickClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) gkeyFunctions.ServerKickClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) gkeyFunctions.ChannelKickClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) gkeyFunctions.ChannelKickClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	/***** Whispering *****/
	else if(!strcmp(cmd, "TS3_WHISPER_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetWhisperList(scHandlerID, TRUE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetWhisperList(scHandlerID, FALSE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetWhisperList(scHandlerID, !gkeyFunctions.whisperActive);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLEAR"))
	{
		gkeyFunctions.WhisperListClear(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) gkeyFunctions.WhisperAddClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) gkeyFunctions.WhisperAddClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CHANNEL"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = gkeyFunctions.GetChannelIDFromPath(scHandlerID, arg);
			if(id == (uint64)NULL) id = gkeyFunctions.GetChannelIDByVariable(scHandlerID, arg, CHANNEL_NAME);
			if(id != (uint64)NULL) gkeyFunctions.WhisperAddChannel(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Channel not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CHANNELID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) gkeyFunctions.WhisperAddChannel(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Channel not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_REPLY_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetReplyList(scHandlerID, TRUE);
	}
	else if(!strcmp(cmd, "TS3_REPLY_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetReplyList(scHandlerID, FALSE);
	}
	else if(!strcmp(cmd, "TS3_REPLY_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			gkeyFunctions.SetReplyList(scHandlerID, !gkeyFunctions.replyActive);
	}
	else if(!strcmp(cmd, "TS3_REPLY_CLEAR"))
	{
		gkeyFunctions.ReplyListClear(scHandlerID);
	}
	/***** Miscellaneous *****/
	else if(!strcmp(cmd, "TS3_MUTE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) gkeyFunctions.MuteClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) gkeyFunctions.MuteClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) gkeyFunctions.UnmuteClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) gkeyFunctions.UnmuteClient(scHandlerID, id);
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) gkeyFunctions.MuteClient(scHandlerID, id);
				else gkeyFunctions.UnmuteClient(scHandlerID, id);
			}
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = gkeyFunctions.GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) gkeyFunctions.MuteClient(scHandlerID, id);
				else gkeyFunctions.UnmuteClient(scHandlerID, id);
			}
			else gkeyFunctions.ErrorMessage(scHandlerID, "Client not found");
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_VOLUME_UP"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			float diff = (arg!=NULL && *arg != (char)NULL)?(float)atof(arg):1.0f;
			float value;
			ts3Functions.getPlaybackConfigValueAsFloat(scHandlerID, "volume_modifier", &value);
			gkeyFunctions.SetMasterVolume(scHandlerID, value+diff);
		}
	}
	else if(!strcmp(cmd, "TS3_VOLUME_DOWN"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			float diff = (arg!=NULL && *arg != (char)NULL)?(float)atof(arg):1.0f;
			float value;
			ts3Functions.getPlaybackConfigValueAsFloat(scHandlerID, "volume_modifier", &value);
			gkeyFunctions.SetMasterVolume(scHandlerID, value-diff);
		}
	}
	else if(!strcmp(cmd, "TS3_VOLUME_SET"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			float value = (float)atof(arg);
			gkeyFunctions.SetMasterVolume(scHandlerID, value);
		}
		else gkeyFunctions.ErrorMessage(scHandlerID, "Missing argument");
	}
	/***** Error handler *****/
	else
	{
		ts3Functions.logMessage("Command not recognized:", LogLevel_WARNING, "G-Key Plugin", 0);
		ts3Functions.logMessage(cmd, LogLevel_WARNING, "G-Key Plugin", 0);
		gkeyFunctions.ErrorMessage(scHandlerID, "Command not recognized");
	}

	// Release the mutex
	ReleaseMutex(hMutex);
}

int GetLogitechProcessId(LPDWORD ProcessId)
{
	PROCESSENTRY32 entry;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, (DWORD)NULL);

	entry.dwSize = sizeof(PROCESSENTRY32);

	if(Process32First(snapshot, &entry))
	{
		while(Process32Next(snapshot, &entry))
		{
			if(!strcmp(entry.szExeFile, "LCore.exe"))
			{
				*ProcessId = entry.th32ProcessID;
				CloseHandle(snapshot);
				return 0;
			}
			else if(!strcmp(entry.szExeFile, "LGDCore.exe")) // Legacy support
			{
				*ProcessId = entry.th32ProcessID;
				CloseHandle(snapshot);
				return 0;
			} 
		}
	}

	CloseHandle(snapshot);
	return 1; // No processes found
}

void DebugMain(DWORD ProcessId, HANDLE hProcess)
{
	DEBUG_EVENT DebugEv; // Buffer for debug messages

	// While the plugin is running
	while(pluginRunning)
	{
		// Wait for a debug message
		if(WaitForDebugEvent(&DebugEv, PLUGIN_THREAD_TIMEOUT))
		{
			// If the debug message is from the logitech driver
			if(DebugEv.dwProcessId == ProcessId)
			{
				// If this is a debug message and it uses ANSI
				if(DebugEv.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT && !DebugEv.u.DebugString.fUnicode)
				{
					char *DebugStr, *arg;

					// Retrieve debug string
					DebugStr = (char*)malloc(DebugEv.u.DebugString.nDebugStringLength);
					ReadProcessMemory(hProcess, DebugEv.u.DebugString.lpDebugStringData, DebugStr, DebugEv.u.DebugString.nDebugStringLength, NULL);
					
					// Continue the process
					ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, DBG_CONTINUE);

					// Seperate the argument from the command
					arg = strchr(DebugStr, ' ');
					if(arg != NULL)
					{
						// Split the string by inserting a NULL-terminator
						*arg = (char)NULL;
						arg++;
					}

					// Parse debug string
					ParseCommand(DebugStr, arg);

					// Free the debug string
					free(DebugStr);
				}
				else if(DebugEv.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
				{
					// The process is shutting down, exit the debugger
					return;
				}
				else if(DebugEv.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && DebugEv.u.Exception.ExceptionRecord.ExceptionCode != STATUS_BREAKPOINT)
				{
					// The process has crashed, exit the debugger
					return;
				}
				else ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, DBG_CONTINUE); // Continue the process
			}
			else ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, DBG_CONTINUE); // Continue the process
		}
	}
}

/*********************************** Plugin threads ************************************/
/*
 * NOTE: Never let threads sleep longer than PLUGINTHREAD_TIMEOUT per iteration,
 * the shutdown procedure will not wait that long for the thread to exit.
 */

DWORD WINAPI DebugThread(LPVOID pData)
{
	DWORD ProcessId; // Process ID for the Logitech software
	HANDLE hProcess; // Handle for the Logitech software

	// Get process id of the logitech software
	if(GetLogitechProcessId(&ProcessId))
	{
		ts3Functions.logMessage("Could not find Logitech software", LogLevel_ERROR, "G-Key Plugin", 0);
		pluginRunning = FALSE;
		return PLUGIN_ERROR_NOT_FOUND;
	}

	// Open a read memory handle to the Logitech software
	hProcess = OpenProcess(PROCESS_VM_READ, FALSE, ProcessId);
	if(hProcess==NULL)
	{
		ts3Functions.logMessage("Failed to open Logitech software for reading", LogLevel_ERROR, "G-Key Plugin", 0);
		pluginRunning = FALSE;
		return PLUGIN_ERROR_READ_FAILED;
	}

	// Attach debugger to Logitech software
	if(!DebugActiveProcess(ProcessId))
	{
		// Could not attach debugger, exit debug thread
		ts3Functions.logMessage("Failed to attach debugger", LogLevel_ERROR, "G-Key Plugin", 0);
		pluginRunning = FALSE;
		return PLUGIN_ERROR_HOOK_FAILED;
	}

	ts3Functions.logMessage("Debugger attached to Logitech software", LogLevel_INFO, "G-Key Plugin", 0);
	DebugMain(ProcessId, hProcess);

	// Dettach the debugger
	DebugActiveProcessStop(ProcessId);
	ts3Functions.logMessage("Debugger detached from Logitech software", LogLevel_INFO, "G-Key Plugin", 0);

	// Close the handle to the Logitech software
	CloseHandle(hProcess);

	return 0;
}

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
    return "G-Key Plugin";
}

/* Plugin version */
const char* ts3plugin_version() {
    return "0.5.8";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
    return "Jules Blok";
}

/* Plugin description */
const char* ts3plugin_description() {
    return "This plugin allows you to use the macro G-keys on any Logitech device to control TeamSpeak 3 without rebinding them to other keys.\n\nPlease read the G-Key ReadMe for instructions by pressing the Settings button, or read it online at http://jules.aerix.nl/g-key";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
	// Create the command mutex
	hMutex = CreateMutex(NULL, FALSE, NULL);

	// Create the PTT delay timer
	hPttDelayTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	// Find and open the settings database
	char db[MAX_PATH];
	ts3Functions.getConfigPath(db, MAX_PATH);
	_strcat(db, MAX_PATH, "settings.db");
	ts3Settings.OpenDatabase(db);

	// Find the error sound and info icon
	SetErrorSound();
	SetInfoIcon();

	// Start the plugin threads
	pluginRunning = true;
	hDebugThread = CreateThread(NULL, (SIZE_T)NULL, DebugThread, 0, 0, NULL);

	if(hDebugThread==NULL)
	{
		ts3Functions.logMessage("Failed to start threads, unloading plugin", LogLevel_ERROR, "G-Key Plugin", 0);
		return 1;
	}

	/* Initialize return codes array for requestClientMove */
	memset(requestClientMoveReturnCodes, 0, REQUESTCLIENTMOVERETURNCODES_SLOTS * RETURNCODE_BUFSIZE);

    return 0;  /* 0 = success, 1 = failure */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
	// Stop the plugin threads
	pluginRunning = false;

	// Close settings database
	ts3Settings.CloseDatabase();
	
	// Cancel PTT delay timer
	CancelWaitableTimer(hPttDelayTimer);

	// Wait for the thread to stop
	WaitForSingleObject(hDebugThread, PLUGIN_THREAD_TIMEOUT);

	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_CONFIGURE_QT_THREAD;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
	char path[MAX_PATH];
	ts3Functions.getPluginPath(path, MAX_PATH);
	_strcat(path, MAX_PATH, "/G-Key_ReadMe.pdf");
	ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOW);
}

/*
 * If the plugin wants to use error return codes or plugin commands, it needs to register a command ID. This function will be automatically
 * called after the plugin was initialized. This function is optional. If you don't use error return codes or plugin commands, the function
 * can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "gkey";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	size_t length = strlen(command);
	char* str = (char*)malloc(length+1);
	_strcpy(str, length+1, command);

	// Seperate the argument from the command
	char* arg = strchr(str, ' ');
	if(arg != NULL)
	{
		// Split the string by inserting a NULL-terminator
		*arg = (char)NULL;
		arg++;
	}

	ParseCommand(str, arg);

	free(str);

	return 0;  /* Plugin did not handle command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return NULL;
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 1;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Show an error message if the plugin failed to load */
void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    if(newStatus == STATUS_CONNECTION_ESTABLISHED)
	{
		if(!pluginRunning) 
		{
			DWORD errorCode;
			if(GetExitCodeThread(hDebugThread, &errorCode))
			{
				switch(errorCode)
				{
					case PLUGIN_ERROR_HOOK_FAILED: gkeyFunctions.ErrorMessage(serverConnectionHandlerID, "Could not hook into Logitech software, make sure you're using the 64-bit version"); break;
					case PLUGIN_ERROR_READ_FAILED: gkeyFunctions.ErrorMessage(serverConnectionHandlerID, "Not enough permissions to hook into Logitech software, try running as as administrator"); break;
					case PLUGIN_ERROR_NOT_FOUND: gkeyFunctions.ErrorMessage(serverConnectionHandlerID, "Logitech software not running, start the Logitech software and reload the G-Key Plugin"); break;
					default: gkeyFunctions.ErrorMessage(serverConnectionHandlerID, "G-Key Plugin failed to start, check the clientlog for more info"); break;
				}
			}
		}
	}
}

/* Add whisper clients to reply list */
void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
	if(isReceivedWhisper) gkeyFunctions.ReplyAddClient(gkeyFunctions.GetActiveServerConnectionHandlerID(), clientID);
}
