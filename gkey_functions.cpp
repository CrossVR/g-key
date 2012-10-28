/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2010-2012 Jules Blok
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning(disable : 4996)  /* Disable unsafe localtime warning */
#endif

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

bool GKeyFunctions::CheckAndLog(unsigned int returnCode, char* message)
{
	if(returnCode != ERROR_ok)
	{
		char* errorMsg;
		if(ts3Functions.getErrorMessage(returnCode, &errorMsg) == ERROR_ok)
		{
			if(message != NULL) ts3Functions.logMessage(message, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
			ts3Functions.freeMemory(errorMsg);
			return true;
		}
	}
	return false;
}

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
		CheckAndLog(ts3Functions.playWaveFile(scHandlerID, errorSound.c_str()), "Error playing error sound");
	}
}

uint64 GKeyFunctions::GetActiveServerConnectionHandlerID()
{
	uint64* servers;
	uint64* server;
	uint64 handle = NULL;
	
	if(CheckAndLog(ts3Functions.getServerConnectionHandlerList(&servers), "Error retrieving list of servers"))
		return NULL;
	
	// Find the first server that matches the criteria
	for(server = servers; *server != (uint64)NULL && handle == NULL; server++)
	{
		int result;
		if(!CheckAndLog(ts3Functions.getClientSelfVariableAsInt(*server, CLIENT_INPUT_HARDWARE, &result), "Error retrieving client variable") && result)
		{
			handle = *server;
		}
	}
	
	ts3Functions.freeMemory(servers);
	return handle;
}

uint64 GKeyFunctions::GetServerHandleByVariable(char* value, size_t flag)
{
	char* variable;
	uint64* servers;
	uint64* server;
	uint64 result;

	if(CheckAndLog(ts3Functions.getServerConnectionHandlerList(&servers), "Error retrieving list of servers"))
		return (uint64)NULL;
	
	// Find the first server that matches the criteria
	for(server = servers, result = (uint64)NULL; *server != (uint64)NULL && result == (uint64)NULL; server++)
	{
		if(!CheckAndLog(ts3Functions.getServerVariableAsString(*server, flag, &variable), "Error retrieving server variable"))
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
	char* variable;
	uint64* channels;
	uint64* channel;
	uint64 result;
	
	if(CheckAndLog(ts3Functions.getChannelList(scHandlerID, &channels), "Error retrieving list of channels"))
		return (uint64)NULL;
	
	// Find the first channel that matches the criteria
	for(channel = channels, result = (uint64)NULL; *channel != (uint64)NULL && result == NULL; channel++)
	{
		if(!CheckAndLog(ts3Functions.getChannelVariableAsString(scHandlerID, *channel, flag, &variable), "Error retrieving channel variable"))
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
	char* variable;
	anyID* clients;
	anyID* client;
	anyID result;

	if(CheckAndLog(ts3Functions.getClientList(scHandlerID, &clients), "Error retrieving list of clients"))
		return (anyID)NULL;
	
	// Find the first client that matches the criteria
	for(client = clients, result = (anyID)NULL; *client != (uint64)NULL && result == (anyID)NULL; client++)
	{
		if(!CheckAndLog(ts3Functions.getClientVariableAsString(scHandlerID, *client, flag, &variable), "Error retrieving client variable"))
		{
			// If the variable matches the value set the result, this will end the loop
			if(!strcmp(value, variable)) result = *client;
			ts3Functions.freeMemory(variable);
		}
	}
	
	ts3Functions.freeMemory(clients);
	return result;
}

bool GKeyFunctions::SetPushToTalk(uint64 scHandlerID, bool shouldTalk)
{
	// If PTT is inactive, store the current settings
	if(!pttActive)
	{
		// Get the current VAD setting
		char* vad;
		if(CheckAndLog(ts3Functions.getPreProcessorConfigValue(scHandlerID, "vad", &vad), "Error retrieving vad setting"))
			return false;
		vadActive = !strcmp(vad, "true");
		ts3Functions.freeMemory(vad);
		
		// Get the current input setting, this will indicate whether VAD is being used in combination with PTT
		int input;
		if(CheckAndLog(ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, &input), "Error retrieving input setting"))
			return false;
		inputActive = !input; // We want to know when it is active, not when it is inactive 
	}
	
	// If VAD is active and the input is active, disable VAD, restore VAD setting afterwards
	if(CheckAndLog(ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad",
		(shouldTalk && (vadActive && inputActive)) ? "false" : (vadActive)?"true":"false"), "Error toggling vad"))
		return false;

	// Activate the input, restore the input setting afterwards
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldTalk || inputActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED), "Error toggling input"))
		return false;

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	pttActive = shouldTalk;

	return true;
}

