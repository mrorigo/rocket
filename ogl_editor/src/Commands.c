#include "Commands.h"
#include "RemoteConnection.h"
#include "Types.h"
#include "../../sync/sync.h"
#include "../../sync/track.h"
#include <stdlib.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static struct sync_track** s_syncTracks;
static struct TrackData* s_trackData;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Command
{
	void* userData;
	void (*exec)(void* userData);
	void (*undo)(void* userData);
	struct Command* next;
	struct Command* prev;
} Command;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct CommandList
{
	Command* first;
	Command* last;
} CommandList;

static CommandList s_undoStack;
static CommandList s_redoStack;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_addEntry(CommandList* commandList, Command* command);
//static void CommandList_delEntry(CommandList* commandList, Command* command);
static void CommandList_clear(CommandList* commandList);
static bool CommandList_isEmpty(CommandList* list);
static void CommandList_pop(CommandList* commandList);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MultiCommandData
{
	CommandList list;
};

static struct MultiCommandData* s_multiCommand = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_init(struct sync_track** syncTracks, struct TrackData* trackData)
{
	s_syncTracks = syncTracks;
	s_trackData = trackData;

	memset(&s_undoStack, 0, sizeof(CommandList));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int countEntriesInList(CommandList* list)
{
	Command* command;
	int count = 0;

	for (command = list->first; command; command = command->next) 
		count++;

	return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void execCommand(Command* command)
{
	// set if we have multi command recording enabled)
	if (s_multiCommand)
	{
		printf("cmd add to multicommand %p\n", command);
		CommandList_addEntry(&s_multiCommand->list, command);
	}
	else
	{
		printf("cmd add to undoStack %p\n", command);
		CommandList_addEntry(&s_undoStack, command);
		printf("undo stack size %d\n", countEntriesInList(&s_undoStack));
		command->exec(command->userData);
	}

	CommandList_clear(&s_redoStack);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void execMultiCommand(void* userData)
{
	Command* command;
	struct MultiCommandData* data = (struct MultiCommandData*)userData;

	for (command = data->list.first; command; command = command->next) 
		command->exec(command->userData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void undoMultiCommand(void* userData)
{
	Command* command;
	struct MultiCommandData* data = (struct MultiCommandData*)userData;

	for (command = data->list.first; command; command = command->next) 
		command->undo(command->userData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_beginMulti(const char* name)
{
	s_multiCommand = malloc(sizeof(struct MultiCommandData));
	memset(s_multiCommand, 0, sizeof(struct MultiCommandData)); 
	printf("multi_cmd: %s start %p\n", name, s_multiCommand);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_endMulti()
{
	Command* command;


	// Check if any command was added during multi command

	if (CommandList_isEmpty(&s_multiCommand->list))
	{
		printf("multi_cmd: end (nothing to add)\n");
		free(s_multiCommand);
		s_multiCommand = 0;
		return;
	}

	printf("multi_cmd: end %p\n", s_multiCommand);

	command = malloc(sizeof(Command));
	memset(command, 0, sizeof(Command));

	command->userData = s_multiCommand; 
	command->exec = execMultiCommand;
	command->undo = undoMultiCommand;

	s_multiCommand = 0;

	execCommand(command);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DeleteKeyData
{
	int track;
	int row;
	struct track_key oldKey;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void execDeleteKey(void* userData)
{
	struct DeleteKeyData* data = (struct DeleteKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	int idx = sync_find_key(t, data->row);

	data->oldKey = t->keys[idx];
	sync_del_key(t, data->row);

	printf("del_cmd exec: %d %d\n", data->track, data->row); 

	RemoteConnection_sendDeleteKeyCommand(t->name, data->row);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void undoDeleteKey(void* userData)
{
	struct DeleteKeyData* data = (struct DeleteKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	sync_set_key(t, &data->oldKey);

	printf("del_cmd undo: %d %d\n", data->track, data->oldKey.row); 

	RemoteConnection_sendSetKeyCommand(t->name, &data->oldKey);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_deleteKey(int track, int row)
{
	struct DeleteKeyData* data;
	Command* command;
	struct sync_track* t = s_syncTracks[track];
	
	if (!is_key_frame(t, row))
		return;
	
	command = malloc(sizeof(Command));
	memset(command, 0, sizeof(Command));

	command->userData = data = malloc(sizeof(struct DeleteKeyData));
	command->exec = execDeleteKey;
	command->undo = undoDeleteKey;
	data->track = track;
	data->row = row;

	printf("del_cmd add: %p %d %d\n", command, track, row); 

	execCommand(command);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct UpdateKeyData
{
	int track;
	struct track_key key;
	struct track_key oldKey;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void execUpdateKey(void* userData)
{
	struct UpdateKeyData* data = (struct UpdateKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	int idx = sync_find_key(t, data->key.row);

	data->oldKey = t->keys[idx];
	sync_set_key(t, &data->key);

	RemoteConnection_sendSetKeyCommand(t->name, &data->key);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void undoUpdateKey(void* userData)
{
	struct UpdateKeyData* data = (struct UpdateKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	sync_set_key(t, &data->oldKey);

	RemoteConnection_sendSetKeyCommand(t->name, &data->oldKey);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_updateKey(int track, struct track_key* key)
{
	struct UpdateKeyData* data;
	Command* command;
	
	command = malloc(sizeof(Command));
	memset(command, 0, sizeof(Command));

	command->userData = data = malloc(sizeof(struct UpdateKeyData));
	command->exec = execUpdateKey;
	command->undo = undoUpdateKey;
	data->track = track;
	data->key = *key;

	execCommand(command);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct InsertKeyData
{
	int track;
	struct track_key key;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void execInsertKey(void* userData)
{
	struct InsertKeyData* data = (struct InsertKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	sync_set_key(t, &data->key);

	RemoteConnection_sendSetKeyCommand(t->name, &data->key);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void undoInsertKey(void* userData)
{
	struct InsertKeyData* data = (struct InsertKeyData*)userData;
	struct sync_track* t = s_syncTracks[data->track];
	sync_del_key(t, data->key.row);

	RemoteConnection_sendDeleteKeyCommand(t->name, data->key.row);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_addOrUpdateKey(int track, struct track_key* key)
{
	struct InsertKeyData* data;
	Command* command;
	struct sync_track* t = s_syncTracks[track];

	if (is_key_frame(t, key->row))
	{
		Commands_updateKey(track, key);
		return;
	}

	command = malloc(sizeof(Command));
	memset(command, 0, sizeof(Command));

	command->userData = data = malloc(sizeof(struct InsertKeyData));
	command->exec = execInsertKey;
	command->undo = undoInsertKey;
	data->track = track;
	data->key = *key;

	execCommand(command);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_undo()
{
	Command* command;

	printf("calling undo\n");

	if (CommandList_isEmpty(&s_undoStack))
	{
		printf("Thinks undo stack is empty.\n");
		return;
	}

	printf("undo stack size before pop %d\n", countEntriesInList(&s_undoStack)); 

	command = s_undoStack.last;
	CommandList_pop(&s_undoStack);

	printf("undo stack size after pop %d\n", countEntriesInList(&s_undoStack)); 

	command->prev = 0;
	command->next = 0;

	CommandList_addEntry(&s_redoStack, command);

	command->undo(command->userData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Commands_redo()
{
	Command* command;

	if (CommandList_isEmpty(&s_redoStack))
		return;

	command = s_redoStack.last;
	CommandList_pop(&s_redoStack);

	command->prev = 0;
	command->next = 0;

	CommandList_addEntry(&s_undoStack, command);

	command->exec(command->userData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_addEntry(CommandList* list, Command* command)
{
	if (list->last) 
	{
		list->last->next = command;
		command->prev = list->last;
		list->last = command;
	}
	else 
	{
		list->first = command;
		list->last = command;
	}	
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_unlinkEntry(CommandList* list, Command* command)
{
	Command* prev;
	Command* next;

    prev = command->prev;
    next = command->next;

    if (prev) 
    {
        if (next) 
        {
            prev->next = next;
            next->prev = prev;
        }
        else 
        {
            prev->next = 0;
            list->last = prev;
        }
    }
    else 
    {
        if (next) 
        {
            next->prev = 0;
            list->first = next;
        }
        else 
        {
            list->first = 0;
            list->last = 0;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_delEntry(CommandList* list, Command* command)
{
	CommandList_unlinkEntry(list, command);

	free(command->userData);
    free(command);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_clear(CommandList* list)
{
	while (list->last)
	{
		CommandList_delEntry(list, list->last);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandList_pop(CommandList* list)
{
	CommandList_unlinkEntry(list, list->last);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool CommandList_isEmpty(CommandList* list)
{
	return (!list->first && !list->last);
}


