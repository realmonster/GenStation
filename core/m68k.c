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

#define lprintf if (1) printf

static m68k_function opcode_table[0x10000];
static char* op_size[3]={"b","w","l"};
uint32_t opcode_length[4]={1,2,4,0};

M68K_FUNCTION(opcode_decode)
{
	m68k->opcode = m68k->fetched_value;
	opcode_table[m68k->opcode](m68k);
}

#define FETCH(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = fetch
#define TIMEOUT(time,next) m68k->timeout = (time), m68k->next_func = (next)
#define FETCH_OPCODE FETCH(opcode_decode)
#define INVALID do {invalid(m68k); return;} while(0)

M68K_FUNCTION(fetch)
{
	lprintf("fetch\n");
	m68k->fetched_value = (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);
	m68k->reg[M68K_REG_PC] += 2;
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(invalid)
{
	lprintf("invalid\n");
	TIMEOUT((1<<30),invalid); // almost maximum int32
}

M68K_FUNCTION(effective_address_1)
{
	m68k->effective_value |= (uint16_t)m68k->read_w(m68k,m68k->effective_address+2);
	m68k->effective_ret(m68k);
}

M68K_FUNCTION(effective_address_2)
{
	switch (m68k->opcode_length)
	{
		case 4: // long
			if (m68k->effective_address&1)
				INVALID; // address bus error
			else
			{
				m68k->effective_value = m68k->read_w(m68k,m68k->effective_address)<<16;
				TIMEOUT(4,effective_address_1);
			}
			break;
		case 2:
			if (m68k->effective_address&1)
				INVALID; // address bus error
			else
			{
				m68k->effective_value = m68k->read_w(m68k,m68k->effective_address);
				m68k->effective_ret(m68k);
			}
			break;
		case 1:
			m68k->effective_value = m68k->read_w(m68k,m68k->effective_address);
			if (m68k->effective_address&1)
				m68k->effective_value >>= 8;
			m68k->effective_ret(m68k);
			break;
		default:
			INVALID;
	}
}

M68K_FUNCTION(effective_address_4)
{
	uint32_t brief = m68k->fetched_value;
	m68k->effective_address += (int8_t)brief;
	lprintf("ea: ($%02X,??,%c%d.%c)\n", brief&0xFF,
		brief&0x8000?'a':'d', (brief>>12)&7, brief&0x800?'l':'w');
	if (brief&0x800)
		m68k->effective_address += (int32_t)m68k->reg[M68K_REG_D0+((brief>>12)&0xF)];
	else
		m68k->effective_address += (int16_t)m68k->reg[M68K_REG_D0+((brief>>12)&0xF)];
	TIMEOUT(4,effective_address_2);
}

M68K_FUNCTION(effective_address_3)
{
	m68k->effective_address += (int16_t)m68k->fetched_value;
	lprintf("ea: ($%04X,??)\n", m68k->fetched_value&0xFFFF);
	TIMEOUT(4,effective_address_2);
}

M68K_FUNCTION(effective_address_5)
{
	m68k->effective_address |= m68k->fetched_value;
	lprintf("ea: ($%X)\n", m68k->effective_address);
	TIMEOUT(4,effective_address_2);
}

M68K_FUNCTION(effective_address_6)
{
	m68k->effective_address = m68k->fetched_value<<16;
	FETCH(effective_address_5);
}

M68K_FUNCTION(effective_address_7)
{
	m68k->effective_value |= m68k->fetched_value;
	lprintf("ea: #%X\n", m68k->effective_value&(m68k->opcode_length==1?0xFF:~0));
	TIMEOUT(4,effective_address_2);
}

M68K_FUNCTION(effective_address_8)
{
	m68k->effective_value = m68k->fetched_value<<16;
	FETCH(effective_address_7);
}

M68K_FUNCTION(effective_address)
{
	switch(m68k->opcode&0x38)
	{
		case 0x00: // Dn
			lprintf("ea: d%d\n",(m68k->opcode&7));
			m68k->effective_value = m68k->reg[M68K_REG_D0+(m68k->opcode&7)];
			m68k->effective_ret(m68k);
			break;
		case 0x08: // An
			lprintf("ea: a%d\n",(m68k->opcode&7));
			m68k->effective_value = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			m68k->effective_ret(m68k);
			break;
		case 0x10: // (An)
			lprintf("ea: (a%d)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			TIMEOUT(4,effective_address_2);
			break;
		case 0x18: // (An)+
		{
			uint32_t *r = &m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			lprintf("ea: (a%d)+\n",(m68k->opcode&7));
			m68k->effective_address = *r;
			*r += m68k->opcode_length;
			TIMEOUT(4,effective_address_2);
		}
			break;
		case 0x20: // -(An)
		{
			uint32_t *r = &m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			lprintf("ea: -(a%d)\n",(m68k->opcode&7));
			*r -= m68k->opcode_length;
			m68k->effective_address = *r;
			TIMEOUT(6,effective_address_2);
		}
			break;
		case 0x28: // (d16,An)
			lprintf("ea: (d16,a%d)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			FETCH(effective_address_3);
			break;
		case 0x30: // (d8,An,xn)
			lprintf("ea: (d8,a%d,xn)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			FETCH(effective_address_4);
			break;
		case 0x38:
			switch (m68k->opcode&7)
			{
				case 0: // (xxx).W
					m68k->effective_address = 0;
					FETCH(effective_address_5);
					break;
				case 1: // (xxx).L
					FETCH(effective_address_6);
					break;
				case 2: // (d16,pc)
					lprintf("ea: (d16,pc)\n");
					m68k->effective_address = m68k->reg[M68K_REG_PC];
					FETCH(effective_address_3);
					break;
				case 3: // (d8,pc,xn)
					lprintf("ea: (d8,pc,xn)\n");
					m68k->effective_address = m68k->reg[M68K_REG_PC];
					FETCH(effective_address_4);
					break;
				case 4: // #<data>
					lprintf("ea: #<data>\n");
					switch(m68k->opcode_length)
					{
						case 1:
							m68k->effective_value = 0;
							FETCH(effective_address_7);
							break;
						case 2:
							m68k->effective_value = 0;
							FETCH(effective_address_7);
							break;
						case 4:
							FETCH(effective_address_8);
							break;
						default:
							INVALID;
					}
					break;
				default:
					INVALID;
					break;
			}
			break;
		default:
			INVALID;
			break;
	}
}

M68K_FUNCTION(ori_ccr_3)
{
	lprintf("ori.b #$%02X,ccr\n",m68k->fetched_value);
	FETCH_OPCODE;
}

M68K_FUNCTION(ori_ccr_2)
{
	//if (m68k->fetched_value > 0xFFFF)
	//    what to do?
	TIMEOUT(16,ori_ccr_3); // from reference, but I'm not sure
}

M68K_FUNCTION(ori_ccr)
{ 
	FETCH(ori_ccr_2);
}

M68K_FUNCTION(ori_5)
{
	m68k->effective_value |= m68k->operand;
	lprintf("ori done\n");
	FETCH_OPCODE;
}

M68K_FUNCTION(ori_4)
{
	lprintf("imm: #$%X\n",m68k->operand);
	m68k->effective_ret = ori_5;
	effective_address(m68k);
}

M68K_FUNCTION(ori_3)
{
	m68k->operand |= m68k->fetched_value;
	ori_4(m68k);
}

M68K_FUNCTION(ori_2)
{
	switch(m68k->opcode_length)
	{
		case 1:
		case 2:
			m68k->operand = m68k->fetched_value;
			ori_4(m68k);
			break;
		case 4:
			m68k->operand = m68k->fetched_value<<16;
			FETCH(ori_3);
			break;
		default:
			INVALID;
	}
}

// 00000000sseeeeee
M68K_FUNCTION(ori)
{
	m68k->opcode_length = opcode_length[(m68k->opcode>>6)&3];
	if (!m68k->opcode_length)
		INVALID;
	lprintf("ori.%s\n",op_size[(m68k->opcode>>6)&3]);
	FETCH(ori_2);
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
	FETCH_OPCODE;
}

void m68k_update(m68k_context *m68k)
{
	if (!(--m68k->timeout))
		m68k->next_func(m68k);
}
