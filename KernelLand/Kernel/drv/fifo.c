/* AcessOS
 * FIFO Pipe Driver
 */
#define DEBUG	0
#include <acess.h>
#include <modules.h>
#include <fs_devfs.h>
#include <semaphore.h>

// === CONSTANTS ===
#define DEFAULT_RING_SIZE	2048
#define PF_BLOCKING		1

// === TYPES ===
typedef struct sPipe {
	struct sPipe	*Next;
	char	*Name;
	tVFS_Node	Node;
	Uint	Flags;
	 int	ReadPos;
	 int	WritePos;
	 int	BufSize;
	char	*Buffer;
} tPipe;

// === PROTOTYPES ===
 int	FIFO_Install(char **Arguments);
 int	FIFO_IOCtl(tVFS_Node *Node, int Id, void *Data);
char	*FIFO_ReadDir(tVFS_Node *Node, int Id);
tVFS_Node	*FIFO_FindDir(tVFS_Node *Node, const char *Filename);
 int	FIFO_MkNod(tVFS_Node *Node, const char *Name, Uint Flags);
void	FIFO_Reference(tVFS_Node *Node);
void	FIFO_Close(tVFS_Node *Node);
 int	FIFO_Relink(tVFS_Node *Node, const char *OldName, const char *NewName);
Uint64	FIFO_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);
Uint64	FIFO_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, const void *Buffer);
tPipe	*FIFO_Int_NewPipe(int Size, const char *Name);

// === GLOBALS ===
MODULE_DEFINE(0, 0x0032, FIFO, FIFO_Install, NULL, NULL);
tVFS_NodeType	gFIFO_DirNodeType = {
	.TypeName = "FIFO Dir Node",
	.ReadDir = FIFO_ReadDir,
	.FindDir = FIFO_FindDir,
	.MkNod = FIFO_MkNod,
	.Relink = FIFO_Relink,
	.IOCtl = FIFO_IOCtl
};
tVFS_NodeType	gFIFO_PipeNodeType = {
	.TypeName = "FIFO Pipe Node",
	.Read = FIFO_Read,
	.Write = FIFO_Write,
	.Close = FIFO_Close,
	.Reference = FIFO_Reference
};
tDevFS_Driver	gFIFO_DriverInfo = {
	NULL, "fifo",
	{
	.Size = 1,
	.NumACLs = 1,
	.ACLs = &gVFS_ACL_EveryoneRW,
	.Flags = VFS_FFLAG_DIRECTORY,
	.Type = &gFIFO_DirNodeType
	}
};
tVFS_Node	gFIFO_AnonNode = {
	.NumACLs = 1,
	.ACLs = &gVFS_ACL_EveryoneRW,
	};
tPipe	*gFIFO_NamedPipes = NULL;

// === CODE ===
/**
 * \fn int FIFO_Install(char **Options)
 * \brief Installs the FIFO Driver
 */
int FIFO_Install(char **Options)
{
	DevFS_AddDevice( &gFIFO_DriverInfo );
	return MODULE_ERR_OK;
}

/**
 * \fn int FIFO_IOCtl(tVFS_Node *Node, int Id, void *Data)
 */
int FIFO_IOCtl(tVFS_Node *Node, int Id, void *Data)
{
	return 0;
}

/**
 * \fn char *FIFO_ReadDir(tVFS_Node *Node, int Id)
 * \brief Reads from the FIFO root
 */
char *FIFO_ReadDir(tVFS_Node *Node, int Id)
{
	tPipe	*tmp = gFIFO_NamedPipes;
	
	// Entry 0 is Anon Pipes
	if(Id == 0)	return strdup("anon");
	
	// Find the id'th node
	while(--Id && tmp)	tmp = tmp->Next;
	// If node found, return it
	if(tmp)	return strdup(tmp->Name);
	// else error return
	return NULL;
}

/**
 * \fn tVFS_Node *FIFO_FindDir(tVFS_Node *Node, const char *Filename)
 * \brief Find a file in the FIFO root
 * \note Creates an anon pipe if anon is requested
 */
tVFS_Node *FIFO_FindDir(tVFS_Node *Node, const char *Filename)
{
	tPipe	*tmp;
	if(!Filename)	return NULL;
	
	// NULL String Check
	if(Filename[0] == '\0')	return NULL;
	
	// Anon Pipe
	if(Filename[0] == 'a' && Filename[1] == 'n'
	&& Filename[2] == 'o' && Filename[3] == 'n'
	&& Filename[4] == '\0') {
		tmp = FIFO_Int_NewPipe(DEFAULT_RING_SIZE, "anon");
		return &tmp->Node;
	}
	
	// Check Named List
	tmp = gFIFO_NamedPipes;
	while(tmp)
	{
		if(strcmp(tmp->Name, Filename) == 0)
			return &tmp->Node;
		tmp = tmp->Next;
	}
	return NULL;
}

