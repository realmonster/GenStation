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

#define SET_FLAG(bit,val) m68k->reg[M68K_REG_SR] = (m68k->reg[M68K_REG_SR]&(~(1<<(bit))))|((val)<<(bit))

#define FETCH_WORD(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = fetch_word
#define FETCH_LONG(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = fetch_long
#define FETCH_OPCODE FETCH_WORD(opcode_decode)

#define TIMEOUT(time,next) m68k->timeout = (time), m68k->next_func = (next)

#define READ_WORD(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = read_word
#define READ_LONG(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = read_long

#define READ_WORD(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = read_word
#define READ_LONG(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = read_long

#define WRITE_WORD(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = write_word
#define WRITE_LONG(next) m68k->fetch_ret=(next), m68k->timeout = 4, m68k->next_func = write_long

#define INVALID if (1) {invalid(m68k); return;} else (void)0

#define FETCH_BY_SIZE(size, next) if(1) \
{ \
if ((size) == 1) \
	FETCH_WORD(next); \
else if ((size) == 2) \
	FETCH_WORD(next); \
else if ((size) == 4) \
	FETCH_LONG(next); \
else \
	INVALID; \
} else (void)0

#define READ_BY_SIZE(size, next) if(1) \
{ \
if ((size) == 1) \
	READ_WORD(next); \
else if ((size) == 2) \
	READ_WORD(next); \
else if ((size) == 4) \
	READ_LONG(next); \
else \
	INVALID; \
} else (void)0

#define WRITE_BY_SIZE(size, next) if(1) \
{ \
if ((size) == 1) \
	WRITE_WORD(next); \
else if ((size) == 2) \
	WRITE_WORD(next); \
else if ((size) == 4) \
	WRITE_LONG(next); \
else \
	INVALID; \
} else (void)0

M68K_FUNCTION(fetch_word)
{
	lprintf("fetch\n");
	m68k->fetched_value = m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);
	m68k->reg[M68K_REG_PC] += 2;
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(fetch_long_2)
{
	lprintf("fetch\n");
	m68k->fetched_value |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);
	m68k->reg[M68K_REG_PC] += 2;
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(fetch_long)
{
	lprintf("fetch\n");
	m68k->fetched_value = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;
	m68k->reg[M68K_REG_PC] += 2;
	TIMEOUT(4, fetch_long_2);
}

