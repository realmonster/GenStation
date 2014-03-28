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
	m68k_function next_func;
	uint32_t opcode;
	m68k_read_handler read_b,read_w,read_l;
	m68k_read_handler write_b,write_w,write_l;
};

void m68k_init(m68k_context *m68k);

void m68k_update(m68k_context *m68k);

#endif