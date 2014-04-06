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

#include <stdio.h>
#include <stdlib.h>

int is_checking(int opcode)
{
	return (opcode & 0x10000);
}

int check(int opcode)
{
	return (opcode | 0x10000);
}

int ea_mode(int opcode)
{
	switch (opcode & 0x38)
	{
		case 0x00: return 0; // Dn
		case 0x08: return 1; // An
		case 0x10: return 2; // (An)
		case 0x18: return 3; // (An)+
		case 0x20: return 4; // -(An)
		case 0x28: return 5; // (d16,An)
		case 0x30: return 6; // (d8,An,xn)
		case 0x38:
		switch (opcode & 7)
		{
			case 0: return 7; // (xxx).W
			case 1: return 8; // (xxx).L
			case 2: return 9; // (d16,pc,xn)
			case 3: return 10; // (d8,pc,xn)
			case 4: return 11; // #<data>
			default: return -1;
		}
		default: return -1;
	}
}

int ea_valid(int opcode)
{
	return (ea_mode(opcode) >= 0);
}

int ea_alterable_array[] = {1,1,1,1,1,1,1,1,1,0,0,0};

int ea_alterable(int opcode)
{
	int mode = ea_mode(opcode);
	if (mode < 0)
		return 0;
	return ea_alterable_array[mode];
}

// not An
int ea_alterable_na(int opcode)
{
	return (ea_mode(opcode) != 1 && ea_alterable(opcode));
}

int ea_address(int opcode)
{
	int mode = ea_mode(opcode);
	return (mode != 0) && (mode != 1) && (mode != 11);
}

#define MAX_NAME (100)
#define HASH_SIZE (1<<18)
#define HASH_MASK (HASH_SIZE-1)

int func_hash[HASH_SIZE];
int func_id[HASH_SIZE];
char **func_names = 0;
int func_count = 0;
int valid[0x10000];

void add_opcode(int func_id, int opcode)
{
	if (func_id<0)
	{
		fprintf(stderr, "Error: at generation of opcode %04X\n", opcode);
		exit(0);
	}

	if (func_id == invalid())
		return;

	if (valid[opcode] != invalid())
	{
		fprintf(stderr, "Error: two opcode handlers at same opcode number %04X\n", opcode);
		exit(0);
	}
	valid[opcode] = func_id;
}

void hash_init(void)
{
	int i;
	for (i=0; i<HASH_SIZE; ++i)
		func_id[i] = -1;
}

int hash_str(const char* str)
{
	int i, hash;
	hash = 0;
	for (i=0; str[i]; ++i)
	{
		hash *= 21; // random constant
		hash += str[i];
	}
	return hash;
}

void hash_insert(const char* name, int id)
{
	int i, c, hash;
	hash = hash_str(name);
	c = 0;
	for (i = hash&HASH_MASK; func_id[i] != -1; i = (i+1)&HASH_MASK)
		if (++c > HASH_SIZE*3)
		{
			fprintf(stderr, "Error! Hash is full! Increase HASH_SIZE\n");
			exit(0);
		}
	func_id[i] = id;
	func_hash[i] = hash;
}

int func_by_name(const char* name)
{
	int i, c, hash;
	hash = hash_str(name);
	c = 0;
	for (i = hash&HASH_MASK; func_id[i] != -1; i = (i+1)&HASH_MASK)
	{
		if (++c > HASH_SIZE*3)
		{
			fprintf(stderr, "Error! Hash is full! Increase HASH_SIZE\n");
			exit(0);
		}
		if (func_hash[i] == hash
		 && !strcmp(name, func_names[func_id[i]]))
			return func_id[i];
	}
	return -1;
}

int declare_function(const char* name)
{
	if (func_by_name(name) >= 0)
		return -1;

	++func_count;
	func_names = (char**)realloc(func_names, func_count * sizeof(char*));
	if (!func_names)
		return -1;

	func_names[func_count - 1] = (char*)malloc(strlen(name) + 1);
	if (!func_names[func_count - 1])
		return -1;

	hash_insert(name, func_count - 1);
	strcpy(func_names[func_count - 1], name);
	return func_count - 1;
}

