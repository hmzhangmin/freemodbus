/*
	FreeRTOS V4.1.0 - Copyright (C) 2003-2006 Richard Barry. 
    Modification for STR71X/GCC -  Copyright (C) 2006 Christian Walter.

	This file is part of the FreeRTOS distribution.

	FreeRTOS is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	FreeRTOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeRTOS; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	A special exception to the GPL can be applied should you wish to distribute
	a combined work that includes FreeRTOS, without being obliged to provide
	the source code for any proprietary components.  See the licensing section 
	of http://www.FreeRTOS.org for full details of how and when the exception
	can be applied.

	***************************************************************************
	See http://www.FreeRTOS.org for documentation, latest information, license 
	and contact details.  Please ensure to read the configuration and relevant 
	port sections of the online documentation.
	***************************************************************************
*/

/* ----------------------- System incldues ----------------------------------*/
#include <stdlib.h>

/* ----------------------- FreeRTOS -----------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"

/* ----------------------- STR71X includes ----------------------------------*/
#include "eic.h"
#include "wdg.h"

/* ----------------------- Defines ------------------------------------------*/

/* System mode, ARM mode, interrupts enabled. */
#define portINITIAL_SPSR                ( ( portSTACK_TYPE ) 0x1F )
#define portTHUMB_MODE_BIT				( ( portSTACK_TYPE ) 0x20 )
#define portINSTRUCTION_SIZE			( ( portSTACK_TYPE ) 4 )
#define portNO_CRITICAL_SECTION_NESTING	( ( portSTACK_TYPE ) 0 )

/* ----------------------- Static functions ---------------------------------*/

/* Setup the timer to generate the tick interrupts. */
static void     prvSetupTimerInterrupt( void );

/* Scheduler can only be started from ARM mode, so vPortISRStartFirstSTask()
 * is defined in portISR.c. 
 */
extern void     vPortISRStartFirstTask( void );

/* ----------------------- Start implementation -----------------------------*/

/* 
 * Initialise the stack of a task to look exactly as if a call to 
 * portSAVE_CONTEXT had been called.
 */
portSTACK_TYPE *
pxPortInitialiseStack( portSTACK_TYPE * pxTopOfStack, pdTASK_CODE pxCode, void *pvParameters )
{
    portSTACK_TYPE *pxOriginalTOS;

    pxOriginalTOS = pxTopOfStack;

    /* Setup the initial stack of the task.  The stack is set exactly as 
       expected by the portRESTORE_CONTEXT() macro. */

    /* First on the stack is the return address - which in this case is the
       start of the task.  The offset is added to make the return address appear
       as it would within an IRQ ISR. */
    *pxTopOfStack = ( portSTACK_TYPE ) pxCode + portINSTRUCTION_SIZE;
    pxTopOfStack--;

    *pxTopOfStack = ( portSTACK_TYPE ) 0xaaaaaaaa;      /* R14 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) pxOriginalTOS;   /* Stack used when task starts goes in R13. */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x12121212;      /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x11111111;      /* R11 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x10101010;      /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x09090909;      /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x08080808;      /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x07070707;      /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x06060606;      /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x05050505;      /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x04040404;      /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x03030303;      /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x02020202;      /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = ( portSTACK_TYPE ) 0x01010101;      /* R1 */
    pxTopOfStack--;

    /* When the task starts is will expect to find the function parameter in
       R0. */
    *pxTopOfStack = ( portSTACK_TYPE ) pvParameters;    /* R0 */
    pxTopOfStack--;

    /* The last thing onto the stack is the status register, which is set for
       system mode, with interrupts enabled. */
    *pxTopOfStack = ( portSTACK_TYPE ) portINITIAL_SPSR;

#ifdef THUMB_INTERWORK
    {
        /* We want the task to start in thumb mode. */
        *pxTopOfStack |= portTHUMB_MODE_BIT;
    }
#endif

    pxTopOfStack--;

    /* Some optimisation levels use the stack differently to others.  This 
       means the interrupt flags cannot always be stored on the stack and will
       instead be stored in a variable, which is then saved as part of the
       tasks context. */
    *pxTopOfStack = portNO_CRITICAL_SECTION_NESTING;

    return pxTopOfStack;
}

portBASE_TYPE
xPortStartScheduler( void )
{
    /* Start the timer that generates the tick ISR.  Interrupts are disabled
       here already. */
    prvSetupTimerInterrupt(  );

    /* Start the first task. */
    vPortISRStartFirstTask(  );

    /* Should not get here! */
    return 0;
}

void
vPortEndScheduler( void )
{
    /* It is unlikely that the ARM port will require this function as there
       is nothing to return to.  */
}

/* We use the interal watchdog timer to generate a periodic clock tick. */
static void
prvSetupTimerInterrupt( void )
{
    /* Set the watchdog up to generate a periodic tick. */
    WDG_ECITConfig( DISABLE );
    WDG_CntOnOffConfig( DISABLE );
    WDG_PeriodValueConfig( configTICK_RATE_HZ );

    /* Setup the tick interrupt in the EIC. */
    EIC_IRQChannelPriorityConfig( WDG_IRQChannel, 1 );
    EIC_IRQChannelConfig( WDG_IRQChannel, ENABLE );
    EIC_IRQConfig( ENABLE );
    WDG_ECITConfig( ENABLE );

    /* Start the timer - interrupts are actually disabled at this point so
       it is safe to do this here. */
    WDG_CntOnOffConfig( ENABLE );
}
