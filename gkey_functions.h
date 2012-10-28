/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "public_definitions.h"
#include "plugin_definitions.h"

#include <vector>
#include <map>
#include <string>

typedef struct
{
	std::vector<anyID> clients;
	std::vector<uint64> channels;
} WhisperList;
typedef std::map<uint64, std::vector<anyID>>::iterator ReplyIterator;
typedef std::map<uint64, WhisperList>::iterator WhisperIterator;

class GKeyFunctions
{
public:
	/* Push-to-talk */
	bool pttActive;
	bool vadActive;
	bool inputActive;

	/* Whisper lists */
	bool whisperActive;
	bool replyActive;

	/* Resources */
	std::string infoIcon;
	std::string errorSound;
private:
	std::map<uint64, WhisperList> whisperLists;
	std::map<uint64, std::vector<anyID>> replyLists;

	inline bool CheckAndLog(unsigned int returnCode, char* message = NULL);
public:
	GKeyFunctions(void);
	~GKeyFunctions(void);

	// Error handler
	void ErrorMessage(uint64 scHandlerID, char* message);

	// Getters
	uint64 GetActiveServerConnectionHandlerID(void);
	uint64 GetServerHandleByVariable(char* value, size_t flag);
	uint64 GetChannelIDByVariable(uint64 scHandlerID, char* value, size_t flag);
	anyID GetClientIDByVariable(uint64 scHandlerID, char* value, size_t flag);
	uint64 GetChannelIDFromPath(uint64 scHandlerID, char* path);

	// Communication
	bool SetPushToTalk(uint64 scHandlerID, bool shouldTalk);
	bool SetVoiceActivation(uint64 scHandlerID, bool shouldActivate);
	bool SetContinuousTransmission(uint64 scHandlerID, bool shouldActivate);
	bool SetInputMute(uint64 scHandlerID, bool shouldMute);
	bool SetOutputMute(uint64 scHandlerID, bool shouldMute);

	// Whispering
	bool SetWhisperList(uint64 scHandlerID, bool shouldWhisper);
	void WhisperListClear(uint64 scHandlerID);
	void WhisperAddClient(uint64 scHandlerID, anyID client);
	void WhisperAddChannel(uint64 scHandlerID, uint64 channel);
	bool SetReplyList(uint64 scHandlerID, bool shouldReply);
	void ReplyListClear(uint64 scHandlerID);
	void ReplyAddClient(uint64 scHandlerID, anyID client);

	// Server interaction
	bool SetActiveServer(uint64 handle);
	bool SetAway(uint64 scHandlerID, bool isAway, char* msg = "");
	bool SetGlobalAway(bool isAway, char* msg = NULL);
	bool JoinChannel(uint64 scHandlerID, uint64 channel);
	bool ServerKickClient(uint64 scHandlerID, anyID client);
	bool ChannelKickClient(uint64 scHandlerID, anyID client);
	bool JoinChannelRelative(uint64 scHandlerID, bool next);
	inline bool JoinNextChannel(uint64 scHandlerID) { return JoinChannelRelative(scHandlerID, true); }
	inline bool JoinPrevChannel(uint64 scHandlerID) { return JoinChannelRelative(scHandlerID, false); }
	bool SetActiveServerRelative(uint64 scHandlerID, bool next);
	inline bool SetNextActiveServer(uint64 scHandlerID) { return SetActiveServerRelative(scHandlerID, true); }
	inline bool SetPrevActiveServer(uint64 scHandlerID) { return SetActiveServerRelative(scHandlerID, false); }
	bool ConnectToBookmark(char* label, PluginConnectTab connectTab, uint64* scHandlerID);

	// Miscellaneous
	bool SetMasterVolume(uint64 scHandlerID, float value);
	bool MuteClient(uint64 scHandlerID, anyID client);
	bool UnmuteClient(uint64 scHandlerID, anyID client);
};

#endif