void print_bus_wait(const char* bus_wait, const char* bus_access)
{
	int bw, ba;

	bw = declare_function(bus_wait);
	ba = declare_function(bus_access);
	printf("\tWAIT_BUS(%s, %s);\n}\n\n", bus_wait, bus_access);
	if (bw >= 0)
		printf("M68K_FUNCTION(%s) { WAIT_BUS(%s, %s); }\n\n", bus_wait, bus_wait, bus_access);
	if (ba >= 0)
		printf("M68K_FUNCTION(%s)\n{\n", bus_access);
}

#define WAIT_BUS(bus_wait_str, bus_access_str) if (1) {\
sprintf(wait_name, "%s" bus_wait_str, func_name); \
sprintf(access_name, "%s" bus_access_str, func_name); \
print_bus_wait(wait_name, access_name);\
} else (void*)0

void print_bus_read(const char* prefix, const char* address, const char* lval, int size)
{
	const char *func_name;
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];

	func_name = prefix;
	WAIT_BUS("_wait", "_read");

	switch (size)
	{
		case 0:
			printf("\t%s = READ_8(%s);\n", lval, address);
			break;

		case 1:
			printf("\t%s = READ_16(%s);\n", lval, address);
			break;

		case 2:
			printf("\t%s = READ_16(%s)<<16;\n", lval, address);

			WAIT_BUS("_wait2", "_read2");

			printf("\t%s |= READ_16(%s + 2);\n", lval, address);
			break;
	}
}

#define READ_BUS(str, address, lval, size) if (1) {\
sprintf(access_name, "%s" str, func_name); \
print_bus_read(access_name, address, lval, size);\
} else (void*)0

void print_bus_fetch(const char* prefix, const char* lval, int size)
{
	const char *func_name;
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];

	func_name = prefix;
	WAIT_BUS("_wait", "_read");

	switch (size)
	{
		case 0:
			printf("\t%s = (uint8_t)READ_16(PC);\n", lval);
			break;

		case 1:
			printf("\t%s = READ_16(PC);\n", lval);
			break;

		case 2:
			printf("\t%s = READ_16(PC)<<16;\n", lval);
			printf("\tPC += 2;\n");

			WAIT_BUS("_wait2", "_read2");

			printf("\t%s |= READ_16(PC);\n", lval);
			break;

		default:
			fprintf(stderr, "Error: invalid bus fetch size %d in %s\n", size, prefix);
			exit(0);
			break;
	}
	printf("\tPC += 2;\n");
}

#define FETCH_BUS(str, lval, size) if (1) {\
sprintf(access_name, "%s" str, func_name); \
print_bus_fetch(access_name, lval, size);\
} else (void*)0


int invalid(void)
{
	return func_by_name("invalid");
}

