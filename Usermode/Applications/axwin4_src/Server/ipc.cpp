/*
 * Acess2 GUI v4
 * - By John Hodge (thePowersGang)
 *
 * ipc.cpp
 * - Client-Server communication (dispatch)
 */
#define __STDC_LIMIT_MACROS
#include <ipc.hpp>
#include <list>
#include <IIPCChannel.hpp>
#include <algorithm>
#include <CClient.hpp>
#include <serialisation.hpp>
#include <ipc_proto.hpp>
#include <CCompositor.hpp>
extern "C" {
#include <assert.h>
};
#include <CIPCChannel_AcessIPCPipe.hpp>
#include <draw_control.hpp>
#include <draw_text.hpp>

namespace AxWin {
namespace IPC {

CCompositor*	gpCompositor;
::std::list<IIPCChannel*>	glChannels;
::std::map<uint16_t,CClient*>	glClients;
uint16_t	giNextClient = 1;

void Initialise(const CConfigIPC& config, CCompositor& compositor)
{
	gpCompositor = &compositor;
	
	::std::string pipe_basepath = "axwin4";
	glChannels.push_back( new CIPCChannel_AcessIPCPipe( pipe_basepath ) );

	//glChannels.push_back( new CIPCChannel_TCP("0.0.0.0:2100") );
	
	//for( auto channel : config.m_channels )
	//{
	//	channels.push_back(  );
	//}
}

int FillSelect(fd_set& rfds)
{
	int ret = 0;
	for( const auto channel : glChannels )
	{
		assert(channel);
		ret = ::std::max(ret, channel->FillSelect(rfds));
	}
	return ret;
}

void HandleSelect(const fd_set& rfds)
{
	for( const auto channel : glChannels )
	{
		assert(channel);
		channel->HandleSelect(rfds);
	}
}

void RegisterClient(CClient& client)
{
	_SysDebug("RegisterClient(&client=%p)", &client);
	// allocate a client ID, and save
	for( int i = 0; i < 100; i ++ )
	{
		uint16_t id = giNextClient++;
		if(giNextClient == 0)	giNextClient = 1;
		auto r = glClients.insert( ::std::pair<uint16_t,CClient*>(id, &client) );
		if( r.second == true )
		{
			client.set_id(id);
			return;
		}
	}
	// Wut? 100 attempts and fail!
	assert(!"Todo - Better way of handling client ID reuse");
}

CClient* GetClientByID(uint16_t id)
{
	auto it = glClients.find(id);
	if(it == glClients.end()) {
		//_SysDebug("Client %i not registered", id);
		return nullptr;
	}
	else {
		//_SysDebug("Client %i %i = %p", id, it->first, it->second);
		return it->second;
	}
}

void DeregisterClient(CClient& client)
{
	glClients.erase( client.id() );
}


void SendMessage_NotifyDims(CClient& client, unsigned int WinID, unsigned int NewW, unsigned int NewH)
{
	_SysDebug("TODO: IPC::SendMessage_NotifyDims");
}
void SendMessage_MouseButton(CClient& client, unsigned int WinID, unsigned int X, unsigned int Y, uint8_t Button, bool Pressed)
{
	CSerialiser	msg;
	msg.WriteU8(IPCMSG_INPUTEVENT);
	msg.WriteU8(IPC_INEV_MOUSEBTN);
	msg.WriteU16(WinID);
	msg.WriteU16(X);
	msg.WriteU16(Y);
	msg.WriteU8(Button);
	msg.WriteU8(Pressed ? 0 : 1);
	client.SendMessage(msg);
}
void SendMessage_MouseMove(CClient& client, unsigned int WinID, unsigned int X, unsigned int Y)
{
	_SysDebug("TODO: IPC::SendMessage_MouseMove");
}
void SendMessage_KeyEvent(CClient& client, unsigned int WinID, uint32_t KeySym, bool Pressed, const char *Translated)
{
	CSerialiser	msg;
	msg.WriteU8(IPCMSG_INPUTEVENT);
	msg.WriteU8(IPC_INEV_KEYBOARD);
	msg.WriteU16(WinID);
	msg.WriteU16(KeySym);
	msg.WriteU8(Pressed ? 0 : 1);
	msg.WriteString(Translated);
	client.SendMessage(msg);
}


void HandleMessage_Nop(CClient& client, CDeserialiser& message)
{
	// Do nothing
}
void HandleMessage_Reply(CClient& client, CDeserialiser& message)
{
	// Reply to a sent message
	// - Not many messages need server-bound replies
	int orig_command = message.ReadU8();
	switch(orig_command)
	{
	case IPCMSG_PING:
		// Ping reply, mark client as still responding
		break;
	default:
		// Unexpected reply
		break;
	}
}

void HandleMessage_Ping(CClient& client, CDeserialiser& message)
{
	// A client has asked for a ping, we pong them back
	CSerialiser	reply;
	reply.WriteU8(IPCMSG_REPLY);
	reply.WriteU8(IPCMSG_PING);
	client.SendMessage(reply);
}

void HandleMessage_GetGlobalAttr(CClient& client, CDeserialiser& message)
{
	uint16_t	attr_id = message.ReadU16();
	
	CSerialiser	reply;
	reply.WriteU8(IPCMSG_REPLY);
	reply.WriteU8(IPCMSG_GETGLOBAL);
	reply.WriteU16(attr_id);
	
	switch(attr_id)
	{
	case IPC_GLOBATTR_SCREENDIMS: {
		uint8_t	screen_id = message.ReadU8();
		unsigned int w, h;
		gpCompositor->GetScreenDims(screen_id, &w, &h);
		reply.WriteU16( (w <= UINT16_MAX ? w : UINT16_MAX) );
		reply.WriteU16( (h <= UINT16_MAX ? h : UINT16_MAX) );
		break; }
	case IPC_GLOBATTR_MAXAREA:
		assert(!"TODO: IPC_GLOBATTR_MAXAREA");
		break;
	default:
		throw IPC::CClientFailure("Bad global attribute ID");
	}
	
	client.SendMessage(reply);
}

void HandleMessage_SetGlobalAttr(CClient& client, CDeserialiser& message)
{
	uint16_t	attr_id = message.ReadU16();
	
	switch(attr_id)
	{
	case IPC_GLOBATTR_SCREENDIMS:
		// Setting readonly
		break;
	case IPC_GLOBATTR_MAXAREA:
		assert(!"TODO: IPC_GLOBATTR_MAXAREA");
		break;
	default:
		throw IPC::CClientFailure("Bad global attribute ID");
	}
}

void HandleMessage_CreateWindow(CClient& client, CDeserialiser& message)
{
	uint16_t	new_id = message.ReadU16();
	//uint16_t	parent_id = message.ReadU16();
	//CWindow* parent = client.GetWindow( parent_id );
	::std::string	name = message.ReadString();
	
	::_SysDebug("_CreateWindow: (%i, '%s')", new_id, name.c_str());
	client.SetWindow( new_id, new CWindow(*gpCompositor, client, name, new_id) );
}

void HandleMessage_DestroyWindow(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	_SysDebug("_DestroyWindow: (%i)", win_id);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_DestroyWindow: Bad window");
	}
	client.SetWindow(win_id, 0);	
	
