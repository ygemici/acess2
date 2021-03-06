/*
 * Acess2 Kernel
 * Timekeeping
 * arch/x86/time.c
 */
#include <acess.h>
#include <timers.h>

// === MACROS ===
#define	TIMER_QUANTUM	100
// 2^(15-rate), 14: 2Hz, 5: 1024Hz, 2: 8192Hz
// (Max: 14, Min: 2) - 14 = 2Hz, 13 = 4Hz, 12 = 8Hz, 11 = 16Hz 10 = 32Hz, 2 = 8192Hz
//#define TIMER_RATE	10	// 32 Hz
//#define TIMER_RATE	12	// 8 Hz
#define TIMER_RATE	14	// 2 Hz - Lowest
#define TIMER_FREQ	(0x8000>>TIMER_RATE)	//Hz
#define MS_PER_TICK_WHOLE	(1000/(TIMER_FREQ))
#define MS_PER_TICK_FRACT	((0x80000000*(1000%TIMER_FREQ))/TIMER_FREQ)
#define US_PER_TICK	(1000*1000/(TIMER_FREQ))

// === IMPORTS ===
extern volatile Sint64	giTimestamp;
extern volatile Uint64	giTicks;
extern volatile Uint64	giPartMiliseconds;
extern void	Timer_CallTimers(void);

// === GLOBALS ===
volatile Uint64	giTime_TSCAtLastTick = 0;
volatile Uint64	giTime_TSCPerTick = 0;

// === PROTOTYPES ===
//Sint64	now(void);
 int	Time_Setup(void);
void	Time_Interrupt(int IRQ, void *Ptr);
Uint64	Time_ReadTSC(void);

// === CODE ===
/**
 * \fn Sint64 now()
 * \brief Return the current timestamp
 */
Sint64 now(void)
{
	Uint64	tsc = Time_ReadTSC();
	tsc -= giTime_TSCAtLastTick;
	tsc *= MS_PER_TICK_WHOLE;
	if( giTime_TSCPerTick ) {
		tsc /= giTime_TSCPerTick;
	}
	else
		tsc = 0;
	return giTimestamp + tsc;
}

/**
 * \fn int Time_Setup(void)
 * \brief Sets the system time from the Realtime-Clock
 */
int Time_Setup(void)
{
	Uint8	val;
	
	Log_Log("Timer", "RTC Timer firing at %iHz (%i divisor), %i.0x%08x",
		TIMER_FREQ, TIMER_RATE, MS_PER_TICK_WHOLE, MS_PER_TICK_FRACT);
	
	outb(0x70, inb(0x70)&0x7F);	// Disable NMIs
	__asm__ __volatile__ ("cli");	// Disable normal interrupts
	
	// Set IRQ8 firing rate
	outb(0x70, 0x0A);	// Set the index to register A
	val = inb(0x71); // Get the current value of register A
	val &= 0xF0;
	val |= TIMER_RATE+1;
	outb(0x70, 0x0A); // Reset index to A
	outb(0x71, val);	// Update the timer rate
		
	// Enable IRQ8
	outb(0x70, 0x0B);	// Set the index to register B
	val = inb(0x71);	// Read the current value of register B
	outb(0x70, 0x0B);	// Set the index again (a read will reset the index to register D)
	outb(0x71, val | 0x40);	// Write the previous value or'd with 0x40. This turns on bit 6 of register D
	
	__asm__ __volatile__ ("sti");	// Re-enable normal interrupts
	outb(0x70, inb(0x70)|0x80);	// Re-enable NMIs
	
	// Install IRQ Handler
	IRQ_AddHandler(8, Time_Interrupt, NULL);
	
	// Make sure the RTC actually fires
	outb(0x70, 0x0C); // Select register C
	inb(0x71);	// Just throw away contents.
	
	return 0;
}

/**
 * \brief Called on the timekeeping IRQ
 */
void Time_Interrupt(int IRQ, void *Ptr)
{
	Uint64	curTSC = Time_ReadTSC();
	
	if( giTime_TSCAtLastTick )
	{
		giTime_TSCPerTick = curTSC - giTime_TSCAtLastTick;
		//Debug("TSC Frequency is %llu-%llu = %llu Hz", curTSC, giTime_TSCAtLastTick, giTime_TSCPerTick*2);
	}
	giTime_TSCAtLastTick = curTSC;
	
	giTicks ++;
	giTimestamp += MS_PER_TICK_WHOLE;
	giPartMiliseconds += MS_PER_TICK_FRACT;
	if(giPartMiliseconds > 0x80000000) {
		giTimestamp ++;
		giPartMiliseconds -= 0x80000000;
	}
	
	Timer_CallTimers();

	// Make sure the RTC Fires again
	outb(0x70, 0x0C); // Select register C
	inb(0x71);	// Just throw away contents.
}

void Time_MicroSleep(Uint16 Microsecs)	// max 64 ms
{
	Uint64	cur_tsc = Time_ReadTSC();
	// tsc_per_us * Microsec
	Uint64	delta_tsc = (Uint64)Microsecs * giTime_TSCPerTick / US_PER_TICK;
	Uint64	tgt_tsc = cur_tsc + delta_tsc;

	if( tgt_tsc < cur_tsc )
		while(Time_ReadTSC() > cur_tsc)
			;	

	while( Time_ReadTSC() < tgt_tsc )
		;
}

Uint64 Time_ReadTSC(void)
{
	Uint32	a, d;
	__asm__ __volatile__ ("rdtsc" : "=a" (a), "=d" (d));
	return ((Uint64)d << 32) | a;
}
