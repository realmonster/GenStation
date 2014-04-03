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

#include "m68k.h"
#include "m68k_opcode.h"

M68K_FUNCTION(invalid)
{
	printf("invalid");
	TIMEOUT(1<<20, invalid); // almost maximum int32
}

M68K_FUNCTION(opcode_read)
{
	m68k->opcode = m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);
	//printf("%04X\n",m68k->opcode);
	m68k->reg[M68K_REG_PC] += 2;
	m68k_opcode_table[m68k->opcode](m68k);
}

M68K_FUNCTION(opcode_wait)
{
	FETCH_OPCODE;
}

M68K_FUNCTION(done_wait_wb) { WAIT_BUS(done_wait_wb, done_write_wb); }

M68K_FUNCTION(done_write_wb)
{
	m68k->write_w(m68k, m68k->effective_address, m68k->effective_value);
	FETCH_OPCODE;
}

M68K_FUNCTION(done_wait_ww) { WAIT_BUS(done_wait_ww, done_write_ww); }

M68K_FUNCTION(done_write_ww)
{
	m68k->write_w(m68k, m68k->effective_address, m68k->effective_value);
	FETCH_OPCODE;
}

M68K_FUNCTION(done_wait_wl) { WAIT_BUS(done_wait_wl, done_write_wl); }

M68K_FUNCTION(done_write_wl)
{
	m68k->write_w(m68k, m68k->effective_address, m68k->effective_value>>16);
	WAIT_BUS(done_wait_wl2, done_write_wl2);
}

M68K_FUNCTION(done_wait_wl2) { WAIT_BUS(done_wait_wl2, done_write_wl2); }

M68K_FUNCTION(done_write_wl2)
{
	m68k->write_w(m68k, m68k->effective_address+2, m68k->effective_value);
	FETCH_OPCODE;
}

void m68k_init(m68k_context *m68k)
{
	//if (!opcode_table[0])
	//	build_opcode_table();

	memset(m68k->reg, 0, sizeof(m68k->reg));
	FETCH_OPCODE;
}
