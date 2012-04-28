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
#include <vector>
#include <map>

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); dest[destSize-1] = '\0'; }
#endif

// Push-to-talk
bool pttActive = false;
bool vadActive = false;
bool inputActive = false;

// Whisper list
class WhisperList
{
	public:
		std::vector<anyID> clients;
		std::vector<uint64> channels;
};
typedef std::map<uint64, WhisperList>::iterator WhisperIterator;

bool whisperActive = false;
std::map<uint64, WhisperList> whisperLists;

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
	snprintf(styledMsg, newLength, "[img]%s[/img][color=red]%s[color=transparent]_[/color]%s[/color]", icon, timeStr, message);
	ts3Functions.printMessage(scHandlerID, styledMsg, PLUGIN_MESSAGE_TARGET_SERVER);
	if(sound!=NULL) ts3Functions.playWaveFile(scHandlerID, sound);
	free(styledMsg);
}

int GetServerHandleByVariable(char* value, size_t flag, uint64* result)
{
	unsigned int error;
	char* variable;
	uint64* servers;
	uint64* server;

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
	
	// Find the first server that matches the criteria
	for(server = servers, *result = NULL; *server != (uint64)NULL && *result == NULL; server++)
	{
		if((error = ts3Functions.getServerVariableAsString(*server, flag, &variable)) != ERROR_ok)
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
			// If the variable matches the value set the result, this will end the loop
			if(!strcmp(value, variable)) *result = *server;
			ts3Functions.freeMemory(variable);
		}
	}

	ts3Functions.freeMemory(servers);
	return 0;
}

int GetChannelIDByVariable(uint64 scHandlerID, char* value, size_t flag, uint64* result)
{
	unsigned int error;
	char* variable;
	uint64* channels;
	uint64* channel;
	
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
	
	// Find the first channel that matches the criteria
	for(channel = channels, *result = NULL; *channel != (uint64)NULL && *result == NULL; channel++)
	{
		if((error = ts3Functions.getChannelVariableAsString(scHandlerID, *channel, flag, &variable)) != ERROR_ok)
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
			// If the variable matches the value set the result, this will end the loop
			if(!strcmp(value, variable)) *result = *channel;
			ts3Functions.freeMemory(variable);
		}
	}

	ts3Functions.freeMemory(channels);
	return 0;
}

