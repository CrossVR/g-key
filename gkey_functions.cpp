/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2010-2012 Jules Blok
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gkey_functions.h"
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include "channel.h"

#include <vector>
#include <map>
#include <string>
#include <sstream>

GKeyFunctions::GKeyFunctions(void) : 
	pttActive(false),
	vadActive(false),
	inputActive(false),
	whisperActive(false),
	replyActive(false)
{
}

GKeyFunctions::~GKeyFunctions(void)
{
}

#pragma warning( disable : 4996 )
void GKeyFunctions::ErrorMessage(uint64 scHandlerID, char* message)
{
	// If an info icon has been found create a styled message
	if(!infoIcon.empty())
	{
		// Get the time
		time_t timer;
		char timeStr[11];
		time(&timer);
		strftime(timeStr, 11, "<%X>", localtime(&timer));

		// Format and print the error message, use a transparent underscore because a double space will be collapsed
		std::stringstream ss;
		ss << "[img]" << infoIcon << "[/img][color=red]" << timeStr << "[color=transparent]_[/color]" << message << "[/color]";
		ts3Functions.printMessage(scHandlerID, ss.str().c_str(), PLUGIN_MESSAGE_TARGET_SERVER);
	}
	else
	{
		// Format a simplified styled error message
		std::stringstream ss;
		ss << "[color=red]" << message << "[/color]";
		ts3Functions.printMessage(scHandlerID, ss.str().c_str(), PLUGIN_MESSAGE_TARGET_SERVER);
	}

	// If an error sound has been found play it
	if(!errorSound.empty())
	{
		// Play the error sound
		unsigned int error;
		if((error = ts3Functions.playWaveFile(scHandlerID, errorSound.c_str())) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error playing error sound:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
	}
}

uint64 GKeyFunctions::GetActiveServerConnectionHandlerID()
{
	unsigned int error;
	uint64* servers;
	uint64* server;
	uint64 handle = NULL;
	
	if((error = ts3Functions.getServerConnectionHandlerList(&servers)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving list of servers:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return NULL;
	}
	
	// Find the first server that matches the criteria
	for(server = servers; *server != (uint64)NULL && handle == NULL; server++)
	{
		int result;
		if((error = ts3Functions.getClientSelfVariableAsInt(*server, CLIENT_INPUT_HARDWARE, &result)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error retrieving client variable:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		else if(result)
		{
			handle = *server;
		}
	}
	
	ts3Functions.freeMemory(servers);
	return handle;
}

uint64 GKeyFunctions::GetServerHandleByVariable(char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	uint64* servers;
	uint64* server;
	uint64 result;

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
	
	// Find the first server that matches the criteria
	for(server = servers, result = (uint64)NULL; *server != (uint64)NULL && result == (uint64)NULL; server++)
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
			if(!strcmp(value, variable)) result = *server;
			ts3Functions.freeMemory(variable);
		}
	}

	ts3Functions.freeMemory(servers);
	return result;
}

uint64 GKeyFunctions::GetChannelIDByVariable(uint64 scHandlerID, char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	uint64* channels;
	uint64* channel;
	uint64 result;
	
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
	for(channel = channels, result = (uint64)NULL; *channel != (uint64)NULL && result == NULL; channel++)
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
			if(!strcmp(value, variable)) result = *channel;
			ts3Functions.freeMemory(variable);
		}
	}

	ts3Functions.freeMemory(channels);
	return result;
}

anyID GKeyFunctions::GetClientIDByVariable(uint64 scHandlerID, char* value, size_t flag)
{
	unsigned int error;
	char* variable;
	anyID* clients;
	anyID* client;
	anyID result;

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
	
	// Find the first client that matches the criteria
	for(client = clients, result = (anyID)NULL; *client != (uint64)NULL && result == (anyID)NULL; client++)
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
			if(!strcmp(value, variable)) result = *client;
			ts3Functions.freeMemory(variable);
		}
	}
	
	ts3Functions.freeMemory(clients);
	return result;
}

int GKeyFunctions::SetPushToTalk(uint64 scHandlerID, bool shouldTalk)
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

int GKeyFunctions::SetVoiceActivation(uint64 scHandlerID, bool shouldActivate)
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

int GKeyFunctions::SetContinuousTransmission(uint64 scHandlerID, bool shouldActivate)
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

int GKeyFunctions::SetInputMute(uint64 scHandlerID, bool shouldMute)
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

int GKeyFunctions::SetOutputMute(uint64 scHandlerID, bool shouldMute)
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

int GKeyFunctions::SetGlobalAway(bool isAway, char* msg)
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
		SetAway(handle, isAway, msg);
		handle = servers[i];
	}

	ts3Functions.freeMemory(servers);
	return 0;
}

