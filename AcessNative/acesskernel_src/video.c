/*
 * Acess2 Native Kernel
 * 
 * Video Driver
 */
#define VERSION	((0<<8)|10)
#define DEBUG	0
#include <acess.h>
#include <vfs.h>
#include <fs_devfs.h>
#include <modules.h>
#include <tpl_drv_video.h>
#include "ui.h"

// === PROTOTYPES ===
 int	Video_Install(char **Arguments);
Uint64	Video_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);
Uint64	Video_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer);
 int	Video_IOCtl(tVFS_Node *Node, int ID, void *Data);
// --- 2D Acceleration Functions --
void	Video_2D_Fill(void *Ent, Uint16 X, Uint16 Y, Uint16 W, Uint16 H, Uint32 Colour);
void	Video_2D_Blit(void *Ent, Uint16 DstX, Uint16 DstY, Uint16 SrcX, Uint16 SrcY, Uint16 W, Uint16 H);

// === GLOBALS ===
//MODULE_DEFINE(0, VERSION, NativeVideo, Video_Install, NULL, NULL);
tDevFS_Driver	gVideo_DriverStruct = {
	NULL, "NativeVideo",
	{
	.Read = Video_Read,
	.Write = Video_Write,
	.IOCtl = Video_IOCtl
	}
};
 int	giVideo_DriverID;
 int	giVideo_CurrentFormat;
// --- 2D Video Stream Handlers ---
tDrvUtil_Video_2DHandlers	gVideo_2DFunctions = {
	NULL,
	Video_2D_Fill,
	Video_2D_Blit
};

// === CODE ===
int Video_Install(char **Arguments)
{
	// Install Device
	giVideo_DriverID = DevFS_AddDevice( &gVideo_DriverStruct );
	if(giVideo_DriverID == -1)
		return MODULE_ERR_MISC;
	
	return MODULE_ERR_OK;
}

/**
 * \brief Read from framebuffer (unimplemented)
 */
Uint64 Video_Read(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	return 0;
}

/**
 * \brief Write to the framebuffer
 */
Uint64 Video_Write(tVFS_Node *Node, Uint64 Offset, Uint64 Length, void *Buffer)
{
	 int	i;
	ENTER("pNode XOffset XLength pBuffer", Node, Offset, Length, Buffer);

	if(Buffer == NULL) {
		LEAVE('i', 0);
		return 0;
	}
	// Text Mode
	switch( giVideo_CurrentFormat )
	{
	case VIDEO_BUFFMT_TEXT:
		{
		tVT_Char	*chars = Buffer;
		// int	pitch = giUI_Pitch;
		 int	widthInChars = giUI_Width/giVT_CharWidth;
		 int	heightInChars = giUI_Height/giVT_CharHeight;
		 int	x, y;
		Uint32	tmpBuf[giVT_CharHeight*giVT_CharWidth];
		
		Length /= sizeof(tVT_Char);
		Offset /= sizeof(tVT_Char);
		
		x = Offset % widthInChars;
		y = Offset / widthInChars;
		
		// Sanity Check
		if( Offset > (Uint64)(heightInChars*widthInChars) ) {
			Log_Notice("Video", "Offset (%i) > %i*%i (%i)", Offset,
				heightInChars, widthInChars, heightInChars*widthInChars);
			LEAVE('i', 0);
			return 0;
		}
		if(y >= heightInChars) {
			LEAVE('i', 0);
			return 0;
		}
		
		// Clip to screen size
		if( (int)Offset + (int)Length > heightInChars*widthInChars ) {
			Log_Debug("Video", "%i + %i > %i*%i (%i)",
				(int)Offset, (int)Length, heightInChars, widthInChars, heightInChars*widthInChars);
			Length = heightInChars*widthInChars - Offset;
			Log_Notice("Video", "Clipping write size to %i characters", (int)Length);
		}
		
//		Log_Debug("Video", "(%i,%i) %i chars", x, y, (int)Length);
		
		// Print characters
		for( i = 0; i < (int)Length; i++ )
		{
			if( chars->Ch )
			{
//				Log_Debug("Video", "Render Char 0x%x in 0x%03x:%03x",
//					chars->Ch, chars->FGCol, chars->BGCol);
				memset(tmpBuf, 0xFF, giVT_CharWidth*giVT_CharHeight*4);
				VT_Font_Render(
					chars->Ch,
					tmpBuf, 32, giVT_CharWidth*4,
					VT_Colour12to24(chars->BGCol),
					VT_Colour12to24(chars->FGCol)
					);
				UI_BlitBitmap(
					x*giVT_CharWidth, y*giVT_CharHeight,
					giVT_CharWidth, giVT_CharHeight,
					tmpBuf
					);
			}
			else
			{
				UI_FillBitmap(
					x*giVT_CharWidth, y*giVT_CharHeight,
					giVT_CharWidth, giVT_CharHeight,
					VT_Colour12to24(chars->BGCol)
					);
			}
			
			chars ++;
			x ++;
			if( x >= widthInChars ) {
				x = 0;
				y ++;
				//dest += pitch*giVT_CharHeight;
			}
		}
		Length *= sizeof(tVT_Char);
		}
		break;
	
	case VIDEO_BUFFMT_FRAMEBUFFER:
		{
		 int	startX, startY;
		
		if(giUI_Pitch*giUI_Height < Offset+Length)
		{
			Log_Warning("Video", "Video_Write - Framebuffer Overflow");
			LEAVE('i', 0);
			return 0;
		}
		
		LOG("buffer = %p", Buffer);
		
		startX = Offset % giUI_Width;
		startY = Offset / giUI_Width;
		
		if( Length + startX < giUI_Width )
		{
			// Single line
			UI_BlitBitmap(
				startX, startY,
				Length, 1,
				Buffer);
		}
		else
		{
			// First scanline (partial or full)
			UI_BlitBitmap(
				startX, startY,
				giUI_Width - startX, 1,
				Buffer);
			
			Length -= giUI_Width - startX;
			Buffer += giUI_Width - startX;
			
			// Middle Scanlines
			for( i = 0; i < Length / giUI_Height; i ++ )
			{
				UI_BlitBitmap(
					0, startY + i,
					giUI_Width, 1,
					Buffer);
				Buffer += giUI_Width;
			}
			
			// Final scanline (partial)
			if( Length % giUI_Height )
			{
				UI_BlitBitmap(
					0, startY + i,
					Length % giUI_Height, 1,
					Buffer);
			}
		}
		}
		break;
	
	case VIDEO_BUFFMT_2DSTREAM:
		Length = DrvUtil_Video_2DStream(
			NULL,	// Single framebuffer, so Ent is unused
			Buffer, Length, &gVideo_2DFunctions, sizeof(gVideo_2DFunctions)
			);
		break;
	
	default:
		LEAVE('i', -1);
		return -1;
	}
	
	// Tell the UI to blit
	UI_Redraw();
	
	LEAVE('X', Length);
	return Length;
}