	// TODO: Directly inform compositor?
	delete win;
}

void HandleMessage_SetWindowAttr(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	attr_id = message.ReadU16();
	_SysDebug("_SetWindowAttr: (Win=%i, ID=%i)", win_id, attr_id);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_SetWindowAttr - Bad window");
	}
	
	switch(attr_id)
	{
	case IPC_WINATTR_DIMENSIONS: {
		uint16_t new_w = message.ReadU16();
		uint16_t new_h = message.ReadU16();
		win->Resize(new_w, new_h);
		break; }
	case IPC_WINATTR_POSITION: {
		int16_t new_x = message.ReadS16();
		int16_t new_y = message.ReadS16();
		win->Move(new_x, new_y);
		break; }
	case IPC_WINATTR_SHOW:
		win->Show( message.ReadU8() != 0 );
		break;
	case IPC_WINATTR_FLAGS:
		win->SetFlags( message.ReadU8() );	// TODO: U8? why so small?
		break;
	case IPC_WINATTR_TITLE:
		assert(!"TODO: IPC_WINATTR_TITLE");
		break;
	default:
		_SysDebug("HandleMessage_SetWindowAttr - Bad attr %u", attr_id);
		throw IPC::CClientFailure("Bad window attr");
	}
}

void HandleMessage_GetWindowAttr(CClient& client, CDeserialiser& message)
{
	assert(!"TODO HandleMessage_GetWindowAttr");
}

