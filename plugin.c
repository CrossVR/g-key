/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2011 TeamSpeak Systems GmbH
 */

#ifdef WINDOWS
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#endif

#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public_errors.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin_events.h"
#include "plugin.h"

static struct TS3Functions ts3Functions;

#ifdef WINDOWS
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); dest[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 11

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define REQUESTCLIENTMOVERETURNCODES_SLOTS 5

#define WINDEBUG_TIMEOUT 1000

static char* pluginID = NULL;
static HANDLE pluginThread = NULL;
static BOOL pluginRunning = FALSE;
static uint64 scHandlerID = NULL;
static BOOL vadActive = FALSE;
static BOOL inputActive = FALSE;
static BOOL pttActive = FALSE;

/* Array for request client move return codes. See comments within ts3plugin_processCommand for details */
static char requestClientMoveReturnCodes[REQUESTCLIENTMOVERETURNCODES_SLOTS][RETURNCODE_BUFSIZE];

/*********************************** Plugin functions ************************************/

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
				printf("Error retrieving vad setting: %s\n", errorMsg);
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
				printf("Error retrieving input setting: %s\n", errorMsg);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
	}
	
	// Temporarily disable VAD if it is not used in combination with PTT.
	// Restore the previous VAD setting afterwards.
	if((error = ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad",
		(shouldTalk && (vadActive && !pttActive)) ? "false" : (vadActive)?"true":"false")) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			printf("Error toggling vad: %s\n", errorMsg);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Toggle input, it should always be on if PTT is inactive.
	// (In the case of VAD or CT)
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldTalk || !pttActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			printf("Error toggling push-to-talk: %s\n", errorMsg);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Update the client
	if(ts3Functions.flushClientSelfUpdates(scHandlerID) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			printf("Error flushing after toggling push-to-talk: %s\n", errorMsg);
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
			printf("Error toggling input mute: %s\n", errorMsg);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.flushClientSelfUpdates(scHandlerID) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			printf("Error flushing after toggling input mute: %s\n", errorMsg);
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
			printf("Error toggling output mute: %s\n", errorMsg);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if(ts3Functions.flushClientSelfUpdates(scHandlerID) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
		{
			printf("Error flushing after toggling output mute: %s\n", errorMsg);
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
			printf("Error retrieving list of servers: %s\n", errorMsg);
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
				printf("Error toggling away status: %s\n", errorMsg);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		if(ts3Functions.flushClientSelfUpdates(HandlerID) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) != ERROR_ok)
			{
				printf("Error flushing after toggling away status: %s\n", errorMsg);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		HandlerID = servers[i];
	}

	ts3Functions.freeMemory(servers);

	return 0;
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
		if(WaitForDebugEvent(&DebugEv, WINDEBUG_TIMEOUT))
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

					// Interpret debug string
					if(!strcmp(DebugStr, "TS3_PTT_ACTIVATE"))
					{
						SetPushToTalk(TRUE);
					}
					else if(!strcmp(DebugStr, "TS3_PTT_DEACTIVATE"))
					{
						SetPushToTalk(FALSE);
					}
					else if(!strcmp(DebugStr, "TS3_INPUT_MUTE"))
					{
						SetInputMute(TRUE);
					}
					else if(!strcmp(DebugStr, "TS3_INPUT_UNMUTE"))
					{
						SetInputMute(FALSE);
					}
					else if(!strcmp(DebugStr, "TS3_INPUT_TOGGLE"))
					{
						int muted;
						ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, &muted);
						SetInputMute(!muted);
					}
					else if(!strcmp(DebugStr, "TS3_OUTPUT_MUTE"))
					{
						SetOutputMute(TRUE);
					}
					else if(!strcmp(DebugStr, "TS3_OUTPUT_UNMUTE"))
					{
						SetOutputMute(FALSE);
					}
					else if(!strcmp(DebugStr, "TS3_OUTPUT_TOGGLE"))
					{
						int muted;
						ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, &muted);
						SetOutputMute(!muted);
					}
					else if(!strcmp(DebugStr, "TS3_AWAY_ZZZ"))
					{
						SetAway(TRUE);
					}
					else if(!strcmp(DebugStr, "TS3_AWAY_NONE"))
					{
						SetAway(FALSE);
					}
					else if(!strcmp(DebugStr, "TS3_AWAY_TOGGLE"))
					{
						int away;
						ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, &away);
						SetAway(!away);
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

