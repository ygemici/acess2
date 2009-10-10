/*
 * Acess2 Virtual Terminal Driver
 */
#include <common.h>
#include <fs_devfs.h>
#include <modules.h>
#include <tpl_drv_video.h>
#include <tpl_drv_keyboard.h>

// === CONSTANTS ===
#define	NUM_VTS	4
#define MAX_INPUT_CHARS	64
#define VT_SCROLLBACK	1	// 4 Screens of text
#define DEFAULT_OUTPUT	"/Devices/VGA"
#define DEFAULT_INPUT	"/Devices/PS2Keyboard"
#define	DEFAULT_WIDTH	80
#define	DEFAULT_HEIGHT	25
#define	DEFAULT_COLOUR	(VT_COL_BLACK|(0xAAA<<16))

#define	VT_FLAG_HIDECSR	0x01

enum eVT_Modes {
	VT_MODE_TEXT8,	// UTF-8 Text Mode (VT100 Emulation)
	VT_MODE_TEXT32,	// UTF-32 Text Mode (Acess Native)
	VT_MODE_8BPP,	// 256 Colour Mode
	VT_MODE_16BPP,	// 16 bit Colour Mode
	VT_MODE_24BPP,	// 24 bit Colour Mode
	VT_MODE_32BPP,	// 32 bit Colour Mode
	NUM_VT_MODES
};

// === TYPES ===
typedef struct {
	 int	Mode;
	 int	Flags;
	 int	Width, Height;
	 int	ViewPos, WritePos;
	Uint32	CurColour;
	char	Name[2];
	 int	InputRead;
	 int	InputWrite;
	Uint32	InputBuffer[MAX_INPUT_CHARS];
	union {
		tVT_Char	*Text;
		Uint32		*Buffer;
	};
	tVFS_Node	Node;
} tVTerm;

// === PROTOTYPES ===
 int	VT_Install(char **Arguments);
char	*VT_ReadDir(tVFS_Node *Node, int Pos);
tVFS_Node	*VT_FindDir(tVFS_Node *Node, char *Name);
Uint64	VT_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);
Uint64	VT_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);
 int	VT_IOCtl(tVFS_Node *Node, int Id, void *Data);
void	VT_KBCallBack(Uint32 Codepoint);
void	VT_int_PutString(tVTerm *Term, Uint8 *Buffer, Uint Count);
 int	VT_int_ParseEscape(tVTerm *Term, char *Buffer);
void	VT_int_PutChar(tVTerm *Term, Uint32 Ch);
void	VT_int_UpdateScreen( tVTerm *Term, int UpdateAll );

// === CONSTANTS ===
const Uint16	caVT100Colours[] = {
		VT_COL_BLACK, 0x700, 0x070, 0x770, 0x007, 0x707, 0x077, 0xAAA,
		VT_COL_GREY, 0xF00, 0x0F0, 0xFF0, 0x00F, 0xF0F, 0x0FF, VT_COL_WHITE
	};

// === GLOBALS ===
MODULE_DEFINE(0, 0x0032, VTerm, VT_Install, NULL, NULL);
tDevFS_Driver	gVT_DrvInfo = {
	NULL, "VTerm",
	{
	.Flags = VFS_FFLAG_DIRECTORY,
	.Size = NUM_VTS,
	.Inode = -1,
	.NumACLs = 0,
	.ReadDir = VT_ReadDir,
	.FindDir = VT_FindDir
	}
};
tVTerm	gVT_Terminals[NUM_VTS];
char	*gsVT_OutputDevice = NULL;
char	*gsVT_InputDevice = NULL;
 int	giVT_OutputDevHandle = -2;
 int	giVT_InputDevHandle = -2;
 int	giVT_CurrentTerminal = 0;

// === CODE ===
/**
 * \fn int VT_Install(char **Arguments)
 * \brief Installs the Virtual Terminal Driver
 */