int GetClientIDByVariable(uint64 scHandlerID, char* value, size_t flag, anyID* result)
{
	unsigned int error;
	char* variable;
	anyID* clients;
	anyID* client;

	if((error = ts3Functions.getClientList(scHandlerID, &clients)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of clients:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	
	// Find the first client that matches the criteria
	for(client = clients, *result = NULL; *client != (uint64)NULL && *result == NULL; client++)
	{
		if((error = ts3Functions.getClientVariableAsString(scHandlerID, *client, flag, &variable)) != ERROR_ok)
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
			// If the variable matches the value set the result, this will end the loop
			if(!strcmp(value, variable)) *result = *client;
			ts3Functions.freeMemory(variable);
		}
	}
	
	*result = *client;
	ts3Functions.freeMemory(clients);
	return 0;
}

int SetPushToTalk(uint64 scHandlerID, bool shouldTalk)
{
	unsigned int error;

	// If PTT is inactive, store the current settings
	if(!pttActive)
	{
		// Get the current VAD setting
		char* vad;
		if((error = ts3Functions.getPreProcessorConfigValue(scHandlerID, "vad", &vad)) != ERROR_ok)
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
		vadActive = !strcmp(vad, "true");
		ts3Functions.freeMemory(vad);
		
		// Get the current input setting, this will indicate whether VAD is being used in combination with PTT
		int input;
		if((error = ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, &input)) != ERROR_ok)
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
		inputActive = !input; // We want to know when it is active, not when it is inactive 
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

int SetVoiceActivation(uint64 scHandlerID, bool shouldActivate)
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

int SetContinuousTransmission(uint64 scHandlerID, bool shouldActivate)
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

int SetInputMute(uint64 scHandlerID, bool shouldMute)
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

int SetOutputMute(uint64 scHandlerID, bool shouldMute)
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

int SetGlobalAway(bool isAway)
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

int SetWhisperList(uint64 scHandlerID, bool shouldWhisper)
{
	unsigned int error;
	WhisperIterator list;

	if(shouldWhisper)
	{
		list = whisperLists.find(scHandlerID);
		if(list == whisperLists.end()) shouldWhisper = false;
		else
		{
			// Add the NULL-terminator
			list->second.clients.push_back((anyID)NULL);
			list->second.channels.push_back((uint64)NULL);
		}
	}

	/*
	 * For efficiency purposes I will violate the vector abstraction and give a direct pointer to its internal C array
	 */
	if((error = ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, shouldWhisper?&list->second.channels[0]:(uint64*)NULL, shouldWhisper?&list->second.clients[0]:(anyID*)NULL, NULL)) != ERROR_ok)
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

	if(shouldWhisper)
	{
		// Remove the NULL-terminator
		list->second.clients.pop_back();
		list->second.channels.pop_back();
	}

	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	whisperActive = shouldWhisper;

	return 0;
}

void WhisperListClear(uint64 scHandlerID)
{
	SetWhisperList(scHandlerID, false);
	whisperLists.erase(scHandlerID);
}

void WhisperAddClient(uint64 scHandlerID, anyID client)
{
	// Find the whisperlist, create it if it doesn't exist
	std::pair<WhisperIterator,bool> result = whisperLists.insert(std::pair<uint64,WhisperList>(scHandlerID, WhisperList()));
	WhisperIterator list = result.first;
	
	/*
	 * Do not add if duplicate. I could use a set, but that would be inefficient as
	 * ordering is unimportant and it would require me to convert to C arrays when
	 * activating the whisper list.
	 */
	for(std::vector<anyID>::iterator it=list->second.clients.begin(); it!=list->second.clients.end(); it++)
		if(*it == client) return;

	list->second.clients.push_back(client);
	if(whisperActive) SetWhisperList(scHandlerID, true);
}

void WhisperAddChannel(uint64 scHandlerID, uint64 channel)
{
	// Find the whisperlist, create it if it doesn't exist
	std::pair<WhisperIterator,bool> result = whisperLists.insert(std::pair<uint64,WhisperList>(scHandlerID, WhisperList()));
	WhisperIterator list = result.first;

	/*
	 * Do not add if duplicate. I could use a set, but that would be inefficient as
	 * ordering is unimportant and it would require me to convert to C arrays when
	 * activating the whisper list.
	 */
	for(std::vector<uint64>::iterator it=list->second.channels.begin(); it!=list->second.channels.end(); it++)
		if(*it == channel) return;

	list->second.channels.push_back(channel);
	if(whisperActive) SetWhisperList(scHandlerID, true);
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

int SetMasterVolume(uint64 scHandlerID, float value)
{
	unsigned int error;
	char str[5];
	
	// Clamp value
	if(value < -40.0) value = -40.0;
	if(value > 20.0) value = 20.0;

	snprintf(str, 5, "%.1f", value);
	if((error = ts3Functions.setPlaybackConfigValue(scHandlerID, "volume_modifier", str)) != ERROR_ok) {
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

int JoinChannelRelative(uint64 scHandlerID, bool next)
{
	unsigned int error;
	anyID self;
	uint64* channels;
	uint64* channel;
	uint64 ownChannel;
	int result = 1;

	// Get channel list
	if((error = ts3Functions.getChannelList(scHandlerID, &channels)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of channels:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	// Get own channel
	if((error = ts3Functions.getClientID(scHandlerID, &self)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error getting own client id:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	if((error = ts3Functions.getChannelOfClient(scHandlerID, self, &ownChannel)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error getting own channel id:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	
	// Find joinable channel
	uint64 target = ownChannel;
	if(next)
	{
		while(result != 0)
		{
			for(channel=channels; *channel!=NULL && result != target; channel++)
			{
				if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, *channel, CHANNEL_ORDER, &result)) != ERROR_ok)
				{
					char* errorMsg;
					if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
					{
						ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
						ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
						ts3Functions.freeMemory(errorMsg);
					}
					return 1;
				}
			}
			target = *(channel-1);
			if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, target, CHANNEL_FLAG_PASSWORD, &result)) != ERROR_ok)
			{
				char* errorMsg;
				if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
				{
					ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.freeMemory(errorMsg);
				}
				return 1;
			}
		}
	}
	else
	{
		while(result != 0)
		{
			if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, target, CHANNEL_ORDER, &result)) != ERROR_ok)
			{
				char* errorMsg;
				if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
				{
					ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.freeMemory(errorMsg);
				}
				return 1;
			}
			target = (uint64)result;
			if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, target, CHANNEL_FLAG_PASSWORD, &result)) != ERROR_ok)
			{
				char* errorMsg;
				if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
				{
					ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
					ts3Functions.freeMemory(errorMsg);
				}
				return 1;
			}
		}
	}

	// If a joinable channel was found, attempt to join it
	if(!result && target != ownChannel)
	{
		if((error = ts3Functions.requestClientMove(scHandlerID, self, target, "", NULL)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error joining channel:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
			return 1;
		}
	}

	return 0;
}

int SetActiveServerRelative(uint64 scHandlerID, bool next)
{
	unsigned int error;
	uint64* servers;
	uint64* server;

	// Get server list
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

	// Find active server in the list
	for(server=servers; *server!=NULL && *server!=scHandlerID; server++);

	// Find the server in the direction given
	if(next) server++;
	else if(server != servers) server--;

	if(*server != NULL) SetActiveServer(*server);
	else SetActiveServer(scHandlerID);

	return 0;
}
