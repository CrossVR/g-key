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
#include "functions.h"

struct TS3Functions ts3Functions;

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

#define PLUGIN_THREAD_TIMEOUT 1000

#define TIMER_MSEC 10000

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];

// Plugin values
char* pluginID = NULL;
bool pluginRunning = false;
char* configFile = NULL;
char* errorSound = NULL;
char* infoIcon = NULL;

// Error codes
enum PluginError {
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

/*********************************** Plugin functions ************************************/

VOID CALLBACK PTTDelayCallback(LPVOID lpArgToCompletionRoutine,DWORD dwTimerLowValue,DWORD dwTimerHighValue)
{
	// Acquire the mutex
	WaitForSingleObject(hMutex, PLUGIN_THREAD_TIMEOUT);

	// Turn off PTT
	SetPushToTalk(GetActiveServerConnectionHandlerID(), false);
	
	// Release the mutex
	ReleaseMutex(hMutex);
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
	uint64 scHandlerID = GetActiveServerConnectionHandlerID();
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
			if(pttActive) CancelWaitableTimer(hPttDelayTimer);
			SetPushToTalk(scHandlerID, true);
		}
	}
	else if(!strcmp(cmd, "TS3_PTT_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			char str[6];
			GetPrivateProfileStringA("Profiles", "Capture\\Default\\PreProcessing\\delay_ptt", "false", str, 10, configFile);
			if(!strcmp(str, "true"))
			{
				GetPrivateProfileStringA("Profiles", "Capture\\Default\\PreProcessing\\delay_ptt_msecs", "300", str, 10, configFile);
				dueTime.QuadPart = 0 - atoi(str) * TIMER_MSEC;

				SetWaitableTimer(hPttDelayTimer, &dueTime, 0, PTTDelayCallback, NULL, FALSE);
			}
			else
			{
				SetPushToTalk(scHandlerID, false);
			}
		}
	}
	else if(!strcmp(cmd, "TS3_PTT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			if(pttActive) CancelWaitableTimer(hPttDelayTimer);
			SetPushToTalk(scHandlerID, !pttActive);
		}
	}
	else if(!strcmp(cmd, "TS3_VAD_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetVoiceActivation(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_VAD_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetVoiceActivation(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_VAD_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetVoiceActivation(scHandlerID, !vadActive);
	}
	else if(!strcmp(cmd, "TS3_CT_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetContinuousTransmission(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_CT_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetContinuousTransmission(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_CT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetContinuousTransmission(scHandlerID, !inputActive);
	}
	else if(!strcmp(cmd, "TS3_INPUT_MUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetInputMute(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_INPUT_UNMUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetInputMute(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_INPUT_TOGGLE"))
	{
		if(status == STATUS_DISCONNECTED)
		{
			int muted;
			ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, &muted);
			SetInputMute(scHandlerID, !muted);
		}
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_MUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetOutputMute(scHandlerID, true);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_UNMUTE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetOutputMute(scHandlerID, false);
	}
	else if(!strcmp(cmd, "TS3_OUTPUT_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			int muted;
			ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, &muted);
			SetOutputMute(scHandlerID, !muted);
		}
	}
	/***** Server interaction *****/
	else if(!strcmp(cmd, "TS3_AWAY_ZZZ"))
	{
		SetGlobalAway(true);
	}
	else if(!strcmp(cmd, "TS3_AWAY_NONE"))
	{
		SetGlobalAway(false);
	}
	else if(!strcmp(cmd, "TS3_AWAY_TOGGLE"))
	{
		int away;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
		SetGlobalAway(!away);
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVER"))
	{
		if(arg != NULL && *arg != (char)NULL)
		{
			uint64 handle = GetServerHandleByVariable(arg, VIRTUALSERVER_NAME);
			if(handle != (uint64)NULL && handle != scHandlerID)
			{
				CancelWaitableTimer(hPttDelayTimer);
				SetActiveServer(handle);
			}
			else ErrorMessage(scHandlerID, "Server not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_ACTIVATE_SERVERID"))
	{
		if(arg != NULL && *arg != (char)NULL)
		{
			uint64 handle = GetServerHandleByVariable(arg, VIRTUALSERVER_UNIQUE_IDENTIFIER);
			if(handle != (uint64)NULL)
			{
				CancelWaitableTimer(hPttDelayTimer);
				SetActiveServer(handle);
			}
			else ErrorMessage(scHandlerID, "Server not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_SERVER_NEXT"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		SetNextActiveServer(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_SERVER_PREV"))
	{
		CancelWaitableTimer(hPttDelayTimer);
		SetPrevActiveServer(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHANNEL"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = GetChannelIDFromPath(scHandlerID, arg);
			if(id == (uint64)NULL) id = GetChannelIDByVariable(scHandlerID, arg, CHANNEL_NAME);
			if(id != (uint64)NULL) JoinChannel(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Channel not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_JOIN_CHANNELID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) JoinChannel(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Channel not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANNEL_NEXT"))
	{
		if(status == STATUS_DISCONNECTED)
			JoinNextChannel(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_CHANNEL_PREV"))
	{
		if(status != STATUS_DISCONNECTED)
			JoinPrevChannel(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) ServerKickClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_KICK_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) ServerKickClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) ChannelKickClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_CHANKICK_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) ChannelKickClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	/***** Whispering *****/
	else if(!strcmp(cmd, "TS3_WHISPER_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetWhisperList(scHandlerID, TRUE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetWhisperList(scHandlerID, FALSE);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetWhisperList(scHandlerID, !whisperActive);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLEAR"))
	{
		WhisperListClear(scHandlerID);
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) WhisperAddClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) WhisperAddClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CHANNEL"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = GetChannelIDFromPath(scHandlerID, arg);
			if(id == (uint64)NULL) id = GetChannelIDByVariable(scHandlerID, arg, CHANNEL_NAME);
			if(id != (uint64)NULL) WhisperAddChannel(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Channel not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_WHISPER_CHANNELID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			uint64 id = atoi(arg);
			if(id != (uint64)NULL) WhisperAddChannel(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Channel not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_REPLY_ACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetReplyList(scHandlerID, TRUE);
	}
	else if(!strcmp(cmd, "TS3_REPLY_DEACTIVATE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetReplyList(scHandlerID, FALSE);
	}
	else if(!strcmp(cmd, "TS3_REPLY_TOGGLE"))
	{
		if(status != STATUS_DISCONNECTED)
			SetReplyList(scHandlerID, !replyActive);
	}
	else if(!strcmp(cmd, "TS3_REPLY_CLEAR"))
	{
		ReplyListClear(scHandlerID);
	}
	/***** Miscellaneous *****/
	else if(!strcmp(cmd, "TS3_MUTE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) MuteClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) MuteClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL) UnmuteClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_UNMUTE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL) UnmuteClient(scHandlerID, id);
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENT"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_NICKNAME);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) MuteClient(scHandlerID, id);
				else UnmuteClient(scHandlerID, id);
			}
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_MUTE_TOGGLE_CLIENTID"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			anyID id = GetClientIDByVariable(scHandlerID, arg, CLIENT_UNIQUE_IDENTIFIER);
			if(id != (anyID)NULL)
			{
				int muted;
				ts3Functions.getClientVariableAsInt(scHandlerID, id, CLIENT_IS_MUTED, &muted);
				if(!muted) MuteClient(scHandlerID, id);
				else UnmuteClient(scHandlerID, id);
			}
			else ErrorMessage(scHandlerID, "Client not found");
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	else if(!strcmp(cmd, "TS3_VOLUME_UP"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			float diff = (arg!=NULL && *arg != (char)NULL)?(float)atof(arg):1.0f;
			float value;
			ts3Functions.getPlaybackConfigValueAsFloat(scHandlerID, "volume_modifier", &value);
			SetMasterVolume(scHandlerID, value+diff);
		}
	}
	else if(!strcmp(cmd, "TS3_VOLUME_DOWN"))
	{
		if(status != STATUS_DISCONNECTED)
		{
			float diff = (arg!=NULL && *arg != (char)NULL)?(float)atof(arg):1.0f;
			float value;
			ts3Functions.getPlaybackConfigValueAsFloat(scHandlerID, "volume_modifier", &value);
			SetMasterVolume(scHandlerID, value-diff);
		}
	}
	else if(!strcmp(cmd, "TS3_VOLUME_SET"))
	{
		if(status != STATUS_DISCONNECTED && arg != NULL && *arg != (char)NULL)
		{
			float value = (float)atof(arg);
			SetMasterVolume(scHandlerID, value);
		}
		else ErrorMessage(scHandlerID, "Missing argument");
	}
	/***** Error handler *****/
	else
	{
		ts3Functions.logMessage("Command not recognized:", LogLevel_WARNING, "G-Key Plugin", 0);
		ts3Functions.logMessage(cmd, LogLevel_WARNING, "G-Key Plugin", 0);
		ErrorMessage(scHandlerID, "Command not recognized");
	}

	// Release the mutex
	ReleaseMutex(hMutex);
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
    return "0.5.2";
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
	size_t length;

	// Find config file
	configFile = (char*)malloc(MAX_PATH);
	ts3Functions.getConfigPath(configFile, MAX_PATH);
	strcat_s(configFile, MAX_PATH, "ts3clientui_qt.conf");

	// Find error sound
	errorSound = (char*)malloc(MAX_PATH);
	ts3Functions.getResourcesPath(errorSound, MAX_PATH);
	strcat_s(errorSound, MAX_PATH, "sound/");
	length = strlen(errorSound);
	GetPrivateProfileStringA("Notifications", "SoundPack", "default", errorSound+length, MAX_PATH-(DWORD)length, configFile);

	char filename[MAX_PATH];
	strcat_s(errorSound, MAX_PATH, "/settings.ini");
	GetPrivateProfileStringA("soundfiles", "SERVER_ERROR", NULL, filename, MAX_PATH, errorSound);
	length = strlen(errorSound);
	if(strlen(filename) > 0)
	{
		errorSound[length-13] = NULL;
		*strrchr(filename, '\"') = NULL;
		char* file = strchr(filename, '\"');
		*file = '/';
		strcat_s(errorSound, MAX_PATH, file);
		ts3Functions.logMessage(errorSound, LogLevel_DEBUG, "G-Key Plugin", 0);
	}
	else
	{
		free(errorSound);
		errorSound = NULL;
	}

	// Find info icon
	infoIcon = (char*)malloc(MAX_PATH);
	ts3Functions.getResourcesPath(infoIcon, MAX_PATH);
	strcat_s(infoIcon, MAX_PATH, "gfx/");
	length = strlen(infoIcon);
	GetPrivateProfileStringA("Application", "IconPack", "default", infoIcon+length, MAX_PATH-(DWORD)length, configFile);
	strcat_s(infoIcon, MAX_PATH, "/16x16_message_info.png");

	// Create the command mutex
	hMutex = CreateMutex(NULL, FALSE, NULL);

	// Create the PTT delay timer
	hPttDelayTimer = CreateWaitableTimer(NULL, FALSE, NULL);

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
	pluginRunning = FALSE;
	
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
	return "g-key";
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
					case PLUGIN_ERROR_HOOK_FAILED: ErrorMessage(serverConnectionHandlerID, "Could not hook into Logitech software, make sure you're using the 64-bit version"); break;
					case PLUGIN_ERROR_READ_FAILED: ErrorMessage(serverConnectionHandlerID, "Not enough permissions to hook into Logitech software, try running as as administrator"); break;
					case PLUGIN_ERROR_NOT_FOUND: ErrorMessage(serverConnectionHandlerID, "Logitech software not running, start the Logitech software and reload the G-Key Plugin"); break;
					default: ErrorMessage(serverConnectionHandlerID, "G-Key Plugin failed to start, check the clientlog for more info"); break;
				}
			}
		}
	}
}

/* Add whisper clients to reply list */
void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
	if(isReceivedWhisper) ReplyAddClient(GetActiveServerConnectionHandlerID(), clientID);
}
