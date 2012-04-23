/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include "functions.h"

// Push-to-talk
BOOL pttActive = FALSE;
BOOL vadActive = FALSE;
BOOL inputActive = FALSE;

// Whisper list
BOOL whisperActive = FALSE;
static anyID* whisperClients = (anyID*)NULL;
static uint64* whisperChannels = (uint64*)NULL;

void ErrorMessage(uint64 scHandlerID, char* message, char* icon, char* sound)
{
	time_t timer;
	char timeStr[11];
	size_t newLength = strlen(message)+strlen(icon)+69;
	char* styledMsg = (char*)malloc(newLength*sizeof(char));

	// Get the time
	time(&timer);
	strftime(timeStr, 11, "<%X>", localtime(&timer));

	// Format and print the error message and play the error sound
	// Use a transparent underscore because a double space will be collapsed
	sprintf_s(styledMsg, newLength, "[img]%s[/img][color=red]%s[color=transparent]_[/color]%s[/color]", icon, timeStr, message);
	ts3Functions.printMessage(scHandlerID, styledMsg, PLUGIN_MESSAGE_TARGET_SERVER);
	if(sound!=NULL) ts3Functions.playWaveFile(scHandlerID, sound);
	free(styledMsg);
}

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
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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

uint64 GetChannelIDByVariable(uint64 scHandlerID, char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	uint64* channels;
	uint64 handle;
	int i;
	
	if((error = ts3Functions.getChannelList(scHandlerID, &channels)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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

anyID GetClientIDByVariable(uint64 scHandlerID, char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	anyID* clients;
	anyID handle;
	int i;

	if((error = ts3Functions.getClientList(scHandlerID, &clients)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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

int SetPushToTalk(uint64 scHandlerID, BOOL shouldTalk)
{
	unsigned int error;

	// If PTT is inactive, store the current settings
	if(!pttActive)
	{
		// Get the current VAD setting
		char* temp;
		if((error = ts3Functions.getPreProcessorConfigValue(scHandlerID, "vad", &temp)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving vad setting:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
		vadActive = !strcmp(temp, "true");
		ts3Functions.freeMemory(temp);
		
		// Get the current input setting, this will indicate whether VAD is being used in combination with PTT.
		if((error = ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, &inputActive)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving input setting:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
		inputActive = !inputActive; // We want to know when it is active, not when it is inactive 
	}
	
	// If VAD is active and the input is active, disable VAD, restore VAD setting afterwards
	if((error = ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad",
		(shouldTalk && (vadActive && inputActive)) ? "false" : (vadActive)?"true":"false")) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling vad:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Activate the input, restore the input setting afterwards
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldTalk || inputActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling input:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	pttActive = shouldTalk;

	return 0;
}

int SetVoiceActivation(uint64 scHandlerID, BOOL shouldActivate)
{
	unsigned int error;

	// Activate Voice Activity Detection
	if((error = ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad", (shouldActivate && !pttActive)?"true":"false")) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling vad:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Activate the input, restore the input setting afterwards
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldActivate) ? INPUT_ACTIVE : INPUT_DEACTIVATED)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling input:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	vadActive = shouldActivate;
	inputActive = shouldActivate;

	return 0;
}

int SetInputActive(uint64 scHandlerID, BOOL shouldActivate)
{
	unsigned int error;

	// Activate the input, restore the input setting afterwards
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldActivate || pttActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling input:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	inputActive = shouldActivate;

	return 0;
}

int SetInputMute(uint64 scHandlerID, BOOL shouldMute)
{
	unsigned int error;

	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling input mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	return 0;
}

int SetOutputMute(uint64 scHandlerID, BOOL shouldMute)
{
	unsigned int error;

	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error toggling output mute:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
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
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error flushing after toggling away status:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		ts3Functions.flushClientSelfUpdates(handle, NULL);
		handle = servers[i];
	}

	ts3Functions.freeMemory(servers);

	return 0;
}

int JoinChannel(uint64 scHandlerID, uint64 channel)
{
	unsigned int error;
	anyID self;

	if((error = ts3Functions.getClientID(scHandlerID, &self)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error getting own client id:", LogLevel_DEBUG, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_DEBUG, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if((error = ts3Functions.requestClientMove(scHandlerID, self, channel, "", NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error joining channel:", LogLevel_DEBUG, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_DEBUG, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}

int SetWhisperList(uint64 scHandlerID, BOOL shouldWhisper)
{
	unsigned int error;

	if((error = ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, shouldWhisper?whisperChannels:(uint64*)NULL, shouldWhisper?whisperClients:(anyID*)NULL, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error setting whisper list:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	whisperActive = shouldWhisper;

	return 0;
}

void WhisperListClear(uint64 scHandlerID)
{
	SetWhisperList(scHandlerID, FALSE);
	free(whisperClients);
	free(whisperChannels);
	whisperClients = (anyID*)NULL;
	whisperChannels = (uint64*)NULL;
}

void WhisperAddClient(uint64 scHandlerID, anyID client)
{
	int count = 0;
	anyID* newList;
	
	// Check if old list exists
	if(whisperClients != NULL)
	{
		anyID* end;
		for(end = whisperClients; *end != (anyID)NULL; end++)
			if(*end == client) return; // Client already in list
		count = (int)(whisperClients-end);
	}

	// Make new list with enough room for the new id and the NULL terminator
	newList = (anyID*)malloc((count+2) * sizeof(anyID));
	newList[count] = client;
	newList[count+1] = (anyID)NULL;

	// Copy old list
	if(whisperClients != NULL)
	{
		memcpy(newList, whisperClients, (count) * sizeof(anyID));
		free(whisperClients);
	}
	
	// Commit new list
	whisperClients = newList;
	if(whisperActive) SetWhisperList(scHandlerID, TRUE);
}

void WhisperAddChannel(uint64 scHandlerID, uint64 channel)
{
	int count;
	uint64* newList;
	
	// Check if old list exists
	if(whisperChannels != NULL)
	{
		uint64* end;
		for(end = whisperChannels; *end != (uint64)NULL; end++)
			if(*end == channel) return; // Channel already in list
		count = (int)(whisperChannels-end);
	}

	// Make new list with enough room for the new id and the NULL terminator
	newList = (uint64*)malloc((count+2) * sizeof(uint64));
	newList[count] = channel;
	newList[count+1] = (uint64)NULL;

	// Copy old list
	if(whisperChannels != NULL)
	{
		memcpy(newList, whisperClients, (count) * sizeof(uint64));
		free(whisperChannels);
	}

	// Commit new list
	whisperChannels = newList;
	if(whisperActive) SetWhisperList(scHandlerID, TRUE);
}

int SetActiveServer(uint64 handle)
{
	unsigned int error;

	if((error = ts3Functions.activateCaptureDevice(handle)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error activating server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	ts3plugin_currentServerConnectionChanged(handle);

	return 0;
}

int MuteClient(uint64 scHandlerID, anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestMuteClients(scHandlerID, &client, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after muting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	return 0;
}

int UnmuteClient(uint64 scHandlerID, anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestUnmuteClients(scHandlerID, &client, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
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
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after unmuting client:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	return 0;
}

int ServerKickClient(uint64 scHandlerID, anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestClientKickFromServer(scHandlerID, client, "", NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error kicking client from server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}

int ChannelKickClient(uint64 scHandlerID, anyID client)
{
	unsigned int error;

	if((error = ts3Functions.requestClientKickFromChannel(scHandlerID, client, "", NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error kicking client from server:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	return 0;
}