int VT_Install(char **Arguments)
{
	char	**args = Arguments;
	char	*arg;
	 int	i;
	
	// Scan Arguments
	if(Arguments)
	{
		for( ; (arg = *args); args++ )
		{
			if(arg[0] != '-')	continue;
			
			switch(arg[1])
			{
			// Set output device
			case 'o':
				if(args[1] ==  NULL)	break;
				if(gsVT_OutputDevice)	free(gsVT_OutputDevice);
				gsVT_OutputDevice = malloc(strlen(args[1])+1);
				strcpy(gsVT_OutputDevice, args[1]);
				args ++;
				break;
			
			// Set input device
			case 'i':
				if(args[1] == NULL)	break;
				if(gsVT_InputDevice)	free(gsVT_InputDevice);
				gsVT_InputDevice = malloc(strlen(args[1])+1);
				strcpy(gsVT_InputDevice, args[1]);
				args ++;
				break;
			
			}
		}
	}
	
	// Apply Defaults
	if(!gsVT_OutputDevice)	gsVT_OutputDevice = DEFAULT_OUTPUT;
	if(!gsVT_InputDevice)	gsVT_InputDevice = DEFAULT_INPUT;
	
	LOG("Using '%s' as output", gsVT_OutputDevice);
	LOG("Using '%s' as input", gsVT_InputDevice);
	
	// Create Nodes
	for( i = 0; i < NUM_VTS; i++ )
	{
		gVT_Terminals[i].Mode = VT_MODE_TEXT8;
		gVT_Terminals[i].Flags = 0;
		gVT_Terminals[i].Width = DEFAULT_WIDTH;
		gVT_Terminals[i].Height = DEFAULT_HEIGHT;
		gVT_Terminals[i].CurColour = DEFAULT_COLOUR;
		gVT_Terminals[i].WritePos = 0;
		gVT_Terminals[i].ViewPos = 0;
		
		gVT_Terminals[i].Buffer = malloc( DEFAULT_WIDTH*DEFAULT_HEIGHT*VT_SCROLLBACK*sizeof(tVT_Char) );
		memset( gVT_Terminals[i].Buffer, 0, DEFAULT_WIDTH*DEFAULT_HEIGHT*VT_SCROLLBACK*sizeof(tVT_Char) );
		
		gVT_Terminals[i].Name[0] = '0'+i;
		gVT_Terminals[i].Name[1] = '\0';
		gVT_Terminals[i].Node.Inode = i;
		gVT_Terminals[i].Node.NumACLs = 0;	// Only root can open virtual terminals
		
		gVT_Terminals[i].Node.Read = VT_Read;
		gVT_Terminals[i].Node.Write = VT_Write;
		gVT_Terminals[i].Node.IOCtl = VT_IOCtl;
	}
	
	// Add to DevFS
	DevFS_AddDevice( &gVT_DrvInfo );
	
	return 0;
}

/**
 * \fn void VT_InitOutput()
 * \brief Initialise Video Output
 */
void VT_InitOutput()
{
	giVT_OutputDevHandle = VFS_Open(gsVT_OutputDevice, VFS_OPENFLAG_WRITE);
	LOG("giVT_OutputDevHandle = %x\n", giVT_OutputDevHandle);
}

/**
 * \fn void VT_InitInput()
 * \brief Initialises the input
 */
void VT_InitInput()
{
	giVT_InputDevHandle = VFS_Open(gsVT_InputDevice, VFS_OPENFLAG_READ);
	LOG("giVT_InputDevHandle = %x\n", giVT_InputDevHandle);
	if(giVT_InputDevHandle == -1)	return ;
	VFS_IOCtl(giVT_InputDevHandle, KB_IOCTL_SETCALLBACK, VT_KBCallBack);
}

/**
 * \fn char *VT_ReadDir(tVFS_Node *Node, int Pos)
 * \brief Read from the VTerm Directory
 */
