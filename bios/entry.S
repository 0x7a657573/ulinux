.set noreorder
.set noat

//this code carefully written to be position independent, lives at 0xbfc00000, entrries every 0x08 bytes

#include "../hypercall.h"

.globl entry
.section .text
entry:
	bal   boot_continue
	nop

entry_exec:
	b     promcall
	li    $v1, 4 * 1

entry_restart:			//linux calls this halt
	b     promcall
	li    $v1, 4 * 2

entry_3:
	b     promcall
	li    $v1, 4 * 3

entry_reboot:
	b     promcall
	li    $v1, 4 * 4

entry_autoboot:
	b     promcall
	li    $v1, 4 * 5

entry_open:
	b     promcall
	li    $v1, 4 * 6

entry_read:
	b     promcall
	li    $v1, 4 * 7

entry_write:
	b     promcall
	li    $v1, 4 * 8

entry_9:
	b     promcall
	li    $v1, 4 * 9

entry_close:
	b     promcall
	li    $v1, 4 * 10

entry_lseek:
	b     promcall
	li    $v1, 4 * 11

entry_getchar:
	b     promcall
	li    $v1, 4 * 12

entry_putchar:
	b     promcall
	li    $v1, 4 * 13

entry_14:
	b     promcall
	li    $v1, 4 * 14

entry_gets:
	b     promcall
	li    $v1, 4 * 15

entry_16:
	b     promcall
	li    $v1, 4 * 16

entry_printf:
	b     promcall
	li    $v1, 4 * 17

//a few jumps into here will break. ultrix does not do it, nor does linux
//saving bytes in "romboot" matters, so we live with the risk

//number of instrs here counts...
boot_continue:
	move  $a1, $zero	//PA of where we're loading in ram
	li    $at, H_STOR_READ
	.word HYPERCALL
fail_loop:
	beqz  $v0, fail_loop
	lui   $s0, 0x8000	//VA where we're loading
	jr    $s0
	nop

//number of instrs here counts...
promcall:
	lui   $at, 0x8000
	addiu $at, 0x1008
	jr    $at
	nop
	nop

//24
	bal .
	nop

//25
	bal .
	nop
//26
	bal .
	nop
//27
	bal .
	nop
//28
	bal .
	nop
//29
	bal .
	nop
//30
	bal .
	nop
//31
	bal .
	nop
//32
	bal .
	nop

entry_getenv:
	b     promcall
	li    $v1, 4 * 33


/*
	
	reset			000
	exec			008
	restart			010	linux calls this halt
	reboot			020
	autoboot		028
	open			030
	read			038
	write			040
	close			050
	lseek			058
	getchar			060
	putchar			068
	gets			078
	printf			088
	getenv			108
	setenv			110
	halt(ultrix)	1B0

*/