M68K_FUNCTION(read_word)
{
	//lprintf("read\n");
	m68k->fetched_value = m68k->read_w(m68k, m68k->effective_address);
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(read_long_2)
{
	//lprintf("read\n");
	m68k->fetched_value |= (uint16_t)m68k->read_w(m68k, m68k->effective_address+2);
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(read_long)
{
	//lprintf("read\n");
	m68k->fetched_value = m68k->read_w(m68k, m68k->effective_address)<<16;
	TIMEOUT(4, read_long_2);
}

M68K_FUNCTION(write_word)
{
	//lprintf("write\n");
	m68k->write_w(m68k, m68k->effective_address, m68k->effective_value);
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(write_long_2)
{
	//lprintf("write\n");
	m68k->write_w(m68k, m68k->effective_address+2, m68k->effective_value);
	m68k->fetch_ret(m68k);
}

M68K_FUNCTION(write_long)
{
	//lprintf("write\n");
	m68k->write_w(m68k, m68k->effective_address, m68k->effective_value>>16);
	TIMEOUT(4, write_long_2);
}

M68K_FUNCTION(invalid)
{
	lprintf("invalid\n");
	TIMEOUT((1<<30),invalid); // almost maximum int32
}

M68K_FUNCTION(effective_address_1)
{
	m68k->effective_value = m68k->fetched_value;
	m68k->effective_ret(m68k);
}

M68K_FUNCTION(effective_address_2)
{
	m68k->effective_address += (int16_t)m68k->fetched_value;
	lprintf("ea: ($%04X,??)\n", m68k->fetched_value&0xFFFF);
	READ_BY_SIZE(m68k->opcode_length, effective_address_1);
}

M68K_FUNCTION(effective_address_3)
{
	uint32_t brief = m68k->fetched_value;
	m68k->effective_address += (int8_t)brief;
	lprintf("ea: ($%02X,??,%c%d.%c)\n", brief&0xFF,
		brief&0x8000?'a':'d', (brief>>12)&7, brief&0x800?'l':'w');
	if (brief&0x800)
		m68k->effective_address += (int32_t)m68k->reg[M68K_REG_D0+((brief>>12)&0xF)];
	else
		m68k->effective_address += (int16_t)m68k->reg[M68K_REG_D0+((brief>>12)&0xF)];
	READ_BY_SIZE(m68k->opcode_length, effective_address_1);
	m68k->timeout = 6;
}

M68K_FUNCTION(effective_address_4)
{
	m68k->effective_address = m68k->fetched_value;
	READ_BY_SIZE(m68k->opcode_length, effective_address_1);
}

M68K_FUNCTION(effective_address)
{
	switch(m68k->opcode&0x38)
	{
		case 0x00: // Dn
			lprintf("ea: d%d\n",(m68k->opcode&7));
			m68k->effective_address = 0;
			m68k->effective_value = m68k->reg[M68K_REG_D0+(m68k->opcode&7)];
			m68k->effective_ret(m68k);
			break;
		case 0x08: // An
			lprintf("ea: a%d\n",(m68k->opcode&7));
			m68k->effective_address = 0;
			m68k->effective_value = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			m68k->effective_ret(m68k);
			break;
		case 0x10: // (An)
			lprintf("ea: (a%d)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			READ_BY_SIZE(m68k->opcode_length, effective_address_1);
			break;
		case 0x18: // (An)+
		{
			uint32_t *r = &m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			lprintf("ea: (a%d)+\n",(m68k->opcode&7));
			m68k->effective_address = *r;
			*r += m68k->opcode_length;
			READ_BY_SIZE(m68k->opcode_length, effective_address_1);
		}
			break;
		case 0x20: // -(An)
		{
			uint32_t *r = &m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			lprintf("ea: -(a%d)\n",(m68k->opcode&7));
			*r -= m68k->opcode_length;
			m68k->effective_address = *r;
			READ_BY_SIZE(m68k->opcode_length, effective_address_1);
			m68k->timeout = 6;
		}
			break;
		case 0x28: // (d16,An)
			lprintf("ea: (d16,a%d)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			FETCH_WORD(effective_address_2);
			break;
		case 0x30: // (d8,An,xn)
			lprintf("ea: (d8,a%d,xn)\n",(m68k->opcode&7));
			m68k->effective_address = m68k->reg[M68K_REG_A0+(m68k->opcode&7)];
			FETCH_WORD(effective_address_3);
			break;
		case 0x38:
			switch (m68k->opcode&7)
			{
				case 0: // (xxx).W
					FETCH_WORD(effective_address_4);
					break;
				case 1: // (xxx).L
					FETCH_LONG(effective_address_4);
					break;
				case 2: // (d16,pc)
					lprintf("ea: (d16,pc)\n");
					m68k->effective_address = m68k->reg[M68K_REG_PC];
					FETCH_WORD(effective_address_2);
					break;
				case 3: // (d8,pc,xn)
					lprintf("ea: (d8,pc,xn)\n");
					m68k->effective_address = m68k->reg[M68K_REG_PC];
					FETCH_WORD(effective_address_3);
					break;
				case 4: // #<data>
					lprintf("ea: #<data>\n");
					m68k->effective_address = 0;
					FETCH_BY_SIZE(m68k->opcode_length, effective_address_1);
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

M68K_FUNCTION(effective_address_write)
{
	switch(m68k->opcode&0x38)
	{
		case 0x00: // Dn
		case 0x08: // An
			m68k->reg[M68K_REG_D0+(m68k->opcode&0xF)] = m68k->effective_value;
			m68k->effective_ret(m68k);
			break;
		case 0x10: // (An)
		case 0x18: // (An)+
		case 0x20: // -(An)
		case 0x28: // (d16,An)
		case 0x30: // (d8,An,xn)
			WRITE_BY_SIZE(m68k->opcode_length, m68k->effective_ret);
			break;
		case 0x38:
			switch (m68k->opcode&7)
			{
				case 0: // (xxx).W
				case 1: // (xxx).L
				case 2: // (d16,pc)
				case 3: // (d8,pc,xn)
					WRITE_BY_SIZE(m68k->opcode_length, m68k->effective_ret);
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
	FETCH_WORD(ori_ccr_2);
}

M68K_FUNCTION(ori_4)
{
	lprintf("ori done\n");
	FETCH_OPCODE;
}

M68K_FUNCTION(ori_3)
{
	int flag_n, flag_z;
	if (m68k->opcode_length == 1)
	{
		if (m68k->effective_address&1)
		{
			m68k->effective_value |= (uint32_t)((uint8_t)m68k->operand)<<8;
			flag_n = (m68k->effective_value>>15)&1;
			flag_z = (m68k->effective_value&0xFF00?0:1);
		}
		else
		{
			m68k->effective_value |= (uint8_t)m68k->operand;
			flag_n = (m68k->effective_value>>7)&1;
			flag_z = (((uint8_t)m68k->effective_value)?0:1);
		}
	}
	else if (m68k->opcode_length == 2)
	{
		m68k->effective_value |= (uint16_t)m68k->operand;
		flag_n = (m68k->effective_value>>15)&1;
		flag_z = (((uint16_t)m68k->effective_value)?0:1);
	}
	else
	{
		m68k->effective_value |= (uint32_t)m68k->operand;
		flag_n = (m68k->effective_value>>31)&1;
		flag_z = (((uint32_t)m68k->effective_value)?0:1);
	}
	SET_FLAG(M68K_FLAG_N_BIT, flag_n);
	SET_FLAG(M68K_FLAG_Z_BIT, flag_z);
	SET_FLAG(M68K_FLAG_V_BIT, 0);
	SET_FLAG(M68K_FLAG_C_BIT, 0);
	m68k->effective_ret = ori_4;
	effective_address_write(m68k);
}

M68K_FUNCTION(ori_2)
{
	m68k->operand = m68k->fetched_value;
	lprintf("imm: #$%X\n",m68k->operand);
	m68k->effective_ret = ori_3;
	effective_address(m68k);
}

// 00000000sseeeeee
M68K_FUNCTION(ori)
{
	m68k->opcode_length = opcode_length[(m68k->opcode>>6)&3];
	if (!m68k->opcode_length)
		INVALID;
	lprintf("ori.%s\n",op_size[(m68k->opcode>>6)&3]);
	FETCH_BY_SIZE(m68k->opcode_length, ori_2);
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
