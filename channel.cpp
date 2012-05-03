#include "Channel.h"
#include "public_definitions.h"
#include "public_errors.h"
#include "ts3_functions.h"
#include "plugin.h"
#include <list>
#include <vector>
#include <stack>

Channel::Channel(void)
	: id(0), parent(NULL)
{
}

Channel::Channel(uint64 id, Channel* parent)
	: id(id), parent(parent)
{
}

Channel::~Channel(void)
{
}

Channel* Channel::find(uint64 id)
{
	if(id == this->id) return this;

	for(std::list<Channel>::iterator it = subchannels.begin(); it != subchannels.end(); ++it)
	{
		Channel* result = it->find(id);
		if(result != NULL) return result;
	}

	return NULL;
}

Channel* Channel::first()
{
	if(subchannels.empty()) return this;
	return &subchannels.front();
}

Channel* Channel::last()
{
	if(subchannels.empty()) return this;
	return subchannels.back().last();
}

Channel* Channel::next()
{
	// If this is the root, there is no next
	if(parent == NULL) return NULL;

	// Find self in parent channel list
	std::list<Channel>::iterator it;
	for(it = parent->subchannels.begin(); it != parent->subchannels.end() && it->id != this->id; ++it);
	if(it == parent->subchannels.end()) return NULL; // Abort if parent is not really the parent

	// Return the next channel
	it++;
	if(it == parent->subchannels.end()) return parent->next();
	return &(*it);
}

Channel* Channel::prev()
{
	// If this is the root, there is no next
	if(parent == NULL) return NULL;

	// Find self in parent channel list
	std::list<Channel>::iterator it;
	for(it = parent->subchannels.begin(); it != parent->subchannels.end() && it->id != this->id; ++it);
	if(it == parent->subchannels.end()) return NULL; // Abort if parent is not really the parent

	// Return the next channel
	if(it == parent->subchannels.begin()) return parent;
	it--;
	return it->last();
}

int Channel::GetChannelHierarchy(uint64 scHandlerID, Channel* root)
{
	unsigned int error;

	// Get channel list
	uint64* channels;
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

	// Sort channels
	for(uint64* channel = channels; *channel != NULL; channel++)
	{
		Channel* parent = root;
		uint64 id = *channel;
		std::stack<uint64> hierachy;

		// Build the hierarchy stack
		while(id != 0)
		{
			hierachy.push(id);
			ts3Functions.getParentChannelOfChannel(scHandlerID, id, &id);
		}

		while(!hierachy.empty())
		{
			id = hierachy.top();
			hierachy.pop();

			// Find the channel
			std::list<Channel>::iterator it;
			for(it = parent->subchannels.begin(); it != parent->subchannels.end() && it->id != id; ++it);

			// If the channel was found, set the parent, else add it
			if(it != parent->subchannels.end()) parent = &(*it);
			else
			{
				// Get the channel order
				int order;
				if((error = ts3Functions.getChannelVariableAsInt(scHandlerID, id, CHANNEL_ORDER, &order)) != ERROR_ok)
				{
					char* errorMsg;
					if(ts3Functions.getErrorMessage(error, &errorMsg) == ERROR_ok)
					{
						ts3Functions.logMessage("Error getting channel info:", LogLevel_WARNING, "G-Key Plugin", 0);
						ts3Functions.logMessage(errorMsg, LogLevel_WARNING, "G-Key Plugin", 0);
						ts3Functions.freeMemory(errorMsg);
					}
					ts3Functions.freeMemory(channels);
					return 1;
				}

				if(order == 0) // Top channel is added after parent
				{
					if(parent->subchannels.empty() || parent->subchannels.front().id != id) parent->subchannels.push_front(Channel(id, parent));
					parent = &parent->subchannels.front();
				}
				else // Other channels need to be sorted
				{
					// Find the channel after which this channel is sorted
					std::list<Channel>::iterator orderIt;
					for(orderIt = parent->subchannels.begin(); orderIt != parent->subchannels.end() && orderIt->id != order; ++orderIt);

					if(orderIt != parent->subchannels.end())
					{
						// If the channel was found, add it after
						orderIt++;
						parent = &(*parent->subchannels.insert(orderIt, Channel(id, parent)));
					}
					else
					{
						// If the channel was not found, add it to the back
						parent->subchannels.push_back(Channel(id, parent));
						parent = &parent->subchannels.back();
					}
				}
			}
		}
	}

	ts3Functions.freeMemory(channels);
	return 0;
}
