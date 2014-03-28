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
#include <stdio.h>
#include <stdlib.h>

static m68k_function opcode_table[0x10000];
static char* op_size[3]={"b","w","l"};

M68K_FUNCTION(invalid)
{
	printf("invalid\n");
	m68k->timeout = (1<<30); // almost maximum int32
}

M68K_FUNCTION(ori_ccr)
{
	printf("ori to ccr\n");
	m68k->timeout = 2; // I don't know
}

// 00000000sseeeeee
M68K_FUNCTION(ori)
{
	if ((m68k->opcode&0xC0) == 0xC0)
		printf("invalid\n");
	else
		printf("ori.%s \n",op_size[(m68k->opcode>>6)&3]);
	m68k->timeout = 2; // I don't know
}

M68K_FUNCTION(opcode_decode)
{
	opcode_table[m68k->opcode](m68k);
}

M68K_FUNCTION(fetch)
{
	printf("fetch\n");
	m68k->opcode = (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]++);
	m68k->next_func = opcode_decode;
	m68k->timeout = 2;
}

typedef struct
{
	uint32_t mask;
	uint32_t value;
	m68k_function handler;
} opcode_pattern;

opcode_pattern opcode_table_init[] =
// 	  mask, value, handler
{
	{0x0000,0x0000,invalid}, // fill all with invalid opcode
	{0xFF00,0x0000,ori    },
	{0xFFFF,0x003C,ori_ccr},
	{0x0000,0x0000,0} // end of list determinated by null function
};

void build_opcode_table()
{
	int i;
	opcode_pattern *p = opcode_table_init;

	while (p->handler)
	{
		// iterating submasks
		i = 0;
		do
		{
			i = (i-1) & (~p->mask) & 0xFFFF;
			opcode_table[i|p->value] = p->handler;
		}
		while (i != 0);

		// next pattern
		++p;
	}
}

void m68k_init(m68k_context *m68k)
{
	if (!opcode_table[0])
		build_opcode_table();

	memset(m68k->reg, 0, sizeof(m68k->reg));
	m68k->timeout = 2;
	m68k->next_func = fetch;
}

void m68k_update(m68k_context *m68k)
{
	if (!(--m68k->timeout))
		m68k->next_func(m68k);
}