char *VT_ReadDir(tVFS_Node *Node, int Pos)
{
	if(Pos < 0)	return NULL;
	if(Pos >= NUM_VTS)	return NULL;
	return strdup( gVT_Terminals[Pos].Name );
}

/**
 * \fn tVFS_Node *VT_FindDir(tVFS_Node *Node, char *Name)
 * \brief Find an item in the VTerm directory
 */
tVFS_Node *VT_FindDir(tVFS_Node *Node, char *Name)
{
	 int	num;
	
	//ENTER("pNode sName", Node, Name);
	
	// Open the input and output files if needed
	if(giVT_OutputDevHandle == -2)	VT_InitOutput();
	if(giVT_InputDevHandle == -2)	VT_InitInput();
	
	// Sanity check name
	if(Name[0] < '0' || Name[0] > '9' || Name[1] != '\0') {
		LEAVE('n');
		return NULL;
	}
	// Get index
	num = Name[0] - '0';
	if(num >= NUM_VTS) {
		LEAVE('n');
		return NULL;
	}
	// Return node
	//LEAVE('p', &gVT_Terminals[num].Node);
	return &gVT_Terminals[num].Node;
}

/**
 * \fn Uint64 VT_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Read from a virtual terminal
 */
Uint64 VT_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	 int	pos = 0;
	tVTerm	*term = &gVT_Terminals[ Node->Inode ];
	
	// Check current mode
	switch(term->Mode)
	{
	case VT_MODE_TEXT8:
		while(pos < Length)
		{
			while(term->InputRead == term->InputWrite)	Threads_Yield();
			while(term->InputRead != term->InputWrite)
			{
				pos += WriteUTF8(Buffer+pos, term->InputBuffer[term->InputRead]);
				term->InputRead ++;
				term->InputRead %= MAX_INPUT_CHARS;
			}
		}
		break;
	
	case VT_MODE_TEXT32:
		while(pos < Length)
		{
			while(term->InputRead == term->InputWrite)	Threads_Yield();
			while(term->InputRead != term->InputWrite)
			{
				((Uint32*)Buffer)[pos] = term->InputBuffer[term->InputRead];
				pos ++;
				term->InputRead ++;
				term->InputRead %= MAX_INPUT_CHARS;
			}
		}
		break;
	}
	return 0;
}

/**
 * \fn Uint64 VT_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
 * \brief Write to a virtual terminal
 */
Uint64 VT_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	tVTerm	*term = &gVT_Terminals[ Node->Inode ];
	
	//ENTER("pNode XOffset XLength pBuffer",  Node, Offset, Length, Buffer);
	
	// Write
	switch( term->Mode )
	{
	case VT_MODE_TEXT8:
		VT_int_PutString(term, Buffer, Length);
		break;
	case VT_MODE_TEXT32:
		//VT_int_PutString32(term, Buffer, Length);
		break;
	}
	
	//LEAVE('i', 0);
	return 0;
}

/**
 * \fn int VT_IOCtl(tVFS_Node *Node, int Id, void *Data)
 * \brief Call an IO Control on a virtual terminal
 */
int VT_IOCtl(tVFS_Node *Node, int Id, void *Data)
{
	return 0;
}

/**
 * \fn void VT_KBCallBack(Uint32 Codepoint)
 * \brief Called on keyboard interrupt
 */
void VT_KBCallBack(Uint32 Codepoint)
{
	tVTerm	*term = &gVT_Terminals[giVT_CurrentTerminal];
	
	term->InputBuffer[ term->InputWrite ] = Codepoint;
	term->InputWrite ++;
	term->InputWrite %= MAX_INPUT_CHARS;
	if(term->InputRead == term->InputWrite) {
		term->InputRead ++;
		term->InputRead %= MAX_INPUT_CHARS;
	}
}

/**
 * \fn void VT_int_PutString(tVTerm *Term, Uint8 *Buffer, Uint Count)
 * \brief Print a string to the Virtual Terminal
 */
