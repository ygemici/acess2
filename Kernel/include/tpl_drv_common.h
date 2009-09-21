/**
 AcessOS Version 1
 \file tpl_drv_common.h
 \brief Common Driver Interface Definitions
*/
#ifndef _TPL_COMMON_H
#define _TPL_COMMON_H

/**
 * \enum eTplDrv_IOCtl
 * \brief Common IOCtl Calls
 */
enum eTplDrv_IOCtl {
	/// \brief Driver Type - Return an ::eTplDrv_Type value
	DRV_IOCTL_TYPE,
	/// \brief Get driver identifier - (char *dest[4])
	DRV_IOCTL_IDENT,
	/// \brief Get driver version - (int *ver)
	DRV_IOCTL_VERSION,
	/// \brief Get a IOCtl from a symbolic name
	DRV_IOCTL_LOOKUP,
};

/**
 * \enum eTplDrv_Type
 * \brief Driver Types returned by DRV_IOCTL_TYPE
 */
enum eTplDrv_Type {
	DRV_TYPE_NULL,		//!< NULL Type - Custom Interface
	DRV_TYPE_MISC,		//!< Miscelanious Compilant - Supports the core calls
	DRV_TYPE_TERMINAL,	//!< Terminal
	DRV_TYPE_VIDEO,		//!< Video - LFB
	DRV_TYPE_SOUND,		//!< Audio
	DRV_TYPE_DISK,		//!< Disk
	DRV_TYPE_KEYBOARD,	//!< Keyboard
	DRV_TYPE_MOUSE,		//!< Mouse
	DRV_TYPE_JOYSTICK,	//!< Joystick / Gamepad
	DRV_TYPE_NETWORK	//!< Network Device
};

// === FUNCTIONS ===
extern int	GetIOCtlId(int Class, char *Name);

#endif
