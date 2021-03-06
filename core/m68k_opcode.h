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
#define ADDRESS_EXCEPTION if (1) TIMEOUT(50-4*(4+7), address_exception); else (void*)0
#define PRIVILEGE_EXCEPTION INVALID
#define HALT

#define BUS_BUSY (m68k->fetched_value&1) // just to avoid optimization
#define SUPERVISOR (SR & M68K_FLAG_S_MASK)

#define SAVE_CURRENT_STACK \
if (SUPERVISOR) SSP = SP; \
else USP = SP

#define GET_CURRENT_STACK \
if (SUPERVISOR) SP = SSP; \
else SP = USP

#define BUS_WAIT_TIME 1
#define READ_WAIT_TIME 4

#define PC (m68k->reg[M68K_REG_PC])
#define SR (m68k->reg[M68K_REG_SR])
#define SP (m68k->reg[M68K_REG_A7])
#define USP (m68k->reg[M68K_REG_USP])
#define SSP (m68k->reg[M68K_REG_SSP])
#define EA (m68k->effective_address)
#define EV (m68k->effective_value)
#define OP (m68k->operand)

#define REG_D(n) (m68k->reg[M68K_REG_D0+(n)])
#define REG_A(n) (m68k->reg[M68K_REG_A0+(n)])

#define READ_16(address) ((uint16_t)(m68k->read_w(m68k, (address))))
#define READ_8(address) ((uint8_t)(READ_16(address&(~1))>>((address)&1?0:8)))

#define WRITE_16(address, value) m68k->write_w(m68k, (address), (value))
#define WRITE_8(address, value) m68k->write_b(m68k, (address), (value))

#define GET_FLAG(bit) ((m68k->reg[M68K_REG_SR] >> (bit))&1)
#define GET_X_FLAG()  (GET_FLAG(M68K_FLAG_X_BIT))
#define GET_N_FLAG()  (GET_FLAG(M68K_FLAG_N_BIT))
#define GET_Z_FLAG()  (GET_FLAG(M68K_FLAG_Z_BIT))
#define GET_V_FLAG()  (GET_FLAG(M68K_FLAG_V_BIT))
#define GET_C_FLAG()  (GET_FLAG(M68K_FLAG_C_BIT))

#define SET_FLAG(bit,val) m68k->reg[M68K_REG_SR] = (m68k->reg[M68K_REG_SR]&(~(1<<(bit))))|((val)<<(bit))
#define SET_X_FLAG(val) SET_FLAG(M68K_FLAG_X_BIT,val)
#define SET_N_FLAG(val) SET_FLAG(M68K_FLAG_N_BIT,val)
#define SET_Z_FLAG(val) SET_FLAG(M68K_FLAG_Z_BIT,val)
#define SET_V_FLAG(val) SET_FLAG(M68K_FLAG_V_BIT,val)
#define SET_C_FLAG(val) SET_FLAG(M68K_FLAG_C_BIT,val)

#define SET_N_FLAG8(val) SET_N_FLAG(((val)>>7)&1)
#define SET_N_FLAG16(val) SET_N_FLAG(((val)>>15)&1)
#define SET_N_FLAG32(val) SET_N_FLAG(((val)>>31)&1)

#define SET_Z_FLAG8(val) SET_Z_FLAG(((uint8_t)(val))?0:1)
#define SET_Z_FLAG16(val) SET_Z_FLAG(((uint16_t)(val))?0:1)
#define SET_Z_FLAG32(val) SET_Z_FLAG(((uint32_t)(val))?0:1)

#define SET_V_FLAG8(src, dest, res) SET_V_FLAG((int8_t)(((src)^(res))&((dest)^(res)))<0?1:0)
#define SET_V_FLAG16(src, dest, res) SET_V_FLAG((int16_t)(((src)^(res))&((dest)^(res)))<0?1:0)
#define SET_V_FLAG32(src, dest, res) SET_V_FLAG((int32_t)(((src)^(res))&((dest)^(res)))<0?1:0)

#define SET_VAR8(var, val) (var) = ((var)&(~0xFF))|(val)
#define SET_VAR16(var, val) (var) = ((var)&(~0xFFFF))|(val)
#define SET_VAR32(var, val) (var) = val

#define SET_DN_REG8(n, val)  SET_VAR8 (REG_D(n), (val))
#define SET_DN_REG16(n, val) SET_VAR16(REG_D(n), (val))
#define SET_DN_REG32(n, val) SET_VAR32(REG_D(n), (val))

#define SET_AN_REG8(n, val)  SET_VAR8 (REG_A(n), (val))
#define SET_AN_REG16(n, val) SET_VAR16(REG_A(n), (val))
#define SET_AN_REG32(n, val) SET_VAR32(REG_A(n), (val))

#define CONDITION_T (1)
#define CONDITION_F (0)
#define CONDITION_HI (!GET_C_FLAG() && !GET_Z_FLAG())
#define CONDITION_LS (GET_C_FLAG() || GET_Z_FLAG())
#define CONDITION_CC (!GET_C_FLAG())
#define CONDITION_CS (GET_C_FLAG() != 0)
#define CONDITION_NE (!GET_Z_FLAG())
#define CONDITION_EQ (GET_Z_FLAG() != 0)
#define CONDITION_VC (!GET_V_FLAG())
#define CONDITION_VS (GET_V_FLAG() != 0)
#define CONDITION_PL (!GET_N_FLAG())
#define CONDITION_MI (GET_N_FLAG() != 0)
#define CONDITION_GE ((GET_N_FLAG() != 0 && GET_V_FLAG() != 0) || (!GET_N_FLAG() && !GET_V_FLAG()))
#define CONDITION_LT ((GET_N_FLAG() != 0 && !GET_V_FLAG()) || (!GET_N_FLAG() && GET_V_FLAG() != 0))
#define CONDITION_GT (!GET_Z_FLAG() && (CONDITION_GE))
#define CONDITION_LE (GET_Z_FLAG() != 0 || (CONDITION_LT))

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
