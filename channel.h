#ifndef CHANNEL_H
#define CHANNEL_H

#include "public_definitions.h"
#include <list>

class Channel
{
public:
	uint64 id;
	Channel* parent;
	std::list<Channel> subchannels;

	Channel(void);
	Channel(uint64 id, Channel* parent);
	~Channel(void);

	static int GetChannelHierarchy(uint64 scHandlerID, Channel* root);

	Channel* find(uint64 id);
	Channel* first(void);
	Channel* last(void);
	Channel* next(void);
	Channel* prev(void);
};

#endif
