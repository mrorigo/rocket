#include "TrackData.h"
#include "rlog.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int TrackData_createGetTrack(TrackData* trackData, const char* name)
{
	int index = sync_find_track(&trackData->syncData, name); 
	if (index < 0)
	{
        index = sync_create_track(&trackData->syncData, name);
		memset(&trackData->tracks[index], 0, sizeof(Track));
		trackData->tracks[index].index = index;
		trackData->tracks[index].color = TrackData_getNextColor(trackData); 
	}

	return index;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t s_colors[] =
{
	0xffb27474,
	0xffb28050,
	0xffa9b250,
	0xff60b250,

	0xff4fb292,
	0xff4f71b2,
	0xff8850b2,
	0xffb25091,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TrackData_getNextColor(TrackData* trackData)
{
	return s_colors[(trackData->lastColor++) & 0x7];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int findSeparator(const char* name)
{
	int i, len = strlen(name);

	for (i = 0; i < len; ++i)
	{
		if (name[i] == ':')
			return i;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Group* findOrCreateGroup(const char* name, TrackData* trackData)
{
	Group* group;
	int i, group_count = trackData->groupCount;
	Group* groups = trackData->groups;

	for (i = 0; i < group_count; ++i)
	{
		group = &groups[i];

		if (!group->name)
			continue;

		if (!strcmp(name, group->name))
			return &groups[i];
	}

	group = &groups[trackData->groupCount++];
	memset(group, 0, sizeof(Group));

	group->type = GROUP_TYPE_GROUP;
	group->name = strdup(name);
	group->displayName = strdup(name);
	group->displayName[strlen(name)-1] = 0;
	group->trackCount = 0;

	return group;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrackData_linkTrack(int index, const char* name, TrackData* trackData)
{
	int found;
	char group_name[256];
	Group* group;
	Group* groups = trackData->groups;
	Track* track = &trackData->tracks[index];

	if (track->group)
		return;

	found = findSeparator(name);

	if (found == -1)
	{
		Group* group = &groups[trackData->groupCount++];
		memset(group, 0, sizeof(Group));

		group->type = GROUP_TYPE_TRACK;
		group->t.track = track;
		group->trackCount = 1;
		track->group = group;
		track->displayName = strdup(name); 
		return;
	}

	memset(group_name, 0, sizeof(group_name));
	memcpy(group_name, name, found + 1); 

	group = findOrCreateGroup(group_name, trackData); 

	if (group->trackCount == 0)
		group->t.tracks = (Track**)malloc(sizeof(Track**));
	else
		group->t.tracks = (Track**)realloc(group->t.tracks, sizeof(Track**) * group->trackCount + 1);

	group->t.tracks[group->trackCount++] = track;

	track->group = group;
	track->displayName = strdup(&name[found + 1]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrackData_linkGroups(TrackData* trackData)
{
	int i, track_count;
	struct sync_data* sync = &trackData->syncData;

	for (i = 0, track_count = sync->num_tracks; i < track_count; ++i)
		TrackData_linkTrack(i, sync->tracks[i]->name, trackData); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrackData_setActiveTrack(TrackData* trackData, int track)
{
	const int current_track = trackData->activeTrack;
	trackData->tracks[current_track].selected = false;
	trackData->tracks[track].selected = true;
	trackData->activeTrack = track;
}