/**
 * \fn int FIFO_MkNod(tVFS_Node *Node, const char *Name, Uint Flags)
 */
int FIFO_MkNod(tVFS_Node *Node, const char *Name, Uint Flags)
{
	return 0;
}

void FIFO_Reference(tVFS_Node *Node)
{
	if(!Node->ImplPtr)	return ;
	
	Node->ReferenceCount ++;
}

/**
 * \fn void FIFO_Close(tVFS_Node *Node)
 * \brief Close a FIFO end
 */
void FIFO_Close(tVFS_Node *Node)
{
	tPipe	*pipe;
	if(!Node->ImplPtr)	return ;
	
	Node->ReferenceCount --;
	if(Node->ReferenceCount)	return ;
	
	pipe = Node->ImplPtr;
	
	if(strcmp(pipe->Name, "anon") == 0) {
		Log_Debug("FIFO", "Pipe %p closed", Node->ImplPtr);
		free(Node->ImplPtr);
		return ;
	}
	
	return ;
}

/**
 * \fn int FIFO_Relink(tVFS_Node *Node, const char *OldName, const char *NewName)
 * \brief Relink a file (Deletes named pipes)
 */
int FIFO_Relink(tVFS_Node *Node, const char *OldName, const char *NewName)
{
	tPipe	*pipe, *tmp;
	
	if(Node != &gFIFO_DriverInfo.RootNode)	return 0;
	
	// Can't relink anon
	if(strcmp(OldName, "anon"))	return 0;
	
	// Find node
	for(pipe = gFIFO_NamedPipes;
		pipe;
		pipe = pipe->Next)
	{
		if(strcmp(pipe->Name, OldName) == 0)
			break;
	}
	if(!pipe)	return 0;
	
	// Relink a named pipe
	if(NewName) {
		// Check new name
		for(tmp = gFIFO_NamedPipes;
			tmp;
			tmp = tmp->Next)
		{
			if(strcmp(tmp->Name, NewName) == 0)	return 0;
		}
		// Create new name
		free(pipe->Name);
		pipe->Name = malloc(strlen(NewName)+1);
		strcpy(pipe->Name, NewName);
		return 1;
	}
	
	// Unlink the pipe
	if(Node->ImplPtr) {
		free(Node->ImplPtr);
		return 1;
	}
	
	return 0;
}

/**
 * \fn Uint64 FIFO_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Read from a fifo pipe
 */
Uint64 FIFO_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	tPipe	*pipe = Node->ImplPtr;
	Uint	len;
	Uint	remaining = Length;

	if(!pipe)	return 0;
	
	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);
	
	while(remaining)
	{
		// Wait for buffer to fill
		if(pipe->Flags & PF_BLOCKING)
		{
			if( pipe->ReadPos == pipe->WritePos )
				VFS_SelectNode(Node, VFS_SELECT_READ, NULL, "FIFO_Read");
			
		}
		else
		{
			if(pipe->ReadPos == pipe->WritePos)
			{
				VFS_MarkAvaliable(Node, 0);
				LEAVE('i', 0);
				return 0;
			}
		}
	
		len = remaining;
		if( pipe->ReadPos < pipe->WritePos )
		{
			 int	avail_bytes = pipe->WritePos - pipe->ReadPos;
			if( avail_bytes < remaining )	len = avail_bytes;
		}
		else
		{
			 int	avail_bytes = pipe->WritePos + pipe->BufSize - pipe->ReadPos;
			if( avail_bytes < remaining )	len = avail_bytes;
		}

		LOG("len = %i, remaining = %i", len, remaining);		

		// Check if read overflows buffer
		if(len > pipe->BufSize - pipe->ReadPos)
		{
			int ofs = pipe->BufSize - pipe->ReadPos;
			memcpy(Buffer, &pipe->Buffer[pipe->ReadPos], ofs);
			memcpy((Uint8*)Buffer + ofs, &pipe->Buffer, len-ofs);
		}
		else
		{
			memcpy(Buffer, &pipe->Buffer[pipe->ReadPos], len);
		}
		
		// Increment read position
		pipe->ReadPos += len;
		pipe->ReadPos %= pipe->BufSize;
		
		// Mark some flags
		if( pipe->ReadPos == pipe->WritePos ) {
			VFS_MarkAvaliable(Node, 0);
		}
		VFS_MarkFull(Node, 0);	// Buffer can't still be full
		
		// Decrement Remaining Bytes
		remaining -= len;
		// Increment Buffer address
		Buffer = (Uint8*)Buffer + len;
		
		// TODO: Option to read differently
		LEAVE('i', len);
		return len;
	}

	LEAVE('i', Length);
	return Length;

}