int get_ea(const char *func_name, int opcode, int op_size, int read)
{
	char wait_name[100];
	char access_name[100];

	switch(ea_mode(opcode))
	{
		default:
			return -1;

		case 0: // Dn
			printf("\tEV = REG_D(%d);\n", opcode&7);
			break;

		case 1: // An
			printf("\tEV = REG_A(%d);\n", opcode&7);
			break;

		case 2: // (An)
			printf("\tEA = REG_A(%d);\n", opcode&7);
			break;

		case 3: // (An)+
			printf("\tEA = REG_A(%d);\n", opcode&7);
			printf("\tREG_A(%d) += %d;\n", opcode&7, 1<<op_size);
			break;

		case 4: // -(An)
			if (read) // timing fix
			{
				sprintf(wait_name, "%s_wait_pre", func_name);
				printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

				declare_function(wait_name);
				printf("M68K_FUNCTION(%s)\n{\n",wait_name);
			}

			printf("\tREG_A(%d) -= %d;\n", opcode&7, 1<<op_size);
			printf("\tEA = REG_A(%d);\n", opcode&7);
			break;

		case 5: // (d16,An)
		case 9: // (d16,pc)
			WAIT_BUS("_wait_d16", "_read_d16");

			printf("\tEA = (int16_t)READ_16(PC) + ");
			if (ea_mode(opcode) == 5)
				printf("REG_A(%d);\n", opcode&7);
			else
				printf("PC;\n");

			printf("\tPC += 2;\n");
			break;

		case 6: // (d8,An,xn)
		case 10: // (d8,pc,xn)
			WAIT_BUS("_wait_d8", "_read_d8");

			printf("\tEA = READ_16(PC);\n");

			sprintf(wait_name, "%s_wait_d8_sum", func_name);
			declare_function(wait_name);

			printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

			printf("M68K_FUNCTION(%s)\n{\n", wait_name);
			printf(
"	if (EA & 0x800)\n"
"		EA = (uint32_t)(int8_t)EA\n"
"							+m68k->reg[M68K_REG_%c%c]\n"
"							+REG_D((EA>>12)&0xF);\n"
"	else\n"
"		EA = (uint32_t)(int8_t)EA\n"
"							+m68k->reg[M68K_REG_%c%c]\n"
"							+(int16_t)REG_D((EA>>12)&0xF);\n"
"	PC += 2;\n"
	, ea_mode(opcode)==6?'A':'P', ea_mode(opcode)==6?'0'+(opcode&7):'C',
	  ea_mode(opcode)==6?'A':'P', ea_mode(opcode)==6?'0'+(opcode&7):'C');

			break;

		case 7: // (xxx).W
			WAIT_BUS("_wait_w", "_read_w");

			printf("\tEA = (int16_t)READ_16(PC);\n");
			printf("\tPC += 2;\n");

			break;

		case 8: // (xxx).L
			WAIT_BUS("_wait_l", "_read_l");

			printf("\tEA = READ_16(PC)<<16;\n");
			printf("\tPC += 2;\n");

			WAIT_BUS("_wait_l2", "_read_l2");

			printf("\tEA |= (uint16_t)READ_16(PC);\n");
			printf("\tPC += 2;\n");

			break;

		case 11: // #<data>
			FETCH_BUS("_imm", "EV", op_size);
			break;
	}
	return 0;
}

typedef int (*opcode_handler)(int opcode);

int gen_immediate(const char *mnemonic, int opcode, opcode_handler handler)
{
	char func_name[MAX_NAME];
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];
	int func_id;
	int op_size;

	op_size = (opcode >> 6) & 3;
	if (handler(check(opcode)) < 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	FETCH_BUS("", "OP", op_size);

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		int id;
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%d", mnemonic, op_size);
		sprintf(access_name, "%s_read", func_name);
		id = func_by_name(access_name);
		if (id >= 0)
		{
			WAIT_BUS("_wait", "_read");
			return func_id;
		}
		READ_BUS("", "EA", "EV", op_size);
	}

	if (handler(opcode) < 0)
		return -1;
	if (ea_mode(opcode) < 2)
	{
		printf("\t\tSET_DN_REG%d(%d, result);\n\t}\n", 8<<op_size, opcode&0xF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		printf("\t\tEV = result;\n\t}\n");

		switch(op_size)
		{
			case 0: print_bus_wait("done_wait_wb", "done_write_wb"); break;
			case 1: print_bus_wait("done_wait_ww", "done_write_ww"); break;
			case 2: print_bus_wait("done_wait_wl", "done_write_wl"); break;
		}
	}
	return func_id;
}

int ori_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV | OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void ori(int opcode)
{
	if ((opcode & 0xFF00) != 0)
		return;

	add_opcode(gen_immediate("ori", opcode, ori_handler), opcode);
}