void HandleMessage_SendIPC(CClient& client, CDeserialiser& message)
{
	assert(!"TODO HandleMessage_SendIPC");
}

void HandleMessage_GetWindowBuffer(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	_SysDebug("_GetWindowBuffer: (%i)", win_id);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_PushData: Bad window");
	}
	
	uint64_t handle = win->m_surface.GetSHMHandle();
	
	CSerialiser	reply;
	reply.WriteU8(IPCMSG_REPLY);
	reply.WriteU8(IPCMSG_GETWINBUF);
	reply.WriteU16(win_id);
	reply.WriteU64(handle);
	client.SendMessage(reply);
}

void HandleMessage_DamageRect(CClient& client, CDeserialiser& message)
{
	uint16_t	winid = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	
	_SysDebug("_DamageRect: (%i %i,%i %ix%i)", winid, x, y, w, h);
	
	CWindow*	win = client.GetWindow(winid);
	if(!win) {
		throw IPC::CClientFailure("_PushData: Bad window");
	}
	
	CRect	area(x,y,w,h);
	
	win->Repaint(area);
}

void HandleMessage_PushData(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	_SysDebug("_PushData: (%i, (%i,%i) %ix%i)", win_id, x, y, w, h);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_PushData: Bad window");
	}
	
	for( unsigned int row = 0; row < h; row ++ )
	{
		const ::std::vector<uint8_t> scanline_data = message.ReadBuffer();
		if( scanline_data.size() != w * 4 ) {
			_SysDebug("ERROR _PushData: Scanline buffer size mismatch (%i,%i)",
				scanline_data.size(), w*4);
			continue ;
		}
		win->DrawScanline(y+row, x, w, scanline_data.data());
	}
}
void HandleMessage_Blit(CClient& client, CDeserialiser& message)
{
	assert(!"TODO HandleMessage_Blit");
}
void HandleMessage_DrawCtl(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	uint16_t	ctrl_id = message.ReadU16();
	uint16_t 	frame = message.ReadU16();
	_SysDebug("_DrawCtl: (%i, (%i,%i) %ix%i Ctl%i frame?=0x%04x)", win_id, x, y, w, h, ctrl_id, frame);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_DrawCtl: Bad window");
	}
	
	const CControl* ctrl = CControl::GetByID(ctrl_id);
	if(!ctrl) {
		throw IPC::CClientFailure("_DrawCtl: Invalid control ID");
	}
	
	CRect	area(x,y,w,h);
	ctrl->Render(win->m_surface, area);
}
void HandleMessage_DrawText(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	uint16_t	font_id = message.ReadU16();
	::std::string	str = message.ReadString();
	_SysDebug("_DrawText: (%i (%i,%i) %ix%i Font%i \"%s\")", win_id, x, y, w, h, font_id, str.c_str());
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_DrawText: Bad window");
	}
	
	// 1. Get font from client structure
	IFontFace& fontface = client.GetFont(font_id);
	
	// 2. Render
	CRect	area(x, y, w, h);
	fontface.Render(win->m_surface, area, str, h);
}

void HandleMessage_FillRect(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	uint32_t	colour = message.ReadU32();
	_SysDebug("_FillRect: (%i (%i,%i) %ix%i %06x)", win_id, x, y, w, h, colour);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_FillRect: Bad window");
	}
	
	while(h -- ) {
		win->FillScanline(y++, x, w, colour);
	}
}

