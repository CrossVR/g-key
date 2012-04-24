/*
 * TeamSpeak 3 G-key plugin
 * Author: Jules Blok (jules@aerix.nl)
 *
 * Copyright (c) 2008-2012 TeamSpeak Systems GmbH
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/* Global variables */
extern BOOL pttActive;
extern BOOL vadActive;
extern BOOL inputActive;
extern BOOL whisperActive;

/* High-level TeamSpeak functions */

/* Error handler */
void ErrorMessage(uint64 scHandlerID, char* message, char* icon, char* sound);

/* Getters */
uint64 GetServerHandleByVariable(char* value, size_t flag);
uint64 GetChannelIDByVariable(uint64 scHandlerID, char* value, size_t flag);
anyID GetClientIDByVariable(uint64 scHandlerID, char* value, size_t flag);

/* Communication */
int SetPushToTalk(uint64 scHandlerID, BOOL shouldTalk);
int SetVoiceActivation(uint64 scHandlerID, BOOL shouldActivate);
int SetContinuousTransmission(uint64 scHandlerID, BOOL shouldActivate);
int SetInputMute(uint64 scHandlerID, BOOL shouldMute);
int SetOutputMute(uint64 scHandlerID, BOOL shouldMute);
int SetMasterVolume(uint64 scHandlerID, float value);

/* Whispering */
int SetWhisperList(uint64 scHandlerID, BOOL shouldWhisper);
void WhisperListClear(uint64 scHandlerID);
void WhisperAddClient(uint64 scHandlerID, anyID client);
void WhisperAddChannel(uint64 scHandlerID, uint64 channel);

/* Server interaction */
int SetActiveServer(uint64 handle);
int SetGlobalAway(BOOL isAway);
int JoinChannel(uint64 scHandlerID, uint64 channel);
int MuteClient(uint64 scHandlerID, anyID client);
int UnmuteClient(uint64 scHandlerID, anyID client);
int ServerKickClient(uint64 scHandlerID, anyID client);
int ChannelKickClient(uint64 scHandlerID, anyID client);
int JoinChannelRelative(uint64 scHandlerID, int direction);
inline int JoinNextChannel(uint64 scHandlerID) { return JoinChannelRelative(scHandlerID, 1); }
inline int JoinPrevChannel(uint64 scHandlerID) { return JoinChannelRelative(scHandlerID, -1); }

#endif
