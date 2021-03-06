/*
 * Acess2 IP Stack
 * - IPv6 Protcol Handling
 */
#include "ipstack.h"
#include "link.h"
#include "ipv6.h"
#include "firewall.h"
#include "hwaddr_cache.h"

// === IMPORTS ===
extern tInterface	*gIP_Interfaces;
extern Uint32	IPv4_Netmask(int FixedBits);

// === PROTOTYPES ===
 int	IPv6_Initialise();
// int	IPv6_RegisterCallback(int ID, tIPCallback Callback);
void	IPv6_int_GetPacket(tAdapter *Interface, tMacAddr From, int Length, void *Buffer);
tInterface	*IPv6_GetInterface(tAdapter *Adapter, tIPv6 Address, int Broadcast);

// === GLOBALS ===
tIPRxCallback*	gaIPv6_Callbacks[256];

// === CODE ===
/**
 * \brief Initialise the IPv6 handling code
 */
int IPv6_Initialise()
{
	Link_RegisterType(IPV6_ETHERNET_ID, IPv6_int_GetPacket);
	return 1;
}

/**
 * \brief Registers a callback
 * \param ID	8-bit packet type ID
 * \param Callback	Callback function
 */
int IPv6_RegisterCallback(int ID, tIPRxCallback* Callback)
{
	if( ID < 0 || ID > 255 )	return 0;
	if( gaIPv6_Callbacks[ID] )	return 0;
	gaIPv6_Callbacks[ID] = Callback;
	return 1;
}

/**
 * \brief Creates and sends an IPv6 Packet
 * \param Iface	Interface
 * \param Destination	Destination IP
 * \param Protocol	Protocol ID
 * \param Length	Data Length
 * \param Data	Packet Data
 * \return Boolean Success
 */
int IPv6_SendPacket(tInterface *Iface, tIPv6 Destination, int Protocol, tIPStackBuffer *Buffer)
{
	size_t length = IPStack_Buffer_GetLength(Buffer);
	
	// Resolve destination
	tMacAddr to = HWCache_Resolve(Iface, &Destination);
	if( MAC_EQU(to, cMAC_ZERO) ) {
		// No route to host
		return 0;
	}
	
	// Build up header
	tIPv6Header	hdr;
	hdr.Version = 6;
	hdr.TrafficClass = 0;
	hdr.FlowLabel = 0;
	hdr.Head = htonl(hdr.Head);
	hdr.PayloadLength = htons(length);
	hdr.NextHeader = Protocol;	// TODO: Routing header?
	hdr.HopLimit = 64;	// TODO: Configurable TTL
	hdr.Source = *(tIPv6*)Iface->Address;
	hdr.Destination = Destination;
	
	IPStack_Buffer_AppendSubBuffer(Buffer, sizeof(hdr), 0, &hdr, NULL, NULL);
	
	Link_SendPacket(Iface->Adapter, IPV6_ETHERNET_ID, to, Buffer);

	return 1;
}

/**
 * \fn void IPv6_int_GetPacket(tInterface *Interface, tMacAddr From, int Length, void *Buffer)
 * \brief Process an IPv6 Packet
 * \param Interface	Input interface
 * \param From	Source MAC address
 * \param Length	Packet length
 * \param Buffer	Packet data
 */