int andi_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV & OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void andi(int opcode)
{
	if ((opcode & 0xFF00) != 0x200)
		return;

	add_opcode(gen_immediate("andi", opcode, andi_handler), opcode);
}

int subi_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV - OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_C_FLAG((uint%d_t)EV < (uint%d_t)OP?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_X_FLAG((uint%d_t)EV < (uint%d_t)OP?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_V_FLAG%d(EV, -OP, result);\n", 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	return 0;
}

void subi(int opcode)
{
	if ((opcode & 0xFF00) != 0x400)
		return;

	add_opcode(gen_immediate("subi", opcode, subi_handler), opcode);
}

int addi_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV + OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_C_FLAG((uint%d_t)result < (uint%d_t)OP?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_X_FLAG((uint%d_t)result < (uint%d_t)OP?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_V_FLAG%d(EV, OP, result);\n", 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	return 0;
}

void addi(int opcode)
{
	if ((opcode & 0xFF00) != 0x600)
		return;

	add_opcode(gen_immediate("addi", opcode, addi_handler), opcode);
}

int eori_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV ^ OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void eori(int opcode)
{
	if ((opcode & 0xFF00) != 0xA00)
		return;

	add_opcode(gen_immediate("eori", opcode, eori_handler), opcode);
}

int gen_move(const char *mnemonic, int opcode)
{
	char func_name[MAX_NAME];
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];
	int func_id;
	int op_size;
	int op_dest;
	int sizes[4] = {3, 0, 2, 1};

	op_size = (opcode >> 12) & 3;
	op_size = sizes[op_size];

	op_dest = ((opcode>>9)&7)|((opcode >> 3)&0x38);
	if (!ea_alterable(op_dest)
	 || op_size == 3
	 || (ea_mode(opcode) == 1 && op_size == 0)
	 || ea_mode(opcode) < 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		int id;
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%02X", mnemonic, (op_size<<6)|op_dest);
		sprintf(access_name, "%s_read", func_name);
		id = func_by_name(access_name);
		if (id >= 0)
		{
			WAIT_BUS("_wait", "_read");
			return func_id;
		}
		READ_BUS("", "EA", "EV", op_size);
	}

	printf("\t{\n\t\tuint%d_t result = EV;\n", 8<<op_size);
	if (ea_mode(op_dest) != 1)
	{
		printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
		printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
		printf("\t\tSET_V_FLAG(0);\n");
		printf("\t\tSET_C_FLAG(0);\n");
	}
	if (ea_mode(op_dest) == 0)
	{
		printf("\t\tSET_DN_REG%d(%d, result);\n\t}\n", 8<<op_size, op_dest&0xF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else if (ea_mode(op_dest) == 1)
	{
		printf("\t\tREG_D(%d) = (uint32_t)(int%d_t)result;\n\t}\n", op_dest&0xF, 8<<op_size);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		sprintf(access_name, "%s_dest", func_name);
		printf("\t\tEV = result;\n\t}\n");
		if (get_ea(access_name, op_dest, op_size, 0) < 0)
			return -1;

		switch(op_size)
		{
			case 0: print_bus_wait("done_wait_wb", "done_write_wb"); break;
			case 1: print_bus_wait("done_wait_ww", "done_write_ww"); break;
			case 2: print_bus_wait("done_wait_wl", "done_write_wl"); break;
		}
	}
	return func_id;
}

void move(int opcode)
{
	if ((opcode & 0xC000) != 0)
		return;
	add_opcode(gen_move("move", opcode), opcode);
}

int gen_move_from_sr(const char *mnemonic, int opcode)
{
	char func_name[MAX_NAME];
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];
	int func_id;
	int op_size;

	op_size = 1;

	if (!ea_alterable(opcode)
	 || ea_mode(opcode) == 1)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	if (ea_address(opcode))
	{
		if (get_ea(func_name, opcode, op_size, 1) < 0)
			return -1;
	}

	if (ea_address(opcode))
	{
		int id;
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_fsr_common", mnemonic);
		sprintf(access_name, "%s_read", func_name);
		id = func_by_name(access_name);
		if (id >= 0)
		{
			WAIT_BUS("_wait", "_read");
			return func_id;
		}
		READ_BUS("", "EA", "EV", op_size);
	}

	if (ea_mode(opcode)<2)
	{
		printf("\tSET_DN_REG%d(%d, SR);\n", 8<<op_size, opcode&0xF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		printf("\tEV = SR;\n");

		switch(op_size)
		{
			case 0: print_bus_wait("done_wait_wb", "done_write_wb"); break;
			case 1: print_bus_wait("done_wait_ww", "done_write_ww"); break;
			case 2: print_bus_wait("done_wait_wl", "done_write_wl"); break;
		}
	}
	return func_id;
}

void move_fsr(int opcode)
{
	if ((opcode & 0xFFC0) != 0x40C0)
		return;
	add_opcode(gen_move_from_sr("move", opcode), opcode);
}

int gen_move_to_ccr(const char *mnemonic, int opcode)
{
	char func_name[MAX_NAME];
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];
	int func_id;
	int op_size;
	int op_dest;
	int sr;

	op_size = 1;
	sr = opcode & 0x200;

	if (ea_mode(opcode) == 1
	 || ea_mode(opcode) < 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	if (sr)
		printf("if (SUPERVISOR) PRIVILEGE_EXCEPTION;\n");

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		int id;
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		if (sr)
			sprintf(func_name, "%s_sr_common", mnemonic);
		else
			sprintf(func_name, "%s_tcr_common", mnemonic);
		sprintf(access_name, "%s_read", func_name);
		id = func_by_name(access_name);
		if (id >= 0)
		{
			WAIT_BUS("_wait", "_read");
			return func_id;
		}
		READ_BUS("", "EA", "EV", op_size);
	}

	if (sr)
		printf("\tSR = EV;\n");
	else
		printf("\tSET_DN_REG8(M68K_REG_SR, EV);\n");
	printf("\tSR &= M68K_FLAG_ALL;\n");
	printf("\tFETCH_OPCODE;\n}\n\n");
	return func_id;
}

void move_tcr(int opcode)
{
	if ((opcode & 0xFDC0) != 0x44C0)
		return;
	add_opcode(gen_move_to_ccr("move", opcode), opcode);
}

int main(void)
{
	int i;
	FILE *f;

	hash_init();

	declare_function("invalid");
	declare_function("done_wait_wb");
	declare_function("done_wait_ww");
	declare_function("done_wait_wl");
	declare_function("done_wait_wl2");
	declare_function("done_write_wb");
	declare_function("done_write_ww");
	declare_function("done_write_wl");
	declare_function("done_write_wl2");

	declare_function("opcode_wait");
	declare_function("opcode_read");

	printf("#include \"m68k_opcode.h\"\n");
	printf("#include \"m68k_optable.h\"\n\n");

	for (i=0; i<0x10000; ++i)
	{
		valid[i] = invalid();
		ori(i);
		andi(i);
		subi(i);
		addi(i);
		eori(i);
		move(i);
		move_fsr(i);
		move_tcr(i);
	}

	f = fopen("m68k_optable.h","wb");
	for (i=0; i<func_count; ++i)
		fprintf(f, "M68K_FUNCTION(%s);\n", func_names[i]);
	fprintf(f, "\nextern m68k_function m68k_opcode_table[0x10000];\n");
	fclose(f);

	f = fopen("m68k_optable.c","wb");
	fprintf(f, "#include \"m68k_opcode.h\"\n\nm68k_function m68k_opcode_table[0x10000] = {\n");
	for (i=0; i<0x10000; ++i)
	{
		int id = valid[i];
		fprintf(f, "%s,\n", func_names[id]);
	}
	fprintf(f, "};\n");
	fclose(f);
}
