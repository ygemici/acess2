/*
 * Acess 2 USB Stack
 * Universal Host Controller Interface
 */
#define DEBUG	1
#include <common.h>
#include <vfs.h>
#include <drv_pci.h>
#include "usb.h"
#include "uhci.h"

// === CONSTANTS ===
#define	MAX_CONTROLLERS	4
#define NUM_TDs	1024

// === PROTOTYPES ===
 int	UHCI_Initialise();
void	UHCI_Cleanup();
 int	UHCI_IOCtl(tVFS_Node *node, int id, void *data);
 int	UHCI_Int_InitHost(tUHCI_Controller *Host);

// === GLOBALS ===
//Uint	gaFrameList[1024];
tUHCI_TD	gaUHCI_TDPool[NUM_TDs];
tUHCI_Controller	gUHCI_Controllers[MAX_CONTROLLERS];

// === CODE ===
/**
 * \fn int UHCI_Initialise()
 * \brief Called to initialise the UHCI Driver
 */
int UHCI_Initialise()
{
	 int	i=0, id=-1;
	 int	ret;
	Uint16	base;
	
	ENTER("");
	
	// Enumerate PCI Bus, getting a maximum of `MAX_CONTROLLERS` devices
	while( (id = PCI_GetDeviceByClass(0x0C03, 0xFFFF, id)) >= 0 && i < MAX_CONTROLLERS )
	{
		gUHCI_Controllers[i].PciId = id;
		// Assign a port range (BAR4, Reserve 32 ports)
		base = PCI_AssignPort( id, 4, 0x20 );
		gUHCI_Controllers[i].IOBase = base;
		
		LOG("Controller PCI #%i: IO Base = 0x%x", id, base);
		
		// Initialise Host
		ret = UHCI_Int_InitHost(&gUHCI_Controllers[i]);
		// Detect an error
		if(ret != 0) {
			LEAVE('i', ret);
			return ret;
		}
		
		i ++;
	}
	if(i == MAX_CONTROLLERS) {
		Warning("[UHCI ] Over "EXPAND_STR(MAX_CONTROLLERS)" UHCI controllers detected, ignoring rest");
	}
	LEAVE('i', i);
	return i;
}

/**
 * \fn void UHCI_Cleanup()
 * \brief Called just before module is unloaded
 */
void UHCI_Cleanup()
{
}

/**
 * \brief Sends a packet to a device endpoint
 */
int UHCI_SendPacket(int ControllerId, int Device, int Endpoint, void *Data, int Length)
{
	return 0;
}

// === INTERNAL FUNCTIONS ===
/**
 * \fn int UHCI_Int_InitHost(tUCHI_Controller *Host)
 * \brief Initialises a UHCI host controller
 * \param Host	Pointer - Host to initialise
 */
int UHCI_Int_InitHost(tUHCI_Controller *Host)
{
	ENTER("pHost", Host);
	
	// Allocate Frame List
	Host->FrameList = (void *) MM_AllocDMA(1, 32, &Host->PhysFrameList);	// 1 Page, 32-bit
	if( !Host->FrameList ) {
		Warning("[UHCI ] Unable to allocate frame list, aborting");
		LEAVE('i', -1);
		return -1;
	}
	LOG("Allocated frame list 0x%x (0x%x)", Host->FrameList, Host->PhysFrameList);
	memsetd( Host->FrameList, 1, 1024 );	// Clear List (Disabling all entries)
	
	//! \todo Properly fill frame list
	
	// Set frame length to 1 ms
	outb( Host->IOBase + SOFMOD, 64 );
	
	// Set Frame List Address
	outd( Host->IOBase + FLBASEADD, Host->PhysFrameList );
	
	// Set Frame Number
	outw( Host->IOBase + FRNUM, 0 );
	
	// Enable Interrupts
	//PCI_WriteWord( Host->PciId, 0xC0, 0x2000 );
	
	LEAVE('i', 0);
	return 0;
}