/**
 * \fn Uint64 FIFO_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Write to a fifo pipe
 */
Uint64 FIFO_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, const void *Buffer)
{
	tPipe	*pipe = Node->ImplPtr;
	Uint	len;
	Uint	remaining = Length;
	
	if(!pipe)	return 0;

	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);
	
	while(remaining)
	{
		// Wait for buffer to empty
		if(pipe->Flags & PF_BLOCKING) {
			if( pipe->ReadPos == (pipe->WritePos+1)%pipe->BufSize )
				VFS_SelectNode(Node, VFS_SELECT_WRITE, NULL, "FIFO_Write");

			len = remaining;
			if( pipe->ReadPos > pipe->WritePos )
			{
				 int	rem_space = pipe->ReadPos - pipe->WritePos;
				if(rem_space < remaining)	len = rem_space;
			}
			else
			{
				 int	rem_space = pipe->ReadPos + pipe->BufSize - pipe->WritePos;
				if(rem_space < remaining)	len = rem_space;
			}
		}
		else
		{
			if(pipe->ReadPos == (pipe->WritePos+1)%pipe->BufSize)
			{
				LEAVE('i', 0);
				return 0;
			}
			// Write buffer
			if(pipe->ReadPos - pipe->WritePos < remaining)
				len = pipe->ReadPos - pipe->WritePos;
			else
				len = remaining;
		}
		
		// Check if write overflows buffer
		if(len > pipe->BufSize - pipe->WritePos)
		{
			int ofs = pipe->BufSize - pipe->WritePos;
			memcpy(&pipe->Buffer[pipe->WritePos], Buffer, ofs);
			memcpy(&pipe->Buffer, (Uint8*)Buffer + ofs, len-ofs);
		}
		else
		{
			memcpy(&pipe->Buffer[pipe->WritePos], Buffer, len);
		}
		
		// Increment read position
		pipe->WritePos += len;
		pipe->WritePos %= pipe->BufSize;
		
		// Mark some flags
		if( pipe->ReadPos == pipe->WritePos ) {
			VFS_MarkFull(Node, 1);	// Buffer full
		}
		VFS_MarkAvaliable(Node, 1);
		
		// Decrement Remaining Bytes
		remaining -= len;
		// Increment Buffer address
		Buffer = (Uint8*)Buffer + len;
	}

	LEAVE('i', Length);
	return Length;
}

// --- HELPERS ---
/**
 * \fn tPipe *FIFO_Int_NewPipe(int Size, const char *Name)
 * \brief Create a new pipe
 */
tPipe *FIFO_Int_NewPipe(int Size, const char *Name)
{
	tPipe	*ret;
	 int	namelen = strlen(Name) + 1;
	 int	allocsize = sizeof(tPipe) + sizeof(tVFS_ACL) + Size + namelen;
	
	ret = calloc(1, allocsize);
	if(!ret)	return NULL;
	
	// Clear Return
	ret->Flags = PF_BLOCKING;
	
	// Allocate Buffer
	ret->BufSize = Size;
	ret->Buffer = (void*)( (Uint)ret + sizeof(tPipe) + sizeof(tVFS_ACL) );
	
	// Set name (and FIFO name)
	ret->Name = ret->Buffer + Size;
	strcpy(ret->Name, Name);
	// - Start empty, max of `Size`
	//Semaphore_Init( &ret->Semaphore, 0, Size, "FIFO", ret->Name );
	
	// Set Node
	ret->Node.ReferenceCount = 1;
	ret->Node.Size = 0;
	ret->Node.ImplPtr = ret;
	ret->Node.UID = Threads_GetUID();
	ret->Node.GID = Threads_GetGID();
	ret->Node.NumACLs = 1;
	ret->Node.ACLs = (void*)( (Uint)ret + sizeof(tPipe) );
		ret->Node.ACLs->Group = 0;
		ret->Node.ACLs->ID = ret->Node.UID;
		ret->Node.ACLs->Inv = 0;
		ret->Node.ACLs->Perms = -1;
	ret->Node.CTime
		= ret->Node.MTime
		= ret->Node.ATime = now();
	ret->Node.Type = &gFIFO_PipeNodeType;
	
	return ret;
}