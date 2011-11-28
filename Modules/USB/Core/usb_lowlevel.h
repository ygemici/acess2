/**
 * Acess2 USB Stack
 * - By John Hodge (thePowersGang)
 * 
 * usb_lowlevel.h
 * - Low-Level USB IO Functions
 */
#ifndef _USB_LOWLEVEL_H_
#define _USB_LOWLEVEL_H_

extern int	USB_int_SendSetupSetAddress(tUSBHost *Host, int Address);
extern int	USB_int_ReadDescriptor(tUSBDevice *Dev, int Endpoint, int Type, int Index, int Length, void *Dest);
extern char	*USB_int_GetDeviceString(tUSBDevice *Dev, int Endpoint, int Index);

#endif