void VT_int_PutString(tVTerm *Term, Uint8 *Buffer, Uint Count)
{
	Uint32	val;
	 int	i;
	for( i = 0; i < Count; i++ )
	{
		if( Buffer[i] == 0x1B )	// Escape Sequence
		{
			i ++;
			i += VT_int_ParseEscape(Term, (char*)&Buffer[i]);
			continue;
		}
		
		if( Buffer[i] < 128 )	// Plain ASCII
			VT_int_PutChar(Term, Buffer[i]);
		else {	// UTF-8
			i += ReadUTF8(&Buffer[i], &val);
			VT_int_PutChar(Term, val);
		}
	}
	
	// Update cursor
	if( !(Term->Flags & VT_FLAG_HIDECSR) )
	{
		tVideo_IOCtl_Pos	pos;
		pos.x = Term->WritePos % Term->Width;
		pos.y = Term->WritePos / Term->Width;
		VFS_IOCtl(giVT_OutputDevHandle, VIDEO_IOCTL_SETCURSOR, &pos);
	}
}

/**
 * \fn void VT_int_ClearLine(tVTerm *Term, int Num)
 * \brief Clears a line in a virtual terminal
 */
void VT_int_ClearLine(tVTerm *Term, int Num)
{
	 int	i;
	//ENTER("pTerm iNum", Term, Num);
	for( i = Term->Width; i--; )
	{
		Term->Text[ Num*Term->Width + i ].Ch = 0;
		Term->Text[ Num*Term->Width + i ].Colour = Term->CurColour;
	}
	//LEAVE('-');
}

/**
 * \fn int VT_int_ParseEscape(tVTerm *Term, char *Buffer)
 * \brief Parses a VT100 Escape code
 */