DWORD WINAPI DebugThread(LPVOID pData)
{
	DWORD ProcessId; // Process ID for the Logitech drivers
	HANDLE hProcess; // Handle for the Logitech drivers
	
	/*
	 * NOTE: Never let this thread sleep longer than WINDEBUG_TIMEOUT per iteration,
	 * as the shutdown function will not wait that long for the thread to exit.
	 */

	while(pluginRunning)
	{
		// Get process id of the logitech driver
		if(!GetLogitechProcessId(&ProcessId))
		{
			// Open a read memory handle to the Logitech drivers
			hProcess = OpenProcess(PROCESS_VM_READ, FALSE, ProcessId);
			if(hProcess!=NULL)
			{
				// Attach debugger to Logitech drivers
				if(DebugActiveProcess(ProcessId)) DebugMain(ProcessId, hProcess);
				else 
				{
					// Could not attach debugger, exit debug thread
					ts3Functions.logMessage("Failed to attach debugger, are you using the correct version for your platform?", LogLevel_ERROR, "G-Key Plugin", 0);
					return 1;
				}

				// Deattach the debugger
				DebugActiveProcessStop(ProcessId);

				// Close the handle to the Logitech drivers
				CloseHandle(hProcess);
			}
		}
		else Sleep(WINDEBUG_TIMEOUT);
	}

	return 0;
}

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
    return "G-key Plugin";
}

/* Plugin version */
const char* ts3plugin_version() {
    return "0.4";
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
    return "This plugin allows you to use the macro G-keys on any Logitech device to control TeamSpeak 3 without rebinding them to other keys.";
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

	// Start the plugin thread
	pluginRunning = TRUE;
	pluginThread=CreateThread(NULL, NULL, DebugThread, 0, 0, NULL);
	if(pluginThread==NULL) return 1;

	/* Initialize return codes array for requestClientMove */
	memset(requestClientMoveReturnCodes, 0, REQUESTCLIENTMOVERETURNCODES_SLOTS * RETURNCODE_BUFSIZE);

    return 0;  /* 0 = success, 1 = failure */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
	// Stop the plugin thread
	pluginRunning = FALSE;
	// Wait for the thread to stop
	WaitForSingleObject(pluginThread, WINDEBUG_TIMEOUT);

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
	printf("PLUGIN: offersConfigure\n");
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

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
}

void ts3plugin_onNewChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID) {
}

void ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onChannelMoveEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 newChannelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onUpdateChannelEditedEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onUpdateClientEvent(uint64 serverConnectionHandlerID, anyID clientID) {
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
}

void ts3plugin_onClientMoveSubscriptionEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility) {
}

void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
}

void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
}

void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
}

void ts3plugin_onServerEditedEvent(uint64 serverConnectionHandlerID, anyID editerID, const char* editerName, const char* editerUniqueIdentifier) {
}

void ts3plugin_onServerUpdatedEvent(uint64 serverConnectionHandlerID) {
}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	return 0;  /* If no plugin return code was used, the return value of this function is ignored */
}

void ts3plugin_onServerStopEvent(uint64 serverConnectionHandlerID, const char* shutdownMessage) {
}

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
    return 0;  /* 0 = handle normally, 1 = client will ignore the text message */
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
}

void ts3plugin_onConnectionInfoEvent(uint64 serverConnectionHandlerID, anyID clientID) {
}

void ts3plugin_onServerConnectionInfoEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelSubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelSubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelUnsubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelUnsubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelDescriptionUpdateEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelPasswordChangedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onPlaybackShutdownCompleteEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onSoundDeviceListChangedEvent(const char* modeID, int playOrCap) {
}

void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {
}

void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {
}

void ts3plugin_onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels) {
}

void ts3plugin_onEditRecordedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
}

void ts3plugin_onCustom3dRolloffCalculationEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance , float* volume ){
}

void ts3plugin_onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {
}

/* Clientlib rare */

void ts3plugin_onClientBanFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, uint64 time, const char* kickMessage) {
}