void IPv6_int_GetPacket(tAdapter *Adapter, tMacAddr From, int Length, void *Buffer)
{
	tInterface	*iface;
	tIPv6Header	*hdr = Buffer;
	 int	ret;
	char	*dataPtr;
	Uint8	nextHeader;
	
	if(Length < sizeof(tIPv6Header))	return;
	
	hdr->Head = ntohl(hdr->Head);
	
	//if( ((hdr->Head >> (20+8)) & 0xF) != 6 )
	if( hdr->Version != 6 )
		return;
	
	#if 1
	Log_Debug("IPv6", "hdr = {");
	Log_Debug("IPv6", " .Version       = %i", hdr->Version );
	Log_Debug("IPv6", " .TrafficClass  = %i", hdr->TrafficClass );
	Log_Debug("IPv6", " .FlowLabel     = %i", hdr->FlowLabel );
	Log_Debug("IPv6", " .PayloadLength = 0x%04x", ntohs(hdr->PayloadLength) );
	Log_Debug("IPv6", " .NextHeader    = 0x%02x", hdr->NextHeader );
	Log_Debug("IPv6", " .HopLimit      = 0x%02x", hdr->HopLimit );
	Log_Debug("IPv6", " .Source        = %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", hdr->Source );
	Log_Debug("IPv6", " .Destination   = %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", hdr->Destination );
	Log_Debug("IPv6", "}");
	#endif
	
	// No checksum in IPv6
	
	// Check Packet length
	if( ntohs(hdr->PayloadLength)+sizeof(tIPv6Header) > Length) {
		Log_Log("IPv6", "hdr->PayloadLength(%i) > Length(%i)", ntohs(hdr->PayloadLength), Length);
		return;
	}
	
	// Process Options
	nextHeader = hdr->NextHeader;
	dataPtr = hdr->Data;
	for( ;; )
	{
		struct {
			Uint8	NextHeader;
			Uint8	Length;	// In 8-byte chunks, with 0 being 8 bytes long
			Uint8	Data[];
		}	*optionHdr;
		optionHdr = (void*)dataPtr;
		// Hop-by-hop options
		if(nextHeader == 0)
		{
			// TODO: Parse the options (actually, RFC2460 doesn't specify any)
			// Two Defined: Pad1 and PadN
		}
		// Routing Options
		else if(nextHeader == 43)
		{
			// TODO: Routing Header options
		}
		else
		{
			break;	// Unknown, pass to next layer
		}
		nextHeader = optionHdr->NextHeader;
		dataPtr += (optionHdr->Length + 1) * 8;	// 8-octet length (0 = 8 bytes long)
	}
	
	// Get Interface (allowing broadcasts)
	iface = IPv6_GetInterface(Adapter, hdr->Destination, 1);
	
	// Firewall rules
	if( iface ) {
		// Incoming Packets
		ret = IPTables_TestChain("INPUT",
			6, &hdr->Source, &hdr->Destination,
			hdr->NextHeader, 0,
			hdr->PayloadLength, hdr->Data
			);
	}
	else {
		// Routed packets
		
		// If routing is disabled globally, or if it's disabled for v6 only, drop
		if( false || false )
		{
			return ;
		}
		// TODO: Defer routing of packet
		LOG("Route the packet");
		// Drop the packet if the TTL is zero
		if( hdr->HopLimit == 0 ) {
			Log_Warning("IPv6", "TODO: Send ICMP-Timeout when TTL exceeded");
			return ;
		}
		hdr->HopLimit --;
		
		ret = IPTables_TestChain("FORWARD",
			6, &hdr->Source, &hdr->Destination,
			hdr->NextHeader, 0,
			hdr->PayloadLength, hdr->Data
			);
	}
	
	switch(ret)
	{
	// 0 - Allow
	case 0:	break;
	// 1 - Silent Drop
	case 1:
		Log_Debug("IPv6", "Silently dropping packet");
		return ;
	// Unknown, silent drop
	default:
		return ;
	}
	
	// Routing
	if(!iface)
	{
		// TODO: Use tIPStackBuffer instead, for refcounting
		//IPStack_RoutePacket(6, &hdr->Destination, Length, Buffer);
		return ;
	}
	
	// Populate cache
	// - TODO: Populate when routing using source address match for iface
	if( IPStack_CompareAddress(6, &hdr->Source, iface->Address, iface->SubnetBits) )
	{
		HWCache_Set(Adapter, 6, &hdr->Source, &From);
	}
	
	// Send it on
	if( !gaIPv6_Callbacks[hdr->NextHeader] ) {
		Log_Log("IPv6", "Unknown Protocol %i", hdr->NextHeader);
		return ;
	}
	
	gaIPv6_Callbacks[hdr->NextHeader]( iface, &hdr->Source, hdr->PayloadLength, hdr->Data );
}

/**
 * \fn tInterface *IPv6_GetInterface(tAdapter *Adapter, tIPv6 Address)
 * \brief Searches an adapter for a matching address
 * \param Adapter	Source adapter
 * \param Address	Destination Address
 * \param Broadcast	Allow broadcast?
 */
tInterface *IPv6_GetInterface(tAdapter *Adapter, tIPv6 Address, int Broadcast)
{
	 int	i, j;
	tInterface	*iface = NULL;
	Uint32	netmask;
	
	for( iface = gIP_Interfaces; iface; iface = iface->Next)
	{
		tIPv6	*thisAddr;
		
		// Check for this adapter
		if( iface->Adapter != Adapter )	continue;
		
		// Skip non-IPv6 Interfaces
		if( iface->Type != 6 )	continue;
		
		thisAddr = (tIPv6*)iface->Address;
		// If the address is a perfect match, return this interface
		if( IP6_EQU(Address, *thisAddr) )	return iface;
		
		// Check if we want to match broadcast addresses
		if( !Broadcast )	continue;
		
		// Check for broadcast
		// - Check first DWORDs
		if( iface->SubnetBits > 32 && Address.L[0] != thisAddr->L[0] )
			continue;
		if( iface->SubnetBits > 64 && Address.L[1] != thisAddr->L[1] )
			continue;
		if( iface->SubnetBits > 96 && Address.L[2] != thisAddr->L[2] )
			continue;
		
		// Check final DWORD
		j = iface->SubnetBits / 32;
		i = iface->SubnetBits % 32;
		netmask = IPv4_Netmask( iface->SubnetBits % 32 );
		
		// Check the last bit of the netmask
		if( (Address.L[j] >> i) != (thisAddr->L[j] >> i) )	continue;
		
		// Check that the host portion is one
		if( (Address.L[j] & ~netmask) != (0xFFFFFFFF & ~netmask) )	continue;
		if( j >= 2 && Address.L[3] != 0xFFFFFFFF)	continue;
		if( j >= 1 && Address.L[2] != 0xFFFFFFFF)	continue;
		if( j >= 0 && Address.L[1] != 0xFFFFFFFF)	continue;
		
		return iface;
	}
	return NULL;
}