int GKeyFunctions::SetAway(uint64 scHandlerID, bool isAway, char* msg)
{
	unsigned int error;
	
	if((error = ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, 
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
	if((error = ts3Functions.setClientSelfVariableAsString(scHandlerID, CLIENT_AWAY_MESSAGE, isAway && msg != NULL ? msg : "")) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error flushing after setting away message:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
	}

	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	return 0;
}

int GKeyFunctions::JoinChannel(uint64 scHandlerID, uint64 channel)
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

int GKeyFunctions::SetWhisperList(uint64 scHandlerID, bool shouldWhisper)
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

void GKeyFunctions::WhisperListClear(uint64 scHandlerID)
{
	SetWhisperList(scHandlerID, false);
	whisperLists.erase(scHandlerID);
}

void GKeyFunctions::WhisperAddClient(uint64 scHandlerID, anyID client)
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

void GKeyFunctions::WhisperAddChannel(uint64 scHandlerID, uint64 channel)
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

int GKeyFunctions::SetReplyList(uint64 scHandlerID, bool shouldReply)
{
	unsigned int error;
	ReplyIterator list;

	if(shouldReply)
	{
		list = replyLists.find(scHandlerID);
		if(list == replyLists.end()) shouldReply = false;
		else
		{
			// Add the NULL-terminator
			list->second.push_back((anyID)NULL);
		}
	}

	/*
	 * For efficiency purposes I will violate the vector abstraction and give a direct pointer to its internal C array
	 */
	if((error = ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, NULL, shouldReply?&list->second[0]:(anyID*)NULL, NULL)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error setting reply list:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}

	if(shouldReply)
	{
		// Remove the NULL-terminator
		list->second.pop_back();
	}

	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	replyActive = shouldReply;

	if(!shouldReply) return SetWhisperList(scHandlerID, true);
	return 0;
}

void GKeyFunctions::ReplyListClear(uint64 scHandlerID)
{
	SetReplyList(scHandlerID, false);
	replyLists.erase(scHandlerID);
}

void GKeyFunctions::ReplyAddClient(uint64 scHandlerID, anyID client)
{
	// Find the whisperlist, create it if it doesn't exist
	std::pair<ReplyIterator,bool> result = replyLists.insert(std::pair<uint64,std::vector<anyID>>(scHandlerID, std::vector<anyID>()));
	ReplyIterator list = result.first;
	
	/*
	 * Do not add if duplicate. I could use a set, but that would be inefficient as
	 * ordering is unimportant and it would require me to convert to C arrays when
	 * activating the whisper list.
	 */
	for(std::vector<anyID>::iterator it=list->second.begin(); it!=list->second.end(); it++)
		if(*it == client) return;

	list->second.push_back(client);
	if(replyActive) SetReplyList(scHandlerID, true);
}

int GKeyFunctions::SetActiveServer(uint64 handle)
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

	return 0;
}

int GKeyFunctions::MuteClient(uint64 scHandlerID, anyID client)
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

int GKeyFunctions::UnmuteClient(uint64 scHandlerID, anyID client)
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

int GKeyFunctions::ServerKickClient(uint64 scHandlerID, anyID client)
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

int GKeyFunctions::ChannelKickClient(uint64 scHandlerID, anyID client)
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

int GKeyFunctions::SetMasterVolume(uint64 scHandlerID, float value)
{
	unsigned int error;
	char str[6];
	
	// Clamp value
	if(value < -40.0) value = -40.0;
	if(value > 20.0) value = 20.0;

	snprintf(str, 6, "%.1f", value);
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

int GKeyFunctions::JoinChannelRelative(uint64 scHandlerID, bool next)
{
	unsigned int error;
	anyID self;
	uint64 ownId;
	Channel root;

	// Get channel hierarchy
	if(Channel::GetChannelHierarchy(scHandlerID, &root) != 0) return 1;

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
	if((error = ts3Functions.getChannelOfClient(scHandlerID, self, &ownId)) != ERROR_ok)
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
	
	// Find own channel in hierarchy
	Channel* channel = root.find(ownId);
	
	// Find a joinable channel
	bool found = false;
	while(channel != NULL && !found)
	{
		if(next)
		{
			// If the channel has subchannels, go deeper
			if(!channel->subchannels.empty()) channel = channel->first();
			else channel = channel->next();
		}
		else channel = channel->prev();
			
		// If this channel is passworded, join the next
		int pswd;
		if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, channel->id, CHANNEL_FLAG_PASSWORD, &pswd)) != ERROR_ok)
		{
			char* errorMsg;
			if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
			{
				ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
				ts3Functions.freeMemory(errorMsg);
			}
		}
		if(!pswd) found = true;
	}

	// If a joinable channel was found, attempt to join it
	if(found)
	{
		if((error = ts3Functions.requestClientMove(scHandlerID, self, channel->id, "", NULL)) != ERROR_ok)
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
		return 0;
	}
	return 1;
}

int GKeyFunctions::SetActiveServerRelative(uint64 scHandlerID, bool next)
{
	unsigned int error;
	uint64* servers;
	uint64* server;
	int result;

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
	for(server = servers; *server != (uint64)NULL && *server!=scHandlerID; server++);

	// Find the server in the direction given
	if(next)
	{
		if(*(server+1) != NULL) server++;
		else server = servers; // Wrap around to first server
	}
	else
	{
		if(server != servers) server--;
		else
		{
			for(server = servers; *server != (uint64)NULL; server++);
			server--;
		}
	}

	// Check if already active
	if((error = ts3Functions.getClientSelfVariableAsInt(*server, CLIENT_INPUT_HARDWARE, &result)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error retrieving client variable:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		ts3Functions.freeMemory(servers);
		return 1;
	}
	if(!result) SetActiveServer(*server);

	ts3Functions.freeMemory(servers);
	return 0;
}

uint64 GKeyFunctions::GetChannelIDFromPath(uint64 scHandlerID, char* path)
{
	unsigned int error;
	uint64 parent;

	// Split the string, following the hierachy
	char* str = path;
	char* lastStr = path;
	std::vector<char*> hierachy;
	while(str != NULL)
	{
		lastStr = str;
		str = strchr(lastStr, '/');
		if(str!=NULL)
		{
			*str = NULL;
			str++;
		}
		hierachy.push_back(lastStr);
	}
	hierachy.push_back(""); // Add the terminator
	
	/*
	 * For efficiency purposes I will violate the vector abstraction and give a direct pointer to its internal C array
	 */
	if((error = ts3Functions.getChannelIDFromChannelNames(scHandlerID, &hierachy[0], &parent)) != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
		{
			ts3Functions.logMessage("Error getting parent channel ID:", LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
		}
		return 1;
	}
	return parent;
}
