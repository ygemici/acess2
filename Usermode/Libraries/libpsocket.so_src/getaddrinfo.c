/*
 * Acess2 POSIX Sockets Library
 * - By John Hodge (thePowersGang)
 *
 * getaddrinfo.c
 * - getaddrinfo/freeaddrinfo/getnameinfo/gai_strerror
 */
#include <netdb.h>
#include <netinet/in.h>
#include <net.h>	// Net_ParseAddress
#include <stdlib.h>	// malloc
#include <string.h>	// memcpy
#include <acess/sys.h>

// === CODE ===
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
	static const struct addrinfo	defhints = {.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG};
	struct addrinfo	*ret = NULL;

	// Error checks
	if( !node && !service )	return EAI_NONAME;
	
	if( !hints )
		hints = &defhints;

	if( !node )
	{
		if( !(hints->ai_flags & AI_PASSIVE) )
			;	// Use localhost
		else
			;	// Use wildcard
	}
	else
	{
		// 1. Check if the node is an IP address
		// TODO: Break this function out into inet_pton?
		{
			 int	type;
			char	addrdata[16];
			type = Net_ParseAddress(node, addrdata);
			switch(type)
			{
			case 0:
				break;
			case 4:	// IPv4
				ret = malloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
				ret->ai_family = AF_INET;
				ret->ai_socktype = 0;
				ret->ai_protocol = 0;
				ret->ai_addrlen = sizeof(struct in_addr);
				ret->ai_addr = (void*)( ret + 1 );
				ret->ai_canonname = 0;
				ret->ai_next = 0;
				((struct sockaddr_in*)ret->ai_addr)->sin_family = AF_INET;
				((struct sockaddr_in*)ret->ai_addr)->sin_port = 0;
				memcpy( &((struct sockaddr_in*)ret->ai_addr)->sin_addr, addrdata, 4 );
				break;
			default:
				_SysDebug("getaddrinfo: Unknown address family %i", type);
				return 1;
			}
		}
		
		// 2. Check for a DNS name
		// - No luck with above, and hints->ai_flags doesn't have AI_NUMERICHOST set
		if( !ret && !(hints->ai_flags & AI_NUMERICHOST) )
		{
			// TODO: DNS Lookups
			// ? /Acess/Conf/Nameservers
			// ? /Acess/Conf/Hosts
		}
		
		// 3. No Match, chuck sad
		if( !ret )
		{
			return EAI_NONAME;
		}
	}

	int default_socktype = 0;
	int default_protocol = 0;
	int default_port = 0;
	
	// Convert `node` into types
	if( service )
	{
		// TODO: Read something like /Acess/Conf/services
	}

	struct addrinfo	*ai;
	for( ai = ret; ai; ai = ai->ai_next)
	{
		struct sockaddr_in	*in = (void*)ai->ai_addr;
		struct sockaddr_in6	*in6 = (void*)ai->ai_addr;
		
		// Check ai_socktype/ai_protocol
		// TODO: Do both of these need to be zero for defaults to apply?
		if( ai->ai_socktype == 0 )
			ai->ai_socktype = default_socktype;
		if( ai->ai_protocol == 0 )
			ai->ai_protocol = default_protocol;
		
		switch(ai->ai_family)
		{
		case AF_INET:
			if( in->sin_port == 0 )
				in->sin_port = default_port;
			break;
		case AF_INET6:
			if( in6->sin6_port == 0 )
				in6->sin6_port = default_port;
			break;
		default:
			_SysDebug("getaddrinfo: Unknown address family %i (setting port)", ai->ai_family);
			return 1;
		}
	}

	*res = ret;
	return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
	
}

const char *gai_strerror(int errnum)
{
	switch(errnum)
	{
	case EAI_SUCCESS: 	return "No error";
	case EAI_FAIL:  	return "Permanent resolution failure";
	default:	return "UNKNOWN";
	}
}
