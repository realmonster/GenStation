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

#ifndef M68K_OPCODE_H
#define M68K_OPCODE_H
#pragma once

#include "m68k.h"
#include "m68k_optable.h"

#define INVALID invalid(m68k)
#define ADDRESS_EXCEPTION INVALID
#define BUS_BUSY (m68k->reg[M68K_REG_SR]&1) // just to avoid optimization
#define BUS_WAIT_TIME 1
#define READ_WAIT_TIME 4

#define SET_FLAG(bit,val) m68k->reg[M68K_REG_SR] = (m68k->reg[M68K_REG_SR]&(~(1<<(bit))))|((val)<<(bit))
#define SET_X_FLAG(val) SET_FLAG(M68K_FLAG_X_BIT,val)
#define SET_N_FLAG(val) SET_FLAG(M68K_FLAG_N_BIT,val)
#define SET_Z_FLAG(val) SET_FLAG(M68K_FLAG_Z_BIT,val)
#define SET_V_FLAG(val) SET_FLAG(M68K_FLAG_V_BIT,val)
#define SET_C_FLAG(val) SET_FLAG(M68K_FLAG_C_BIT,val)

#define WAIT_BUS(bus_wait,bus_access) \
if (BUS_BUSY) \
	TIMEOUT(BUS_WAIT_TIME, bus_wait); \
else \
	TIMEOUT(READ_WAIT_TIME, bus_access)

#define TIMEOUT(time,next) m68k->timeout = (time), m68k->next_func = (next)

#define FETCH_OPCODE if (BUS_BUSY) \
	TIMEOUT(1, opcode_wait); \
else \
	TIMEOUT(4, opcode_read)

#endif
