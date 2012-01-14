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
#include "plugin_events.h"
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

#define PLUGIN_API_VERSION 15

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define REQUESTCLIENTMOVERETURNCODES_SLOTS 5

#define PLUGINTHREAD_TIMEOUT 1000

static char* pluginID = NULL;
static HANDLE hDebugThread = NULL;
static BOOL pluginRunning = FALSE;
static uint64 scHandlerID = NULL;
static BOOL vadActive = FALSE;
static BOOL inputActive = FALSE;
static BOOL pttActive = FALSE;

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];

/*********************************** TeamSpeak functions ************************************/

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

int SetAway(BOOL isAway)
{
	unsigned int error;
	uint64* servers;
	uint64 HandlerID;
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

	HandlerID = servers[0];
	for(i = 1; HandlerID != NULL; i++)
	{
		if((error = ts3Functions.setClientSelfVariableAsInt(HandlerID, CLIENT_AWAY, 
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
		if(ts3Functions.flushClientSelfUpdates(HandlerID, NULL) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				ts3Functions.logMessage("Error flushing after toggling away status:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		HandlerID = servers[i];
	}

	ts3Functions.freeMemory(servers);

	return 0;
}

/*********************************** Plugin functions ************************************/

BOOL ParseCommand(char* cmd)
{
	// Interpret command string
	if(!strcmp(cmd, "TS3_PTT_ACTIVATE"))
	{
		SetPushToTalk(TRUE);
	}
	else if(!strcmp(cmd, "TS3_PTT_DEACTIVATE"))
	{
		SetPushToTalk(FALSE);
	}
	else if(!strcmp(cmd, "TS3_PTT_TOGGLE"))
	{
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
		SetAway(TRUE);
	}
	else if(!strcmp(cmd, "TS3_AWAY_NONE"))
	{
		SetAway(FALSE);
	}
	else if(!strcmp(cmd, "TS3_AWAY_TOGGLE"))
	{
		int away;
		ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
		SetAway(!away);
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}

int GetLogitechProcessId(DWORD* ProcessId)
{
	PROCESSENTRY32 entry;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

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
					// Retrieve debug string
					char* DebugStr = (char*)malloc(DebugEv.u.DebugString.nDebugStringLength);
					ReadProcessMemory(hProcess, DebugEv.u.DebugString.lpDebugStringData, DebugStr, DebugEv.u.DebugString.nDebugStringLength, NULL);
					
					// Continue the process
					ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, DBG_CONTINUE);

					// Parse debug string
					if(!ParseCommand(DebugStr))
					{
						ts3Functions.logMessage("Debug command not recognized:", LogLevel_DEBUG, "G-Key Plugin", 0);
						ts3Functions.logMessage(DebugStr, LogLevel_DEBUG, "G-Key Plugin", 0);
					}

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

	ts3Functions.logMessage("Debugger attached to Logitech keyboard drivers", LogLevel_INFO, "G-Key Plugin", 0);
	DebugMain(ProcessId, hProcess);

	// Deattach the debugger
	DebugActiveProcessStop(ProcessId);
	ts3Functions.logMessage("Debugger detached from Logitech keyboard drivers", LogLevel_INFO, "G-Key Plugin", 0);

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
    return "0.4.3";
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
	// Get first connection handler
	scHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();

	// Start the plugin threads
	pluginRunning = TRUE;

	// Debug thread
	hDebugThread = CreateThread(NULL, NULL, DebugThread, 0, 0, NULL);

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
	// Compile array of thread handles
	HANDLE threads[] = {
		hDebugThread
	};

	// Stop the plugin threads
	pluginRunning = FALSE;

	// Wait for the thread to stop
	WaitForMultipleObjects(
		2,						// Number of threads
		threads,				// Array of thread handles
		TRUE,					// Wait for all threads
		PLUGINTHREAD_TIMEOUT);	// Timeout

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
