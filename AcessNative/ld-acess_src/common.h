/*
 */
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern int	Binary_GetSymbol(const char *SymbolName, uintptr_t *Value, size_t *Size, void *IgnoreBase);
extern void	*Binary_LoadLibrary(const char *Path);
extern void	*Binary_Load(const char *Path, uintptr_t *EntryPoint);
extern void	Binary_SetReadyToUse(void *Base);

// HACKS - So this can share the usermode elf.c
static inline int GetSymbol(const char *sym, void **val, size_t *sz, void *IgnoreBase)
{
	uintptr_t rv;
	if( !Binary_GetSymbol(sym, &rv, sz, IgnoreBase) )
		return 0;
	*val = (void*)rv;
	return 1;
}
static inline void *LoadLibrary(const char *Name, const char *SearchPath, char **envp)
{
	return Binary_LoadLibrary(Name);
}
static inline void AddLoaded(const char *Path, void *Base)
{
	Binary_SetReadyToUse(Base);
}

static inline int _SysSetMemFlags(uintptr_t Addr, unsigned int flags, unsigned int mask)
{
	return 0;
}


extern int	AllocateMemory(uintptr_t VirtAddr, size_t ByteCount);
extern uintptr_t	FindFreeRange(size_t ByteCount, int MaxBits);

extern void	Warning(const char *Format, ...);
extern void	Notice(const char *Format, ...);
extern void	Debug(const char *Format, ...);
#define SysDebug	Debug

#define ACESS_SEEK_CUR	0
#define ACESS_SEEK_SET	1
#define ACESS_SEEK_END	-1

#include "exports.h"

typedef struct sBinFmt {
	struct sBinFmt	*Next;
	char	*Name;
	void	*(*Load)(int fd);
	uintptr_t	(*Relocate)(void *base);
	 int	(*GetSymbol)(void*,char*,uintptr_t*,size_t*);
}	tBinFmt;

#endif

