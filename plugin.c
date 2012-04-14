/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2011 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
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

#include <Windows.h>
#include <TlHelp32.h>

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); dest[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 16

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define REQUESTCLIENTMOVERETURNCODES_SLOTS 5

#define PLUGINTHREAD_TIMEOUT 1000

#define TIMER_MSEC 10000

// Plugin values
static char* pluginID = NULL;
static BOOL pluginRunning = FALSE;
static char configFile[MAX_PATH];

// Thread handles
static HANDLE hDebugThread = NULL;
static HANDLE hPTTDelayThread = NULL;

// Active server
static uint64 scHandlerID = (uint64)NULL;

// Push-to-talk
static BOOL vadActive = FALSE;
static BOOL inputActive = FALSE;
static BOOL pttActive = FALSE;
static HANDLE hPttDelayTimer = (HANDLE)NULL;
static LARGE_INTEGER dueTime;

// Whisper list
static BOOL whisperActive = FALSE;
static anyID* whisperClients = (anyID*)NULL;
static uint64* whisperChannels = (uint64*)NULL;

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];

/*********************************** TeamSpeak functions ************************************/

uint64 GetServerHandleByVariable(char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	uint64* servers;
	uint64 handle;
	int i;

	if((error = ts3Functions.getServerConnectionHandlerList(&servers)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of servers:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return (uint64)NULL;
	}
	
	handle = servers[0];
	for(i = 1; handle != (uint64)NULL; i++)
	{
		if((error = ts3Functions.getServerVariableAsString(handle, flag, &variable)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving server variable:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		else
		{
			if(!strcmp(value, variable))
			{
				ts3Functions.freeMemory(variable);
				ts3Functions.freeMemory(servers);
				return handle;
			}
			ts3Functions.freeMemory(variable);
		}
		handle = servers[i];
	}

	ts3Functions.freeMemory(servers);
	return handle;
}

uint64 GetChannelIDByVariable(char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	uint64* channels;
	uint64 handle;
	int i;
	
	if((error = ts3Functions.getChannelList(scHandlerID, &channels)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of channels:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return (uint64)NULL;
	}
	
	for(i = 1, handle = channels[0]; handle != (uint64)NULL; i++)
	{
		if((error = ts3Functions.getChannelVariableAsString(scHandlerID, handle, flag, &variable)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving channel variable:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		else
		{
			if(!strcmp(value, variable))
			{
				ts3Functions.freeMemory(variable);
				ts3Functions.freeMemory(channels);
				return handle;
			}
			ts3Functions.freeMemory(variable);
		}
		handle = channels[i];
	}

	ts3Functions.freeMemory(channels);
	return handle;
}

anyID GetClientIDByVariable(char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	anyID* clients;
	anyID handle;
	int i;

	if((error = ts3Functions.getClientList(scHandlerID, &clients)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of clients:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return (anyID)NULL;
	}
	
	for(i = 1, handle = clients[0]; handle != (uint64)NULL; i++)
	{
		if((error = ts3Functions.getClientVariableAsString(scHandlerID, handle, flag, &variable)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving client variable:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		else
		{
			if(!strcmp(value, variable))
			{
				ts3Functions.freeMemory(variable);
				ts3Functions.freeMemory(clients);
				return handle;
			}
			ts3Functions.freeMemory(variable);
		}
		handle = clients[i];
	}

	ts3Functions.freeMemory(clients);
	return handle;
}

int SetPushToTalk(BOOL shouldTalk)
{
	unsigned int error;

	// If PTT is inactive, retrieve some client settings to see how PTT should behave
	if(!inputActive)
	{
		// Get the current VAD setting
		char* temp;
		if((error = ts3Functions.getPreProcessorConfigValue(scHandlerID, "vad", &temp)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving vad setting:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
		vadActive = !strcmp(temp, "true");
		ts3Functions.freeMemory(temp);
		
		// Get the current input setting, this will indicate whether
		// VAD is being used in combination with PTT.
		if((error = ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, &pttActive)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving input setting:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
	}
	
	// Temporarily disable VAD if it is not used in combination with
	// PTT, restore the VAD setting afterwards.
	if((error = ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad",
		(shouldTalk && (vadActive && !pttActive)) ? "false" : (vadActive)?"true":"false")) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling vad:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Toggle input, it should always be on if PTT is inactive.
	// (in the case of VAD or CT)
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldTalk || !pttActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling push-to-talk:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Update the client
	if(ts3Functions.flushClientSelfUpdates(scHandlerID, NULL) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after toggling push-to-talk:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	// Commit the change
	inputActive = shouldTalk;

	return 0;
}

int SetInputMute(BOOL shouldMute)
{
	unsigned int error;

	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling input mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.flushClientSelfUpdates(scHandlerID, NULL) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after toggling input mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}
	return 0;
}

int SetOutputMute(BOOL shouldMute)
{
	unsigned int error;

	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling output mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.flushClientSelfUpdates(scHandlerID, NULL) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after toggling output mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}
	return 0;
}

int SetGlobalAway(BOOL isAway)
{
	unsigned int error;
	uint64* servers;
	uint64 handle;
	int i;

	if((error = ts3Functions.getServerConnectionHandlerList(&servers)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of servers:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	handle = servers[0];
	for(i = 1; handle != (uint64)NULL; i++)
	{
		if((error = ts3Functions.setClientSelfVariableAsInt(handle, CLIENT_AWAY, 
			isAway ? AWAY_ZZZ : AWAY_NONE)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error flushing after toggling away status:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		if(ts3Functions.flushClientSelfUpdates(handle, NULL) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error flushing after toggling away status:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		handle = servers[i];
	}

	ts3Functions.freeMemory(servers);

	return 0;
}

int JoinChannel(uint64 channel)
{
	unsigned int error;
	anyID self;

	if((error = ts3Functions.getClientID(scHandlerID, &self)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error getting own client id:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if((error = ts3Functions.requestClientMove(scHandlerID, self, channel, NULL, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error joining channel:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}

int SetWhisperList(BOOL active)
{
	unsigned int error;

	if((error = ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, whisperActive?whisperChannels:(uint64*)NULL, whisperActive?whisperClients:(anyID*)NULL, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error setting whisper list:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	whisperActive = active;

	return 0;
}

void WhisperListClear()
{
	SetWhisperList(FALSE);
	free(whisperClients);
	free(whisperChannels);
	whisperClients = (anyID*)NULL;
	whisperChannels = (uint64*)NULL;
}

void WhisperAddClient(anyID client)
{
	int count = 0;
	anyID* newList;
	
	// Check if old list exists
	if(whisperClients != NULL)
	{
		anyID* end;
		for(end = whisperClients; *end != (anyID)NULL; end++);
		count = (int)(whisperClients-end);
	}

	// Make new list with enough room for the new id and the NULL terminator
	newList = (anyID*)malloc((count+2) * sizeof(anyID));
	newList[count] = client;
	newList[count+1] = (anyID)NULL;

	// Copy old list
	if(whisperClients != NULL) memcpy(newList, whisperClients, (count) * sizeof(anyID));
	
	// Commit new list
	free(whisperClients);
	whisperClients = newList;
	if(whisperActive) SetWhisperList(TRUE);
}

void WhisperAddChannel(uint64 channel)
{
	int count;
	uint64* newList;
	
	// Check if old list exists
	if(whisperChannels != NULL)
	{
		uint64* end;
		for(end = whisperChannels; *end != (uint64)NULL; end++);
		count = (int)(whisperChannels-end);
	}

	// Make new list with enough room for the new id and the NULL terminator
	newList = (uint64*)malloc((count+2) * sizeof(uint64));
	newList[count] = channel;
	newList[count+1] = (uint64)NULL;

	// Copy old list
	if(whisperChannels != NULL) memcpy(newList, whisperClients, (count) * sizeof(uint64));

	// Commit new list
	free(whisperChannels);
	whisperChannels = newList;
	if(whisperActive) SetWhisperList(TRUE);
}

int SetActiveServer(uint64 handle)
{
	unsigned int error;
	
	SetPushToTalk(FALSE);
	WhisperListClear();

	if((error = ts3Functions.activateCaptureDevice(handle)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error activating server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if((error = ts3Functions.closeCaptureDevice(scHandlerID)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error deactivating server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	scHandlerID = handle;

	return 0;
}

int MuteClient(anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestMuteClients(scHandlerID, &client, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error muting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.requestClientVariables(scHandlerID, client, NULL) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after muting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	return 0;
}

int UnmuteClient(anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestUnmuteClients(scHandlerID, &client, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error unmuting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.requestClientVariables(scHandlerID, client, NULL) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after unmuting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	return 0;
}

int ServerKickClient(anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestClientKickFromServer(scHandlerID, client, "", NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error kicking client from server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}

int ChannelKickClient(anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestClientKickFromChannel(scHandlerID, client, "", NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			ts3Functions.logMessage("Error kicking client from server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}

/*********************************** Plugin functions ************************************/

VOID CALLBACK PTTDelayCallback(LPVOID lpArgToCompletionRoutine,DWORD dwTimerLowValue,DWORD dwTimerHighValue)
{
	SetPushToTalk(FALSE);
}

VOID ParseCommand(char* cmd, char* arg)
{
	// Interpret command string
	if(!strcmp(cmd, "TS3_PTT_ACTIVATE"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		SetPushToTalk(TRUE);
	}
	else if(!strcmp(cmd, "TS3_PTT_DEACTIVATE"))
	{
		char str[6];
		GetPrivateProfileStringA("Profiles", "Capture\\Default\\PreProcessing\\delay_ptt", "false", str, 10, configFile);
		if(!strcmp(str, "true"))
		{
			GetPrivateProfileStringA("Profiles", "Capture\\Default\\PreProcessing\\delay_ptt_msecs", "0", str, 10, configFile);
			dueTime.QuadPart = 0 - atoi(str) * TIMER_MSEC;

			SetWaitableTimer(hPttDelayTimer, &dueTime, 0, PTTDelayCallback, NULL, FALSE);
		}
		else
		{
			SetPushToTalk(FALSE);
		}
	}
	else if(!strcmp(cmd, "TS3_PTT_TOGGLE"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		SetPushToTalk(!inputActive);
	}
	else if(!strcmp(cmd, "TS3_INPUT_MUTE"))
	{
		SetInputMute(TRUE);
	}
	else if(!strcmp(cmd, "TS3_INPUT_UNMUTE"))
	{
		SetInputMute(FALSE);
	}
	else if(!strcmp(cmd, "TS3_INPUT_TOGGLE"))
	{
		int muted;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, &muted);
		SetInputMute(!muted);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_MUTE"))
	{
		SetOutputMute(TRUE);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_UNMUTE"))
	{
		SetOutputMute(FALSE);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_TOGGLE"))
	{
		int muted;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, &muted);
		SetOutputMute(!muted);
	}
	else if(!strcmp(cmd, "TS3_AWAY_ZZZ"))
	{
		SetGlobalAway(TRUE);
	}
	else if(!strcmp(cmd, "TS3_AWAY_NONE"))
	{
		SetGlobalAway(FALSE);
	}
	else if(!strcmp(cmd, "TS3_AWAY_TOGGLE"))
	{
		int away;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
		SetGlobalAway(!away);
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVER"))
	{
		if(arg != NULL)
		{
			uint64 handle = GetServerHandleByVariable(arg, VIRTUALSERVER_NAME);
			if(handle != (uint64)NULL) SetActiveServer(handle);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVERID"))
	{
		if(arg != NULL)
		{
			uint64 handle = GetServerHandleByVariable(arg, VIRTUALSERVER_UNIQUE_IDENTIFIER);
			if(handle != (uint64)NULL) SetActiveServer(handle);
			else ts3Functions.logMessage("Server not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHAN"))
	{
		if(arg != NULL)
		{
			uint64 id = GetChannelIDByVariable(arg, CHANNEL_NAME);
			if(id != (uint64)NULL) JoinChannel(id);
			else ts3Functions.logMessage("Channel not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHANID"))
	{
		if(arg != NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) JoinChannel(id);
			else ts3Functions.logMessage("Channel not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_ACTIVATE"))
	{
		SetWhisperList(TRUE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_DEACTIVATE"))
	{
		SetWhisperList(FALSE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_TOGGLE"))
	{
		SetWhisperList(!whisperActive);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLEAR"))
	{
		WhisperListClear();
	}
	else if(!strcmp(cmd, "TS3_WHISPER_ADD_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) WhisperAddClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_ADD_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) WhisperAddClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_ADD_CHAN"))
	{
		if(arg != NULL)
		{
			uint64 id = GetChannelIDByVariable(arg, CHANNEL_NAME);
			if(id != (uint64)NULL) WhisperAddChannel(id);
			else ts3Functions.logMessage("Channel not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_ADD_CHANID"))
	{
		if(arg != NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) WhisperAddChannel(id);
			else ts3Functions.logMessage("Channel not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_MUTE_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) MuteClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_MUTE_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) MuteClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) UnmuteClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) UnmuteClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) MuteClient(id);
				else UnmuteClient(id);
			}
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) MuteClient(id);
				else UnmuteClient(id);
			}
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) ServerKickClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) ServerKickClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENT"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) ChannelKickClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENTID"))
	{
		if(arg != NULL)
		{
			anyID id = GetClientIDByVariable(arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) ChannelKickClient(id);
			else ts3Functions.logMessage("Client not found", LogLevel_WARNING, "G-Key Plugin", 0);
		}
		else ts3Functions.logMessage("Missing argument", LogLevel_WARNING, "G-Key Plugin", 0);
	}
	else
	{
		ts3Functions.logMessage("Command not recognized:", LogLevel_WARNING, "G-Key Plugin", 0);
		ts3Functions.logMessage(cmd, LogLevel_WARNING, "G-Key Plugin", 0);
	}
}

int GetLogitechProcessId(DWORD* ProcessId)
{
	PROCESSENTRY32 entry;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, (DWORD)NULL);

	entry.dwSize = sizeof(PROCESSENTRY32);

	if(Process32First(snapshot, &entry))
	{
		while(Process32Next(snapshot, &entry))
		{
			if(!wcscmp(entry.szExeFile, L"LCore.exe"))
			{
				*ProcessId = entry.th32ProcessID;
				CloseHandle(snapshot);
				return 0;
			}
			else if(!wcscmp(entry.szExeFile, L"LGDCore.exe")) // Legacy support
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
		if(WaitForDebugEvent(&DebugEv, PLUGINTHREAD_TIMEOUT))
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
	DWORD ProcessId; // Process ID for the Logitech drivers
	HANDLE hProcess; // Handle for the Logitech drivers

	// Get process id of the logitech driver
	if(GetLogitechProcessId(&ProcessId))
	{
		ts3Functions.logMessage("Could not find Logitech software, are you sure it's running?", LogLevel_WARNING, "G-Key Plugin", 0);
		return 1;
	}

	// Open a read memory handle to the Logitech drivers
	hProcess = OpenProcess(PROCESS_VM_READ, FALSE, ProcessId);
	if(hProcess==NULL)
	{
		ts3Functions.logMessage("Failed to open Logitech software for reading, try running as administrator", LogLevel_ERROR, "G-Key Plugin", 0);
		return 1;
	}

	// Attach debugger to Logitech drivers
	if(!DebugActiveProcess(ProcessId))
	{
		// Could not attach debugger, exit debug thread
		ts3Functions.logMessage("Failed to attach debugger, are you using the correct version for your platform?", LogLevel_ERROR, "G-Key Plugin", 0);
		return 1;
	}

	ts3Functions.logMessage("Debugger attached to Logitech software", LogLevel_INFO, "G-Key Plugin", 0);
	DebugMain(ProcessId, hProcess);

	// Deattach the debugger
	DebugActiveProcessStop(ProcessId);
	ts3Functions.logMessage("Debugger detached from Logitech software", LogLevel_INFO, "G-Key Plugin", 0);

	// Close the handle to the Logitech drivers
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
    return "0.5.1";
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
    return "This plugin allows you to use the macro G-keys on any Logitech device to control TeamSpeak 3 without rebinding them to other keys.\n\nPlease read the G-Key ReadMe in the plugins folder for instructions, also available at http://jules.aerix.nl/g-key";
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
	// Find config files
	ts3Functions.getConfigPath(configFile, MAX_PATH);
	strcat_s(configFile, MAX_PATH, "ts3clientui_qt.conf");

	// Get first connection handler
	scHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();

	// Start the plugin threads
	pluginRunning = TRUE;
	hDebugThread = CreateThread(NULL, (SIZE_T)NULL, DebugThread, 0, 0, NULL);

	// Create the PTT delay timer
	hPttDelayTimer = CreateWaitableTimer(NULL, FALSE, NULL);

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
	pluginRunning = FALSE;
	
	// Cancel PTT delay timer
	CancelWaitableTimer(hPttDelayTimer);

	// Wait for the thread to stop
	WaitForSingleObject(hDebugThread, PLUGINTHREAD_TIMEOUT);

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
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
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
	return NULL;
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	return 1;  /* Plugin did not handle command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
	scHandlerID = serverConnectionHandlerID;
}

void ts3plugin_pluginEvent(unsigned short data, const char* message) {
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