bool GKeyFunctions::SetVoiceActivation(uint64 scHandlerID, bool shouldActivate)
{
	// Activate Voice Activity Detection
	if(CheckAndLog(ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad", (shouldActivate && !pttActive)?"true":"false"), "Error toggling vad"))
		return false;

	// Activate the input, restore the input setting afterwards
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldActivate) ? INPUT_ACTIVE : INPUT_DEACTIVATED), "Error toggling input"))
		return false;

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	vadActive = shouldActivate;
	inputActive = shouldActivate;

	return true;
}

bool GKeyFunctions::SetContinuousTransmission(uint64 scHandlerID, bool shouldActivate)
{
	// Activate the input, restore the input setting afterwards
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, 
		(shouldActivate || pttActive) ? INPUT_ACTIVE : INPUT_DEACTIVATED), "Error toggling input"))
		return false;

	// Update the client
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);

	// Commit the change
	inputActive = shouldActivate;

	return true;
}

bool GKeyFunctions::SetInputMute(uint64 scHandlerID, bool shouldMute)
{
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE), "Error toggling input mute"))
		return false;
	
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	return true;
}

bool GKeyFunctions::SetOutputMute(uint64 scHandlerID, bool shouldMute)
{
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_OUTPUT_MUTED, 
		shouldMute ? INPUT_DEACTIVATED : INPUT_ACTIVE), "Error toggling output mute"))
		return false;
	
	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	return true;
}

bool GKeyFunctions::SetGlobalAway(bool isAway, char* msg)
{
	uint64* servers;
	uint64 handle;
	int i;

	if(CheckAndLog(ts3Functions.getServerConnectionHandlerList(&servers), "Error retrieving list of servers"))
		return false;
	
	handle = servers[0];
	for(i = 1; handle != (uint64)NULL; i++)
	{
		SetAway(handle, isAway, msg);
		handle = servers[i];
	}

	ts3Functions.freeMemory(servers);
	return true;
}

bool GKeyFunctions::SetAway(uint64 scHandlerID, bool isAway, char* msg)
{
	if(CheckAndLog(ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_AWAY, 
		isAway ? AWAY_ZZZ : AWAY_NONE), "Error setting away status"))
		return false;

	if(CheckAndLog(ts3Functions.setClientSelfVariableAsString(scHandlerID, CLIENT_AWAY_MESSAGE, isAway && msg != NULL ? msg : ""), "Error setting away message"))
		return false;

	return CheckAndLog(ts3Functions.flushClientSelfUpdates(scHandlerID, NULL), "Error flushing after setting away status");
}

bool GKeyFunctions::JoinChannel(uint64 scHandlerID, uint64 channel)
{
	anyID self;

	if(CheckAndLog(ts3Functions.getClientID(scHandlerID, &self), "Error getting own client id"))
		return false;
	
	if(CheckAndLog(ts3Functions.requestClientMove(scHandlerID, self, channel, "", NULL), "Error joining channel"))
		return false;

	return true;
}

bool GKeyFunctions::SetWhisperList(uint64 scHandlerID, bool shouldWhisper)
{
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
	if(CheckAndLog(ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, shouldWhisper?&list->second.channels[0]:(uint64*)NULL, shouldWhisper?&list->second.clients[0]:(anyID*)NULL, NULL), "Error setting whisper list"))
		return false;

	if(shouldWhisper)
	{
		// Remove the NULL-terminator
		list->second.clients.pop_back();
		list->second.channels.pop_back();
	}

	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	whisperActive = shouldWhisper;

	return true;
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

