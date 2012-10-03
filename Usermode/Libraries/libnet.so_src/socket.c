/*
 * Acess2 Networking Toolkit
 * By John Hodge (thePowersGang)
 * 
 * socket.c
 * - 
 */
#include <net.h>
#include <stdio.h>
#include <stdint.h>
#include <acess/sys.h>

int Net_OpenSocket(int AddrType, void *Addr, const char *Filename)
{
	 int	addrLen = Net_GetAddressSize(AddrType);
	 int	i;
	uint8_t	*addrBuffer = Addr;
	char	hexAddr[addrLen*2+1];
	
	for( i = 0; i < addrLen; i ++ )
		sprintf(hexAddr+i*2, "%02x", addrBuffer[i]);
	
	if(Filename)
	{
		 int	len = snprintf(NULL, 100, "/Devices/ip/routes/@%i:%s/%s", AddrType, hexAddr, Filename);
		char	path[len+1];
		snprintf(path, 100, "/Devices/ip/routes/@%i:%s/%s", AddrType, hexAddr, Filename);
		_SysDebug("%s", path);
		return open(path, OPENFLAG_READ|OPENFLAG_WRITE);
	}
	else
	{
		 int	len = snprintf(NULL, 100, "/Devices/ip/routes/@%i:%s", AddrType, hexAddr);
		char	path[len+1];
		snprintf(path, 100, "/Devices/ip/routes/@%i:%s", AddrType, hexAddr);
		return open(path, OPENFLAG_READ);
	}
}

int Net_OpenSocket_TCPC(int AddrType, void *Addr, int Port)
{
	int fd = Net_OpenSocket(AddrType, Addr, "tcpc");
	if( fd == -1 )	return -1;
	
	ioctl(fd, 5, &Port);	// Remote Port
        ioctl(fd, 6, Addr);	// Remote address
	ioctl(fd, 7, NULL);	// connect
	return fd;
}
