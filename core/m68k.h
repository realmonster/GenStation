/*
    This file is part of GenStation.
 
    GenStation is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 
    GenStation is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with GenStation.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef M68K_H
#define M68K_H
#pragma once

#include <stdint.h>

#define M68K_REG_D0   0
#define M68K_REG_D1   1
#define M68K_REG_D2   2
#define M68K_REG_D3   3
#define M68K_REG_D4   4
#define M68K_REG_D5   5
#define M68K_REG_D6   6
#define M68K_REG_D7   7
#define M68K_REG_A0   8
#define M68K_REG_A1   9
#define M68K_REG_A2  10
#define M68K_REG_A3  11
#define M68K_REG_A4  12
#define M68K_REG_A5  13
#define M68K_REG_A6  14
#define M68K_REG_A7  15
#define M68K_REG_USP 15 // same as A7
#define M68K_REG_PC  16
#define M68K_REG_SR  17
#define M68K_REG_CCR 17 // low byte SR

#define M68K_FLAG_C_BIT 0 // carry
#define M68K_FLAG_C_MASK (1<<0)
#define M68K_FLAG_V_BIT 1 // overflow
#define M68K_FLAG_V_MASK (1<<1)
#define M68K_FLAG_Z_BIT 2 // zero
#define M68K_FLAG_Z_MASK (1<<2)
#define M68K_FLAG_N_BIT 3 // negative
#define M68K_FLAG_N_MASK (1<<3)
#define M68K_FLAG_X_BIT 4 // extend
#define M68K_FLAG_X_MASK (1<<4)

#define M68K_FLAG_I0_BIT 8 // interupt priority bit 0
#define M68K_FLAG_I0_MASK (1<<8)
#define M68K_FLAG_I1_BIT 9 // interupt priority bit 1
#define M68K_FLAG_I1_MASK (1<<9)
#define M68K_FLAG_I2_BIT 10 // interupt priority bit 2
#define M68K_FLAG_I2_MASK (1<<10)

// master/interrupt state (not used in M68000)
//#define M68K_FLAG_M_BIT 12
//#define M68K_FLAG_M_MASK (1<<12)

#define M68K_FLAG_S_BIT 13 // supervisor
#define M68K_FLAG_S_MASK (1<<13)

// trace 0 bit (not used in M68000)
//#define M68K_FLAG_T0_BIT 14
//#define M68K_FLAG_T0_MASK (1<<14)

#define M68K_FLAG_T1_BIT 15  // trace 1 bit
#define M68K_FLAG_T1_MASK (1<<15)

#define M68K_REG_COUNT 18

typedef struct m68k_context_ m68k_context;

#define M68K_FUNCTION(name) static void name(m68k_context* m68k)
typedef void (*m68k_function)(m68k_context* m68k);
typedef uint32_t (*m68k_read_handler)(m68k_context* m68k, uint32_t address);
typedef void (*m68k_write_handler)(m68k_context* m68k, uint32_t address, uint32_t value);

struct m68k_context_
{
	uint32_t reg[M68K_REG_COUNT];
	uint32_t timeout;
	m68k_function next_func,fetch_ret,effective_ret;
	m68k_read_handler read_w;
	m68k_write_handler write_w;

	// current operation data
	uint32_t opcode;
	uint32_t opcode_length;
	uint32_t fetched_value;
	uint32_t effective_value;
	uint32_t effective_address;
	uint32_t operand;
	uint32_t operand2;
};

void m68k_init(m68k_context *m68k);

void m68k_update(m68k_context *m68k);

#endif