int ts3plugin_onClientPokeEvent(uint64 serverConnectionHandlerID, anyID fromClientID, const char* pokerName, const char* pokerUniqueIdentity, const char* message, int ffIgnored) {
    return 0;  /* 0 = handle normally, 1 = client will ignore the poke */
}

void ts3plugin_onClientSelfVariableUpdateEvent(uint64 serverConnectionHandlerID, int flag, const char* oldValue, const char* newValue) {
}

void ts3plugin_onFileListEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path, const char* name, uint64 size, uint64 datetime, int type, uint64 incompletesize, const char* returnCode) {
}

void ts3plugin_onFileListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path) {
}

void ts3plugin_onFileInfoEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* name, uint64 size, uint64 datetime) {
}

void ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, const char* name, int type, int iconID, int saveDB) {
}

void ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerGroupByClientIDEvent(uint64 serverConnectionHandlerID, const char* name, uint64 serverGroupList, uint64 clientDatabaseID) {
}

void ts3plugin_onServerGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onServerGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID) {
}

void ts3plugin_onServerGroupClientListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, uint64 clientDatabaseID, const char* clientNameIdentifier, const char* clientUniqueID) {
}

void ts3plugin_onChannelGroupListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, const char* name, int type, int iconID, int saveDB) {
}

void ts3plugin_onChannelGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID) {
}

void ts3plugin_onChannelPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onClientPermListEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID) {
}

void ts3plugin_onChannelClientPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID) {
}

void ts3plugin_onClientChannelGroupChangedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, uint64 channelID, anyID clientID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, anyID failedPermissionID) {
	return 0;  /* See onServerErrorEvent for return code description */
}

void ts3plugin_onPermissionListEvent(uint64 serverConnectionHandlerID, anyID permissionID, const char* permissionName, const char* permissionDescription) {
}

void ts3plugin_onPermissionListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onPermissionOverviewEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, uint64 channelID, int overviewType, uint64 overviewID1, uint64 overviewID2, anyID permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onPermissionOverviewFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerGroupClientAddedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

void ts3plugin_onServerGroupClientDeletedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

void ts3plugin_onClientNeededPermissionsEvent(uint64 serverConnectionHandlerID, anyID permissionID, int permissionValue) {
}

void ts3plugin_onClientNeededPermissionsFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onFileTransferStatusEvent(anyID transferID, unsigned int status, const char* statusMessage, uint64 remotefileSize, uint64 serverConnectionHandlerID) {
}

void ts3plugin_onClientChatClosedEvent(uint64 serverConnectionHandlerID, anyID clientID) {
}

void ts3plugin_onClientChatComposingEvent(uint64 serverConnectionHandlerID, anyID clientID) {
}

void ts3plugin_onServerLogEvent(uint64 serverConnectionHandlerID, const char* logTimestamp, const char* logChannel, int logLevel, const char* logMsg) {
}

void ts3plugin_onServerLogFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerQueryEvent(uint64 serverConnectionHandlerID, const char* result) {
}

void ts3plugin_onMessageListEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, uint64 timestamp, int flagRead) {
}

void ts3plugin_onMessageGetEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, const char* message, uint64 timestamp) {
}

void ts3plugin_onClientDBIDfromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID) {
}

void ts3plugin_onClientNamefromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
}

void ts3plugin_onClientNamefromDBIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
}

void ts3plugin_onComplainListEvent(uint64 serverConnectionHandlerID, uint64 targetClientDatabaseID, const char* targetClientNickName, uint64 fromClientDatabaseID, const char* fromClientNickName, const char* complainReason, uint64 timestamp) {
}

void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID, uint64 banid, const char* ip, const char* name, const char* uid, uint64 creationTime, uint64 durationTime, const char* invokerName,
							  uint64 invokercldbid, const char* invokeruid, const char* reason, int numberOfEnforcements) {
}

void ts3plugin_onClientServerQueryLoginPasswordEvent(uint64 serverConnectionHandlerID, const char* loginPassword) {
}

void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand) {
}

void ts3plugin_onIncomingClientQueryEvent(uint64 serverConnectionHandlerID, const char* commandText) {
}