int VT_int_ParseEscape(tVTerm *Term, char *Buffer)
{
	char	c;
	 int	argc = 0, j = 1;
	 int	args[4] = {0,0,0,0};
	
	switch(Buffer[0]) {
	//Large Code
	case '[':
		// Get Arguments
		c = Buffer[1];
		do {
			while('0' <= c && c <= '9') {
				args[argc] *= 10;
				args[argc] += c-'0';
				c = Buffer[++j];
			}
			argc ++;
		} while(c == ';');
		
		// Get string (what does this do?)
		if(c == '"') {
			c = Buffer[++j];
			while(c != '"')
				c = Buffer[++j];
		}
		
		// Get Command
		if(	('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))
		{
			switch(c) {
			//Clear By Line
			case 'J':
				// Clear Screen
				switch(args[0])
				{
				case 2:
					{
					 int	i = Term->Height * VT_SCROLLBACK;
					while( i-- )	VT_int_ClearLine(Term, i);
					Term->WritePos = 0;
					Term->ViewPos = 0;
					VT_int_UpdateScreen(Term, 1);
					}
					break;
				}
				break;
			// Set Font flags
			case 'm':
				for( ; argc--; )
				{
					// Flags
					if( 0 <= args[argc] && args[argc] <= 8)
					{
						switch(args[argc])
						{
						case 0:	Term->CurColour = DEFAULT_COLOUR;	break;	// Reset
						case 1:	Term->CurColour |= 0x80000000;	break;	// Bright
						case 2:	Term->CurColour &= ~0x80000000;	break;	// Dim
						}
					}
					// Foreground Colour
					else if(30 <= args[argc] && args[argc] <= 37) {
						Term->CurColour &= 0xF000FFFF;
						Term->CurColour |= (Uint32)caVT100Colours[ args[argc]-30+(Term->CurColour>>28) ] << 16;
					}
					// Background Colour
					else if(40 <= args[argc] && args[argc] <= 47) {
						Term->CurColour &= 0xFFFF8000;
						Term->CurColour |= caVT100Colours[ args[argc]-40+((Term->CurColour>>12)&15) ];
					}
				}
				break;
			}
		}
		break;
		
	default:
		break;
	}
	
	return j + 1;
}

/**
 * \fn void VT_int_PutChar(tVTerm *Term, Uint32 Ch)
 * \brief Write a single character to a VTerm
 */
void VT_int_PutChar(tVTerm *Term, Uint32 Ch)
{
	 int	i;
	//ENTER("pTerm xCh", Term, Ch);
	//LOG("Term = {WritePos:%i, ViewPos:%i}\n", Term->WritePos, Term->ViewPos);
	
	switch(Ch)
	{
	case '\0':	return;	// Ignore NULL byte
	case '\n':
		Term->WritePos += Term->Width;
	case '\r':
		Term->WritePos -= Term->WritePos % Term->Width;
		break;
	
	case '\t':
		do {
			Term->Text[ Term->WritePos ].Ch = '\0';
			Term->Text[ Term->WritePos ].Colour = Term->CurColour;
			Term->WritePos ++;
		} while(Term->WritePos & 7);
		break;
	
	case '\b':
		// Backspace is invalid at Offset 0
		if(Term->WritePos == 0)	break;
		
		Term->WritePos --;
		// Singe Character
		if(Term->Text[ Term->WritePos ].Ch != '\0') {
			Term->Text[ Term->WritePos ].Ch = 0;
			Term->Text[ Term->WritePos ].Colour = Term->CurColour;
			break;
		}
		// Tab
		i = 7;	// Limit it to 8
		do {
			Term->Text[ Term->WritePos ].Ch = 0;
			Term->Text[ Term->WritePos ].Colour = Term->CurColour;
			Term->WritePos --;
		} while(Term->WritePos && i-- && Term->Text[ Term->WritePos ].Ch == '\0');
		if(Term->Text[ Term->WritePos ].Ch != '\0')
			Term->WritePos ++;
		break;
	
	default:
		Term->Text[ Term->WritePos ].Ch = Ch;
		Term->Text[ Term->WritePos ].Colour = Term->CurColour;
		Term->WritePos ++;
		break;
	}
		
	
	// Move Screen
	if(Term->WritePos >= Term->Width*Term->Height*VT_SCROLLBACK)
	{
		 int	base, i;
		Term->WritePos -= Term->Width;
		
		// Update view position
		base = Term->Width*Term->Height*(VT_SCROLLBACK-1);
		if(Term->ViewPos < base)	Term->ViewPos += Term->Width;
		if(Term->ViewPos > base)	Term->ViewPos = base;
		
		// Scroll terminal cache
		base = Term->Width*(Term->Height*VT_SCROLLBACK-1);
		
		// Scroll Back
		memcpy(
			Term->Text,
			&Term->Text[Term->Width],
			Term->Width*(Term->Height-1)*VT_SCROLLBACK*sizeof(tVT_Char)
			);
		
		// Clear last row
		for( i = 0; i < Term->Width; i ++ )
		{
			Term->Text[ base + i ].Ch = 0;
			Term->Text[ base + i ].Colour = Term->CurColour;
		}
		
		VT_int_UpdateScreen( Term, 1 );
	}
	else if(Term->WritePos > Term->Width*Term->Height+Term->ViewPos)
	{
		Term->ViewPos += Term->Width;
		VT_int_UpdateScreen( Term, 1 );
	}
	else
		VT_int_UpdateScreen( Term, 0 );
	
	//LEAVE('-');
}

/**
 * \fn void VT_int_UpdateScreen( tVTerm *Term, int UpdateAll )
 * \brief Updates the video framebuffer
 */
void VT_int_UpdateScreen( tVTerm *Term, int UpdateAll )
{
	if(UpdateAll) {
		VFS_WriteAt(
			giVT_OutputDevHandle,
			0,
			Term->Width*Term->Height*sizeof(tVT_Char),
			&Term->Text[Term->ViewPos]
			);
	} else {
		 int	pos = Term->WritePos - Term->WritePos % Term->Width;
		VFS_WriteAt(
			giVT_OutputDevHandle,
			(pos - Term->ViewPos)*sizeof(tVT_Char),
			Term->Width*sizeof(tVT_Char),
			&Term->Text[pos]
			);
	}
}

// ---
// Font Render
// ---
#define MONOSPACE_FONT	10816

#if MONOSPACE_FONT == 10808	// 8x8
# include "vterm_font_8x8.h"
#elif MONOSPACE_FONT == 10816	// 8x16
# include "vterm_font_8x16.h"
#endif

// === PROTOTYPES ===
Uint8	*VT_Font_GetChar(Uint32 Codepoint);

// === GLOBALS ===
int	giVT_CharWidth = FONT_WIDTH;
int	giVT_CharHeight = FONT_HEIGHT;

// === CODE ===
/**
 * \fn void VT_Font_Render(Uint32 Codepoint, void *Buffer, int Pitch, Uint32 BGC, Uint32 FGC)
 * \brief Render a font character
 */
void VT_Font_Render(Uint32 Codepoint, void *Buffer, int Pitch, Uint32 BGC, Uint32 FGC)
{
	Uint8	*font;
	Uint32	*buf = Buffer;
	 int	x, y;
	font = VT_Font_GetChar(Codepoint);
	
	for(y = 0; y < FONT_HEIGHT; y ++)
	{
		for(x = 0; x < FONT_WIDTH; x ++)
		{
			if(*font & (1 << (FONT_WIDTH-x)))
				buf[x] = FGC;
			else
				buf[x] = BGC;
		}
		buf += Pitch;
		font ++;
	}
}

/**
 * \fn Uint32 VT_Colour12to24(Uint16 Col12)
 * \brief Converts a 
 */
Uint32 VT_Colour12to24(Uint16 Col12)
{
	Uint32	ret;
	 int	tmp;
	tmp = Col12 & 0xF;
	ret  = (tmp << 0) | (tmp << 4);
	tmp = (Col12 & 0xF0) >> 4;
	ret |= (tmp << 8) | (tmp << 12);
	tmp = (Col12 & 0xF00) >> 8;
	ret |= (tmp << 16) | (tmp << 20);
	return ret;
}

/**
 * \fn Uint8 *VT_Font_GetChar(Uint32 Codepoint)
 * \brief Gets an index into the font array given a Unicode Codepoint
 * \note See http://en.wikipedia.org/wiki/CP437
 */
Uint8 *VT_Font_GetChar(Uint32 Codepoint)
{
	 int	index = 0;
	if(Codepoint < 128)
		return &VTermFont[Codepoint*FONT_HEIGHT];
	switch(Codepoint)
	{
	case 0xC7:	index = 128;	break;	// Ç
	case 0xFC:	index = 129;	break;	// ü
	case 0xE9:	index = 130;	break;	// é
	case 0xE2:	index = 131;	break;	// â
	case 0xE4:	index = 132;	break;	// ä
	case 0xE0:	index = 133;	break;	// à
	case 0xE5:	index = 134;	break;	// å
	case 0xE7:	index = 135;	break;	// ç
	case 0xEA:	index = 136;	break;	// ê
	case 0xEB:	index = 137;	break;	// ë
	case 0xE8:	index = 138;	break;	// è
	case 0xEF:	index = 139;	break;	// ï
	case 0xEE:	index = 140;	break;	// î
	case 0xEC:	index = 141;	break;	// ì
	case 0xC4:	index = 142;	break;	// Ä
	case 0xC5:	index = 143;	break;	// Å
	}
	
	return &VTermFont[index*FONT_HEIGHT];
}

EXPORTAS(&giVT_CharWidth, giVT_CharWidth);
EXPORTAS(&giVT_CharHeight, giVT_CharHeight);
EXPORT(VT_Font_Render);
EXPORT(VT_Colour12to24);
