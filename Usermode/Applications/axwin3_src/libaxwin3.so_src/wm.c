/*
 * AxWin3 Interface Library
 * - By John Hodge (thePowersGang)
 *
 * wm.c
 * - Core window management functions
 */
#include <axwin3/axwin.h>
#include <stdlib.h>
#include <string.h>
#include "include/internal.h"
#include "include/ipc.h"

#define WINDOWS_PER_ALLOC	(63)

typedef struct sWindowBlock	tWindowBlock;

typedef struct sAxWin3_Window	tWindow;

// === STRUCTURES ===
struct sWindowBlock
{
	tWindowBlock	*Next;
	tWindow	*Windows[WINDOWS_PER_ALLOC];
};

// === GLOBALS ===
 int	giAxWin3_LowestFreeWinID;
 int	giAxWin3_HighestUsedWinID;
tWindowBlock	gAxWin3_WindowList;

// === CODE ===
tWindow *AxWin3_int_CreateWindowStruct(uint32_t ServerID, int ExtraBytes)
{
	tWindow	*ret;
	
	ret = calloc(sizeof(tWindow) + ExtraBytes, 1);
	ret->ServerID = ServerID;

	return ret;
}

tWindow *AxWin3_int_AllocateWindowInfo(int DataBytes, int *WinID)
{
	 int	idx, newWinID;
	tWindowBlock *block, *prev;
	tWindow	*ret;	

	block = &gAxWin3_WindowList;
	newWinID = giAxWin3_LowestFreeWinID;
	for( idx = 0; block; newWinID ++ )
	{
		if( block->Windows[idx] == NULL )
			break;
		idx ++;
		if(idx == WINDOWS_PER_ALLOC) {
			prev = block;
			block = block->Next;
			idx = 0;
		}
	}
	
	if( !block )
	{
		block = calloc(sizeof(tWindowBlock), 1);
		prev->Next = block;
		idx = 0;
	}
	
	ret = block->Windows[idx] = AxWin3_int_CreateWindowStruct(newWinID, DataBytes);
	if(newWinID > giAxWin3_HighestUsedWinID)
		giAxWin3_HighestUsedWinID = newWinID;
	if(newWinID == giAxWin3_LowestFreeWinID)
		giAxWin3_HighestUsedWinID ++;

	if(WinID)	*WinID = newWinID;

	return ret;
}

tHWND AxWin3_CreateWindow(
	tHWND Parent, const char *Renderer, int RendererArg,
	int DataBytes, tAxWin3_WindowMessageHandler MessageHandler
	)
{
	tWindow	*ret;
	 int	newWinID;
	 int	dataSize = sizeof(tIPCMsg_CreateWin) + strlen(Renderer) + 1;
	tAxWin_IPCMessage	*msg;
	tIPCMsg_CreateWin	*create_win;

	// Allocate a window ID
	ret = AxWin3_int_AllocateWindowInfo(DataBytes, &newWinID);
	if(!ret)	return NULL;
	ret->Handler = MessageHandler;

	// Create message
	msg = AxWin3_int_AllocateIPCMessage(Parent, IPCMSG_CREATEWIN, 0, dataSize);
	create_win = (void*)msg->Data;	
	create_win->NewWinID = newWinID;
	create_win->RendererArg = RendererArg;
	strcpy(create_win->Renderer, Renderer);

	// Send and clean up
	AxWin3_int_SendIPCMessage(msg);
	free(msg);

	// TODO: Detect and handle possible errors

	// Return success
	return ret;
}

void AxWin3_DestroyWindow(tHWND Window)
{
	tAxWin_IPCMessage	*msg;
	
	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_DESTROYWIN, 0, 0);
	AxWin3_int_SendIPCMessage(msg);
	free(msg);
}

void *AxWin3_int_GetDataPtr(tHWND Window)
{
	return Window->Data;
}

void AxWin3_SendMessage(tHWND Window, tHWND Destination, int Message, int Length, void *Data)
{
	tAxWin_IPCMessage	*msg;
	tIPCMsg_SendMsg	*info;
	
	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_SENDMSG, 0, sizeof(*info)+Length);
	info = (void*)msg->Data;
	info->Dest = AxWin3_int_GetWindowID(Destination);
	info->ID = Message;
	info->Length = Length;
	memcpy(info->Data, Data, Length);
	
	AxWin3_int_SendIPCMessage(msg);
	free(msg);
}

void AxWin3_ShowWindow(tHWND Window, int bShow)
{
	tAxWin_IPCMessage	*msg;
	tIPCMsg_ShowWindow	*info;

	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_SHOWWINDOW, 0, sizeof(*info));
	info = (void*)msg->Data;
	info->bShow = !!bShow;
	
	AxWin3_int_SendIPCMessage(msg);
	
	free(msg);
}

void AxWin3_SetWindowPos(tHWND Window, short X, short Y, short W, short H)
{
	tAxWin_IPCMessage	*msg;
	tIPCMsg_SetWindowPos	*info;
	
	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_SETWINPOS, 0, sizeof(*info));
	info = (void*)msg->Data;

	info->bSetPos = 1;
	info->bSetDims = 1;
	info->X = X;	info->Y = Y;
	info->W = W;	info->H = H;

	AxWin3_int_SendIPCMessage(msg);	
	free(msg);
}

void AxWin3_MoveWindow(tHWND Window, short X, short Y)
{
	tAxWin_IPCMessage	*msg;
	tIPCMsg_SetWindowPos	*info;
	
	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_SETWINPOS, 0, sizeof(*info));
	info = (void*)msg->Data;

	info->bSetPos = 1;
	info->bSetDims = 0;
	info->X = X;
	info->Y = Y;
	
	AxWin3_int_SendIPCMessage(msg);
	
	free(msg);
}

void AxWin3_ResizeWindow(tHWND Window, short W, short H)
{
	tAxWin_IPCMessage	*msg;
	tIPCMsg_SetWindowPos	*info;
	
	msg = AxWin3_int_AllocateIPCMessage(Window, IPCMSG_SETWINPOS, 0, sizeof(*info));
	info = (void*)msg->Data;

	info->bSetPos = 0;
	info->bSetDims = 1;
	info->W = W;
	info->H = H;
	
	AxWin3_int_SendIPCMessage(msg);
	
	free(msg);
}