void HandleMessage_DrawRect(CClient& client, CDeserialiser& message)
{
	uint16_t	win_id = message.ReadU16();
	uint16_t	x = message.ReadU16();
	uint16_t	y = message.ReadU16();
	uint16_t	w = message.ReadU16();
	uint16_t	h = message.ReadU16();
	uint32_t	colour = message.ReadU32();
	_SysDebug("_DrawRect: (%i (%i,%i) %ix%i %06x)", win_id, x, y, w, h, colour);
	
	CWindow*	win = client.GetWindow(win_id);
	if(!win) {
		throw IPC::CClientFailure("_DrawRect: Bad window");
	}
	
	if(h == 0) {
	}
	else if(h == 1) {
		win->FillScanline(y, x, w, colour);
	}
	else if(h == 2) {
		win->FillScanline(y++, x, w, colour);
		win->FillScanline(y++, x, w, colour);
	}
	else {
		win->FillScanline(y++, x, w, colour);
		while( h -- > 2 ) {
			win->FillScanline(y, x, 1, colour);
			win->FillScanline(y, x+w-1, 1, colour);
			y ++;
		}
		win->FillScanline(y++, x, w, colour);
	}
}

typedef void	MessageHandler_op_t(CClient& client, CDeserialiser& message);

MessageHandler_op_t	*message_handlers[] = {
	[IPCMSG_NULL]       = &HandleMessage_Nop,
	[IPCMSG_REPLY]      = &HandleMessage_Reply,
	[IPCMSG_PING]       = &HandleMessage_Ping,
	[IPCMSG_GETGLOBAL]  = &HandleMessage_GetGlobalAttr,
	[IPCMSG_SETGLOBAL]  = &HandleMessage_SetGlobalAttr,
	
	[IPCMSG_CREATEWIN]  = &HandleMessage_CreateWindow,
	[IPCMSG_CLOSEWIN]   = &HandleMessage_DestroyWindow,
	[IPCMSG_SETWINATTR] = &HandleMessage_SetWindowAttr,
	[IPCMSG_GETWINATTR] = &HandleMessage_GetWindowAttr,
	[IPCMSG_SENDIPC]    = &HandleMessage_SendIPC,	// Use the GUI server for low-bandwith IPC
	[IPCMSG_GETWINBUF]  = &HandleMessage_GetWindowBuffer,
	[IPCMSG_DAMAGERECT] = &HandleMessage_DamageRect,
	[IPCMSG_PUSHDATA]   = &HandleMessage_PushData,	// to a window's buffer
	[IPCMSG_BLIT]       = &HandleMessage_Blit,	// Copy data from one part of the window to another
	[IPCMSG_DRAWCTL]    = &HandleMessage_DrawCtl,	// Draw a control
	[IPCMSG_DRAWTEXT]   = &HandleMessage_DrawText,	// Draw text
	[IPCMSG_FILLRECT]   = &HandleMessage_FillRect,	// Fill a rectangle
	[IPCMSG_DRAWRECT]   = &HandleMessage_DrawRect,	// Draw (outline) a rectangle
};

void HandleMessage(CClient& client, CDeserialiser& message)
{
	const unsigned int num_commands = sizeof(message_handlers)/sizeof(IPC::MessageHandler_op_t*);
	unsigned int command = message.ReadU8();
	if( command >= num_commands ) {
		// Drop, invalid command
		_SysDebug("HandleMessage: Command %u is invalid (out of range for %u)", command, num_commands);
		return ;
	}
	
	(message_handlers[command])(client, message);
}

CClientFailure::CClientFailure(std::string&& what):
	m_what(what)
{
}
const char *CClientFailure::what() const throw()
{
	return m_what.c_str();
}
CClientFailure::~CClientFailure() throw()
{
}

};	// namespace IPC

IIPCChannel::~IIPCChannel()
{
}

};	// namespace AxWin