const char * csaVIDEO_IOCTLS[] = {DRV_IOCTLNAMES, DRV_VIDEO_IOCTLNAMES, NULL};
/**
 * \brief Handle messages to the device
 */
int Video_IOCtl(tVFS_Node *Node, int ID, void *Data)
{
	 int	ret;
	tVideo_IOCtl_Mode	*mode = Data;
	switch(ID)
	{
	BASE_IOCTLS(DRV_TYPE_VIDEO, "NativeVideo", VERSION, csaVIDEO_IOCTLS);
	
	// Video mode control
	// - We cheat, and only have one mode
	case VIDEO_IOCTL_GETSETMODE:
		return 0;
	case VIDEO_IOCTL_FINDMODE:
	case VIDEO_IOCTL_MODEINFO:
		mode->id = 0;
		mode->width = giUI_Width;
		mode->height = giUI_Height;
		mode->bpp = 32;
		mode->flags = 0;
		return 0;
	
	// Buffer mode
	case VIDEO_IOCTL_SETBUFFORMAT:
		ret = giVideo_CurrentFormat;
		if(Data) {
			giVideo_CurrentFormat = *(int*)Data;
		}
		return ret;
	
	#if 0
	case VIDEO_IOCTL_SETCURSOR:	// Set cursor position
		#if !BLINKING_CURSOR
		if(giVesaCursorX > 0)
			Vesa_FlipCursor(Node);
		#endif
		giVesaCursorX = ((tVideo_IOCtl_Pos*)Data)->x;
		giVesaCursorY = ((tVideo_IOCtl_Pos*)Data)->y;
		//Log_Debug("VESA", "Cursor position (%i,%i)", giVesaCursorX, giVesaCursorY);
		if(
			giVesaCursorX < 0 || giVesaCursorY < 0
		||	giVesaCursorX >= gpVesaCurMode->width/giVT_CharWidth
		||	giVesaCursorY >= gpVesaCurMode->height/giVT_CharHeight)
		{
			#if BLINKING_CURSOR
			if(giVesaCursorTimer != -1) {
				Time_RemoveTimer(giVesaCursorTimer);
				giVesaCursorTimer = -1;
			}
			#endif
			giVesaCursorX = -1;
			giVesaCursorY = -1;
		}
		else {
			#if BLINKING_CURSOR
		//	Log_Debug("VESA", "Updating timer %i?", giVesaCursorTimer);
			if(giVesaCursorTimer == -1)
				giVesaCursorTimer = Time_CreateTimer(VESA_CURSOR_PERIOD, Vesa_FlipCursor, Node);
			#else
			Vesa_FlipCursor(Node);
			#endif
		}
		//Log_Debug("VESA", "Cursor position (%i,%i) Timer %i", giVesaCursorX, giVesaCursorY, giVesaCursorTimer);
		return 0;
	#endif
	
	case VIDEO_IOCTL_REQLFB:	// Request Linear Framebuffer
		return 0;
	}
	return 0;
}

// --- 2D Acceleration Functions --
void Video_2D_Fill(void *Ent, Uint16 X, Uint16 Y, Uint16 W, Uint16 H, Uint32 Colour)
{
	UI_FillBitmap(X, Y, W, H, Colour);
}

void Video_2D_Blit(void *Ent, Uint16 DstX, Uint16 DstY, Uint16 SrcX, Uint16 SrcY, Uint16 W, Uint16 H)
{
	UI_BlitFramebuffer(DstX, DstY, SrcX, SrcY, W, H);
}
