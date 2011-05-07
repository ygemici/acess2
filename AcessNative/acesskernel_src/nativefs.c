/*
 * Acess2 Native Kernel
 * - Acess kernel emulation on another OS using SDL and UDP
 *
 * nativefs.c
 * - Host filesystem access
 */
#define DEBUG	1
#include <acess.h>	// Acess
#include <vfs.h>	// Acess
#include <dirent.h>	// Posix
#include <sys/stat.h>	// Posix
#include <stdio.h>	// Posix

//NOTES:
// tVFS_Node->ImplPtr is a pointer to the filesystem flags (tNativeFS)
// tVFS_Node->Data is the path string (heap string)
// tVFS_Node->ImplInt is the path length

// === STRUCTURES ===
typedef struct
{
	 int	InodeHandle;
	 int	bReadOnly;
}	tNativeFS;

// === PROTOTYPES ===
 int	NativeFS_Install(char **Arguments);
tVFS_Node	*NativeFS_Mount(const char *Device, const char **Arguments);
void	NativeFS_Unmount(tVFS_Node *Node);
tVFS_Node	*NativeFS_FindDir(tVFS_Node *Node, const char *Name);
char	*NativeFS_ReadDir(tVFS_Node *Node, int Position);
Uint64	NativeFS_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);

// === GLOBALS ===
tVFS_Driver	gNativeFS_Driver = {
	"nativefs", 0,
	NativeFS_Mount,	NativeFS_Unmount,
	NULL,
};

// === CODE ===
int NativeFS_Install(char **Arguments)
{
	VFS_AddDriver(&gNativeFS_Driver);
	return 0;
}

tVFS_Node *NativeFS_Mount(const char *Device, const char **Arguments)
{
	tVFS_Node	*ret;
	tNativeFS	*info;
	DIR	*dp;
	
	dp = opendir(Device);
	if(!dp)	return NULL;
	
	// Check if directory exists
	// Parse flags from arguments
	info = malloc(sizeof(tNativeFS));
	info->InodeHandle = Inode_GetHandle();
	info->bReadOnly = 0;
	// Create node
	ret = malloc(sizeof(tVFS_Node));
	memset(ret, 0, sizeof(tVFS_Node));
	ret->Data = strdup(Device);
	ret->ImplInt = strlen(ret->Data);
	ret->ImplPtr = info;
	ret->Inode = (Uint64)dp;
	
	ret->FindDir = NativeFS_FindDir;
	ret->ReadDir = NativeFS_ReadDir;
	
	return ret;
}

void NativeFS_Unmount(tVFS_Node *Node)
{
	tNativeFS	*info = Node->ImplPtr;
	Inode_ClearCache( info->InodeHandle );
	closedir( (void *)Node->Inode );
	free(Node->Data);
	free(Node);
	free(info);
}

void NativeFS_Close(tVFS_Node *Node)
{
	tNativeFS	*info = Node->ImplPtr;
	Inode_UncacheNode( info->InodeHandle, Node->Inode );
}

tVFS_Node *NativeFS_FindDir(tVFS_Node *Node, const char *Name)
{
	char	*path = malloc(Node->ImplInt + 1 + strlen(Name) + 1);
	tNativeFS	*info = Node->ImplPtr;
	tVFS_Node	baseRet;
	struct stat statbuf;

	ENTER("pNode sName", Node, Name);
	
	// Create path
	strcpy(path, Node->Data);
	path[Node->ImplInt] = '/';
	strcpy(path + Node->ImplInt + 1, Name);
	
	LOG("path = '%s'", path);
	
	// Check if file exists
	if( stat(path, &statbuf) ) {
		free(path);
		LOG("Doesn't exist");
		LEAVE('n');
		return NULL;
	}
	
	memset(&baseRet, 0, sizeof(tVFS_Node));
	
	// Check file type
	if( S_ISDIR(statbuf.st_mode) )
	{
		LOG("Directory");
		baseRet.Inode = (Uint64) opendir(path);
		baseRet.FindDir = NativeFS_FindDir;
		baseRet.ReadDir = NativeFS_ReadDir;
		baseRet.Flags |= VFS_FFLAG_DIRECTORY;
	}
	else
	{
		LOG("File");
		baseRet.Inode = (Uint64) fopen(path, "r+");
		baseRet.Read = NativeFS_Read;
	}
	
	// Create new node
	baseRet.ImplPtr = info;
	baseRet.ImplInt = strlen(path);
	baseRet.Data = path;
	
	LEAVE('-');
	return Inode_CacheNode(info->InodeHandle, &baseRet);
}

char *NativeFS_ReadDir(tVFS_Node *Node, int Position)
{
	// Keep track of the current directory position
	return NULL;
}

Uint64 NativeFS_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);
	if( fseek( (void *)Node->Inode, Offset, SEEK_SET ) != 0 )
	{
		LEAVE('i', 0);
		return 0;
	}
	LEAVE('-');
	return fread( Buffer, 1, Length, (void *)Node->Inode );
}