bool GKeyFunctions::SetReplyList(uint64 scHandlerID, bool shouldReply)
{
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
	if(CheckAndLog(ts3Functions.requestClientSetWhisperList(scHandlerID, (anyID)NULL, NULL, shouldReply?&list->second[0]:(anyID*)NULL, NULL), "Error setting reply list"))
		return false;

	if(shouldReply)
	{
		// Remove the NULL-terminator
		list->second.pop_back();
	}

	ts3Functions.flushClientSelfUpdates(scHandlerID, NULL);
	replyActive = shouldReply;

	if(!shouldReply) return SetWhisperList(scHandlerID, true);
	return true;
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

bool GKeyFunctions::SetActiveServer(uint64 handle)
{
	return CheckAndLog(ts3Functions.activateCaptureDevice(handle), "Error activating server");
}

bool GKeyFunctions::MuteClient(uint64 scHandlerID, anyID client)
{
	if(CheckAndLog(ts3Functions.requestMuteClients(scHandlerID, &client, NULL), "Error muting client"))
		return false;
	
	return CheckAndLog(ts3Functions.requestClientVariables(scHandlerID, client, NULL), "Error flushing after muting client");
}

bool GKeyFunctions::UnmuteClient(uint64 scHandlerID, anyID client)
{
	if(CheckAndLog(ts3Functions.requestUnmuteClients(scHandlerID, &client, NULL), "Error unmuting client"))
		return false;
	
	return CheckAndLog(ts3Functions.requestClientVariables(scHandlerID, client, NULL), "Error flushing after unmuting client");
}

bool GKeyFunctions::ServerKickClient(uint64 scHandlerID, anyID client)
{
	return CheckAndLog(ts3Functions.requestClientKickFromServer(scHandlerID, client, "", NULL), "Error kicking client from server");
}

bool GKeyFunctions::ChannelKickClient(uint64 scHandlerID, anyID client)
{
	return CheckAndLog(ts3Functions.requestClientKickFromChannel(scHandlerID, client, "", NULL), "Error kicking client from channel");
}

bool GKeyFunctions::SetMasterVolume(uint64 scHandlerID, float value)
{
	// Clamp value
	char str[6];
	if(value < -40.0) value = -40.0;
	if(value > 20.0) value = 20.0;

	snprintf(str, 6, "%.1f", value);
	return CheckAndLog(ts3Functions.setPlaybackConfigValue(scHandlerID, "volume_modifier", str), "Error setting master volume");
}

bool GKeyFunctions::JoinChannelRelative(uint64 scHandlerID, bool next)
{
	anyID self;
	uint64 ownId;
	Channel root;

	// Get channel hierarchy
	if(Channel::GetChannelHierarchy(scHandlerID, &root) != 0) return false;

	// Get own channel
	if(CheckAndLog(ts3Functions.getClientID(scHandlerID, &self), "Error getting own client id"))
		return false;
	
	if(CheckAndLog(ts3Functions.getChannelOfClient(scHandlerID, self, &ownId), "Error getting own channel id"))
		return false;
	
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
		CheckAndLog(ts3Functions.getChannelVariableAsInt(scHandlerID, channel->id, CHANNEL_FLAG_PASSWORD, &pswd), "Error getting channel info");
		if(!pswd) found = true;
	}
	if(!found) return false;

	// If a joinable channel was found, attempt to join it
	return CheckAndLog(ts3Functions.requestClientMove(scHandlerID, self, channel->id, "", NULL), "Error joining channel");
}

bool GKeyFunctions::SetActiveServerRelative(uint64 scHandlerID, bool next)
{
	uint64* servers;
	uint64* server;
	int result;

	// Get server list
	if(CheckAndLog(ts3Functions.getServerConnectionHandlerList(&servers), "Error retrieving list of servers"))
		return false;

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
	bool ret = !CheckAndLog(ts3Functions.getClientSelfVariableAsInt(*server, CLIENT_INPUT_HARDWARE, &result), "Error retrieving client variable");
	if(!result) SetActiveServer(*server);

	ts3Functions.freeMemory(servers);
	return ret;
}

uint64 GKeyFunctions::GetChannelIDFromPath(uint64 scHandlerID, char* path)
{
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
	if(CheckAndLog(ts3Functions.getChannelIDFromChannelNames(scHandlerID, &hierachy[0], &parent), "Error getting parent channel ID"))
		return false;
	
	return parent;
}

bool GKeyFunctions::ConnectToBookmark(char* label, PluginConnectTab connectTab, uint64* scHandlerID)
{
	// Get the bookmark list
	PluginBookmarkList* bookmarks;
	if(CheckAndLog(ts3Functions.getBookmarkList(&bookmarks), "Error getting bookmark list"))
		return false;

	// Find the bookmark
	bool ret = true;
	for(int i=0; i<bookmarks->itemcount; i++)
	{
		PluginBookmarkItem item = bookmarks->items[i];
		if(!item.isFolder)
		{
			if(!strcmp(item.name, label))
			{
				ret = !CheckAndLog(ts3Functions.guiConnectBookmark(connectTab, item.uuid, scHandlerID), "Failed to connect to bookmark");
			}
		}
	}
	
	ts3Functions.freeMemory(bookmarks);
	return ret;
}
