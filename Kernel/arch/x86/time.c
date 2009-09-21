/*
 * Acess2 Kernel
 * Timekeeping
 * arch/x86/time.c
 */
#include <common.h>

// === MACROS ===
#define TIMER_FREQ	1024	//Hz
#define MS_PER_TICK_WHOLE	(1000/(TIMER_FREQ))
#define MS_PER_TICK_FRACT	((Uint64)(1000*TIMER_FREQ-((Uint64)MS_PER_TICK_WHOLE)*0x80000000/TIMER_FREQ))

// === PROTOTYPES ===
void	Time_Interrupt();

// === GLOBALS ===
Uint64	giTicks = 0;
Sint64	giTimestamp = 0;
Uint64	giPartMiliseconds = 0;

// === CODE ===
/**
 * \fn int Time_Setup()
 * \brief Sets the system time from the Realtime-Clock
 */
int Time_Setup()
{
	Uint8	val;
	
	outb(0x70, inb(0x70)&0x7F);	// Disable NMIs
	__asm__ __volatile__ ("cli");	// Disable normal interrupts
	
	// Enable IRQ8
	outb(0x70, 0x0B);	// Set the index to register B
	val = inb(0x71);	// Read the current value of register B
	outb(0x70, 0x0B);	// Set the index again (a read will reset the index to register D)
	outb(0x71, val | 0x40);	// Write the previous value or'd with 0x40. This turns on bit 6 of register D
	
	__asm__ __volatile__ ("sti");	// Disable normal interrupts
	outb(0x70, inb(0x70)|0x80);	// Disable NMIs
	
	// Install IRQ Handler
	IRQ_AddHandler(8, Time_Interrupt);
	return 0;
}

/**
 * \fn void Time_Interrupt()
 * \brief Called on the timekeeping IRQ
 */
void Time_Interrupt()
{
	giTicks ++;
	giTimestamp += MS_PER_TICK_WHOLE;
	giPartMiliseconds += MS_PER_TICK_FRACT;
	if(giPartMiliseconds > 0x80000000) {
		giTimestamp ++;
		giPartMiliseconds -= 0x80000000;
	}
}

/**
 * \fn Sint64 now()
 * \brief Return the current timestamp
 */
Sint64 now()
{
	return giTimestamp;
}
