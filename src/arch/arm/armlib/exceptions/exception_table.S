/**
 * @file
 * @brief
 *
 * @author  Anton Kozlov
 * @date    10.09.2012
 */

/**
 * exceptions table:
 * 0x00 - reset
 * 0x04 - undefined
 * 0x08 - SWI
 * 0x0C - prefetch abort
 * 0x10 - data abort
 * 0x14 - reserved
 * 0x18 - IRQ
 * 0x1C - FIQ (_fast IRQ)
 */

#include <asm/modes.h>
#include <asm/regs.h>

.extern irq_handler
.extern setup_excpt_table
.extern reset_handler

.section .trap_table, "x"
.global arm_trap_table
arm_trap_table:
/* trap table
 * this code specific for ARM7TDMI cores
 *
 * actually, this is general for all ARM cores
 */
    ldr pc, reset_handler_addr
    ldr pc, undef_handler_addr
    ldr pc, swi_handler_addr
    ldr pc, prefetch_abt_handler_addr
    b data_abt_prepare
    nop /*bad exception not_used:		.word not_used*/
    ldr pc, irq_handler_addr
    ldr pc, fiq_handler_addr

reset_handler_addr:
    .word reset_handler
undef_handler_addr:
    .word undef_handler
swi_handler_addr:
    .word swi_handler
prefetch_abt_handler_addr:
    .word prefetch_abt_handler
.global data_abt_handler_addr
data_abt_handler_addr:
    .word data_abt_handler
irq_handler_addr:
    .word irq_handler
fiq_handler_addr:
    .word fiq_handler

data_abt_prepare:
	/* Prepare stack and call handler */
	stmfd r13!, {r0-r3, r14}
	add r14, pc, #0
	ldr pc, data_abt_handler_addr
	ldmfd r13!, {r0-r3, r14}
	subs pc, lr, #8 // exception return


