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
#include <string.h>

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

int ea_valid_na(int opcode)
{
	return (ea_mode(opcode) != 1 && ea_valid(opcode));
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

int begin_function(const char* name)
{
	int func_id = declare_function(name);
	if (func_id >= 0)
		printf("M68K_FUNCTION(%s)\n{\n", name);
	return func_id;
}

void end_function()
{
	printf("}\n\n");
}

void strconcat(char *buffer, const char *a, const char *b, size_t max_len)
{
	int la, lb;
	la = strlen(a);
	lb = strlen(b);
	if (la + lb >= max_len)
		fprintf(stderr, "Error: max length overflow at concat: %s%s\n", a, b);
	strcpy(buffer, a);
	strcpy(buffer+la, b);
}

int print_bus_wait(const char* bus_wait, const char* bus_access)
{
	int bw = declare_function(bus_wait);

	printf("\tWAIT_BUS(%s, %s);\n}\n\n", bus_wait, bus_access);
	if (bw >= 0)
		printf("M68K_FUNCTION(%s) { WAIT_BUS(%s, %s); }\n\n", bus_wait, bus_wait, bus_access);

	return begin_function(bus_access);
}

int print_bus_wait_(const char *func, const char *wait, const char *access)
{
	char wait_name[MAX_NAME];
	char access_name[MAX_NAME];
	strconcat(wait_name, func, wait, MAX_NAME);
	strconcat(access_name, func, access, MAX_NAME);
	return print_bus_wait(wait_name, access_name);
}

#define WAIT_BUS(bus_wait_str, bus_access_str) \
print_bus_wait_(func_name, bus_wait_str, bus_access_str)

int print_bus_read(const char* prefix, const char* address, const char* lval, int size)
{
	const char *func_name;
	int r;

	func_name = prefix;
	r = WAIT_BUS("_wait", "_read");
	if (r < 0)
		return r;

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

			if (WAIT_BUS("_wait2", "_read2") < 0)
				fprintf(stderr, "Error: %s_read2 already exists\n", prefix);

			printf("\t%s |= READ_16(%s + 2);\n", lval, address);
			break;

		default:
			fprintf(stderr, "Error: invalid bus read size %d in %s\n", size, prefix);
			break;
	}
	return r;
}

int print_bus_read_(const char *func, const char *str, const char *address, const char* lval, int size)
{
	char prefix[MAX_NAME];
	strconcat(prefix, func, str, MAX_NAME);
	return print_bus_read(prefix, address, lval, size);
}

#define READ_BUS(str, address, lval, size) \
print_bus_read_(func_name, str, address, lval, size)

int print_bus_write(const char* prefix, const char* address, const char* val, int size)
{
	const char *func_name;
	int r;

	func_name = prefix;
	r = WAIT_BUS("_wait", "_write");
	if (r < 0)
		return r;

	switch (size)
	{
		case 0:
			printf("\tWRITE_8(%s, %s);\n", address, val);
			break;

		case 1:
			printf("\tWRITE_16(%s, %s);\n", address, val);
			break;

		case 2:
			printf("\tWRITE_16(%s, (%s)>>16);\n", address, val);

			if (WAIT_BUS("_wait2", "_write2") < 0)
				fprintf(stderr, "Error: %s_read2 already exists\n", prefix);

			printf("\tWRITE_16(%s + 2, %s);\n", address, val);
			break;

		default:
			fprintf(stderr, "Error: invalid bus write size %d in %s\n", size, prefix);
			break;
	}
	return r;
}

int print_bus_write_(const char *func, const char *str, const char *address, const char* val, int size)
{
	char prefix[MAX_NAME];
	strconcat(prefix, func, str, MAX_NAME);
	return print_bus_write(prefix, address, val, size);
}

#define WRITE_BUS(str, address, val, size) \
print_bus_write_(func_name, str, address, val, size)

int print_bus_fetch(const char* prefix, const char* lval, int size)
{
	const char *func_name;
	int r;

	func_name = prefix;
	r = WAIT_BUS("_wait", "_read");
	if (r < 0)
		return r;

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

			if (WAIT_BUS("_wait2", "_read2") < 0)
				fprintf(stderr, "Error: %s_read2 already exists\n", prefix);

			printf("\t%s |= READ_16(PC);\n", lval);
			break;

		default:
			fprintf(stderr, "Error: invalid bus fetch size %d in %s\n", size, prefix);
			break;
	}
	printf("\tPC += 2;\n");
	return r;
}

int print_bus_fetch_(const char *func, const char *str, const char* val, int size)
{
	char prefix[MAX_NAME];
	strconcat(prefix, func, str, MAX_NAME);
	return print_bus_fetch(prefix, val, size);
}

#define FETCH_BUS(str, lval, size) \
print_bus_fetch_(func_name, str, lval, size)

void opcode_read()
{
	char* func_name = "opcode";

	if (FETCH_BUS("", "m68k->opcode", 1) < 0)
		return;

	printf("\tm68k_opcode_table[m68k->opcode](m68k);\n}\n\n");
}

void reset_exception()
{
	const char* func_name = "reset_exception";

	begin_function(func_name);

	printf("\tif (!SUPERVISOR) USP = SP;\n");

	READ_BUS("", "0", "REG_A(7)", 2);

	READ_BUS("_pc", "4", "PC", 2);

	printf("\tSR = M68K_FLAG_S_MASK | M68K_FLAG_I0_MASK | M68K_FLAG_I1_MASK | M68K_FLAG_I2_MASK;\n");

	printf("\tif (PC&1) ADDRESS_EXCEPTION;\n");
	opcode_read();
}

void address_exception()
{
	const char* func_name = "address_exception";

	begin_function(func_name);

	printf("\tif (!SUPERVISOR) {USP = SP; SP = SSP;}\n");

	printf("\tif (SP&1) HALT;\n");

	WRITE_BUS("_pcl", "SP-2", "PC", 1);
	WRITE_BUS("_sr" , "SP-6", "SR", 1);
	WRITE_BUS("_pch", "SP-4", "PC>>16", 1);
	WRITE_BUS("_op" , "SP-8", "m68k->opcode", 1);
	WRITE_BUS("_eal", "SP-10", "EA", 1);
	WRITE_BUS("_fc" , "SP-14", "0", 1);
	WRITE_BUS("_eah", "SP-12", "EA>>16", 1);

	READ_BUS("_vec", "12", "PC", 2);

	printf("\tif (PC&1) HALT;\n");
	printf("\tSR = (SR | M68K_FLAG_S_MASK) & (~M68K_FLAG_T1_MASK);\n");
	printf("\tSP -= 14;\n");

	opcode_read();
}

void interrupt()
{
	const char* func_name = "interrupt";

	begin_function(func_name);

	printf("\tif (!SUPERVISOR) {USP = SP; SP = SSP;}\n");

	printf("\tif (SP&1) HALT;\n");

	WRITE_BUS("_pcl", "SP-2", "PC", 1);
	WRITE_BUS("_sr" , "SP-6", "SR", 1);
	WRITE_BUS("_pch", "SP-4", "PC>>16", 1);

	READ_BUS("_vec", "OP*4", "PC", 2);

	printf("\tSR = (SR | M68K_FLAG_S_MASK) & (~M68K_FLAG_T1_MASK);\n");
	printf("\tSP -= 6;\n");
	printf("\tif (PC&1) ADDRESS_EXCEPTION;\n");

	opcode_read();
}

int invalid(void)
{
	return func_by_name("invalid");
}

int get_ea(const char *func_name, int opcode, int op_size, int read)
{
	char wait_name[MAX_NAME];

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
			if (op_size == 0 && (opcode&7) == 7) // sp
				printf("\tREG_A(7) += 2;\n");
			else
				printf("\tREG_A(%d) += %d;\n", opcode&7, 1<<op_size);
			break;

		case 4: // -(An)
			if (read) // timing fix
			{
				sprintf(wait_name, "%s_wait_pre", func_name);
				printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

				begin_function(wait_name);
			}

			if (op_size == 0 && (opcode&7) == 7) // sp
				printf("\tREG_A(7) -= 2;\n");
			else
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
			printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

			begin_function(wait_name);
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
	int func_id;
	int op_size;

	op_size = (opcode >> 6) & 3;
	if (handler(check(opcode)) < 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = begin_function(func_name);

	if (FETCH_BUS("", "OP", op_size) < 0)
		return -1;

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%d", mnemonic, op_size);

		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
	}

	if (handler(opcode) < 0)
		return -1;
	if ((opcode & 0xFF00) == 0xC00) // cmpi
	{
		printf("\t}\n\tFETCH_OPCODE;\n}\n\n");
		return func_id;
	}
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

int cmpi_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV - OP);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG%d(EV, -OP, result);\n", 8<<op_size);
	printf("\t\tSET_C_FLAG((uint%d_t)EV < (uint%d_t)OP?1:0);\n", 8<<op_size, 8<<op_size);
	return 0;
}

void cmpi(int opcode)
{
	if ((opcode & 0xFF00) != 0xC00)
		return;

	add_opcode(gen_immediate("cmpi", opcode, cmpi_handler), opcode);
}

int gen_bit(const char *mnemonic, int opcode, opcode_handler handler)
{
	char func_name[MAX_NAME];
	int func_id;
	int type;
	int dn;

	if (handler(check(opcode)) < 0)
		return invalid();

	type = (opcode&0x100);
	dn = (opcode>>9)&7;

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = begin_function(func_name);

	if (type)
		printf("\tOP = REG_D(%d);\n", dn);
	else
		FETCH_BUS("", "OP", 0);

	if (get_ea(func_name, opcode, 0, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		sprintf(func_name, "%s_common", mnemonic);

		if (READ_BUS("", "EA", "EV", 0) < 0)
			return func_id;
	}

	if (handler(opcode) < 0)
		return -1;
	if ((opcode & 0xF0C0) == 0) // btst
	{
		printf("\t}\n\tFETCH_OPCODE;\n}\n\n");
		return func_id;
	}
	if (ea_mode(opcode) < 2)
	{
		printf("\t\tSET_DN_REG32(%d, result);\n\t}\n", opcode&0xF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		printf("\t\tEV = result;\n\t}\n");
		print_bus_wait("done_wait_wb", "done_write_wb");
	}
	return func_id;
}

int btst_handler(int opcode)
{
	if (!ea_valid_na(opcode))
		return -1;

	if ((opcode&0x100) == 0 && ((opcode>>9)&7) != 4)
		return -1;

	if (is_checking(opcode))
		return 0;

	if (ea_address(opcode))
		printf("\t{\n\t\tuint8_t result = ((~EV)>>(OP&7))&1;\n");
	else
		printf("\t{\n\t\tuint32_t result = ((~EV)>>(OP&31))&1;\n");

	printf("\t\tSET_Z_FLAG(result);\n");
	return 0;
}

void btst(int opcode)
{
	if ((opcode & 0xF0C0) != 0)
		return;

	add_opcode(gen_bit("btst", opcode, btst_handler), opcode);
}

int bchg_handler(int opcode)
{
	if (!ea_alterable_na(opcode))
		return -1;

	if ((opcode&0x100) == 0 && ((opcode>>9)&7) != 4)
		return -1;

	if (is_checking(opcode))
		return 0;

	if (ea_address(opcode))
	{
		printf("\t{\n\t\tuint8_t result = EV^(1<<(OP&7));\n");
		printf("\t\tSET_Z_FLAG((result>>(OP&7))&1);\n");
	}
	else
	{
		printf("\t{\n\t\tuint32_t result = EV^(1<<(OP&31));\n");
		printf("\t\tSET_Z_FLAG((result>>(OP&31))&1);\n");
	}
	return 0;
}

void bchg(int opcode)
{
	if ((opcode & 0xF0C0) != 0x40)
		return;

	add_opcode(gen_bit("bchg", opcode, bchg_handler), opcode);
}

int bclr_handler(int opcode)
{
	if (!ea_alterable_na(opcode))
		return -1;

	if ((opcode&0x100) == 0 && ((opcode>>9)&7) != 4)
		return -1;

	if (is_checking(opcode))
		return 0;

	if (ea_address(opcode))
	{
		printf("\t{\n\t\tuint8_t result = EV&(~(1<<(OP&7)));\n");
		printf("\t\tSET_Z_FLAG(((~EV)>>(OP&7))&1);\n");
	}
	else
	{
		printf("\t{\n\t\tuint32_t result = EV&(~(1<<(OP&31)));\n");
		printf("\t\tSET_Z_FLAG(((~EV)>>(OP&31))&1);\n");
	}
	return 0;
}

void bclr(int opcode)
{
	if ((opcode & 0xF0C0) != 0x80)
		return;

	add_opcode(gen_bit("bclr", opcode, bclr_handler), opcode);
}

int bset_handler(int opcode)
{
	if (!ea_alterable_na(opcode))
		return -1;

	if ((opcode&0x100) == 0 && ((opcode>>9)&7) != 4)
		return -1;

	if (is_checking(opcode))
		return 0;

	if (ea_address(opcode))
	{
		printf("\t{\n\t\tuint8_t result = EV|(1<<(OP&7));\n");
		printf("\t\tSET_Z_FLAG(((~EV)>>(OP&7))&1);\n");
	}
	else
	{
		printf("\t{\n\t\tuint32_t result = EV|(1<<(OP&31));\n");
		printf("\t\tSET_Z_FLAG(((~EV)>>(OP&31))&1);\n");
	}
	return 0;
}

void bset(int opcode)
{
	if ((opcode & 0xF0C0) != 0xC0)
		return;

	add_opcode(gen_bit("bset", opcode, bset_handler), opcode);
}

int gen_unary(const char *mnemonic, int opcode, opcode_handler handler)
{
	char func_name[MAX_NAME];
	int func_id;
	int op_size;

	op_size = (opcode >> 6) & 3;
	if (handler(check(opcode)) < 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = begin_function(func_name);

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%d", mnemonic, op_size);

		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
	}

	if (handler(opcode) < 0)
		return -1;
	if ((opcode & 0xFF00) == 0x4A00) // tst
	{
		printf("\t}\n\tFETCH_OPCODE;\n}\n\n");
		return func_id;
	}
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

int negx_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(0 - EV - GET_X_FLAG());\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_X_FLAG(((uint%d_t)EV != 0 || GET_X_FLAG() != 0)?1:0);\n", 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tif (result) {SET_Z_FLAG(0);}\n");
	printf("\t\tSET_V_FLAG((uint%d_t)result == (1<<(%d-1))?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_C_FLAG(((uint%d_t)EV != 0 || GET_X_FLAG() != 0)?1:0);\n", 8<<op_size);
	return 0;
}

void negx(int opcode)
{
	if ((opcode & 0xFF00) != 0x4000)
		return;

	add_opcode(gen_unary("negx", opcode, negx_handler), opcode);
}

int clr_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = 0;\n", 8<<op_size);
	printf("\t\tSET_N_FLAG(0);\n");
	printf("\t\tSET_Z_FLAG(1);\n");
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void clr(int opcode)
{
	if ((opcode & 0xFF00) != 0x4200)
		return;

	add_opcode(gen_unary("clr", opcode, clr_handler), opcode);
}

int neg_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(- EV);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_X_FLAG(((uint%d_t)EV != 0)?1:0);\n", 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG((uint%d_t)result == (1<<(%d-1))?1:0);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_C_FLAG(((uint%d_t)EV != 0)?1:0);\n", 8<<op_size);
	return 0;
}

void neg(int opcode)
{
	if ((opcode & 0xFF00) != 0x4400)
		return;

	add_opcode(gen_unary("neg", opcode, neg_handler), opcode);
}

int not_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)(~EV);\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void not(int opcode)
{
	if ((opcode & 0xFF00) != 0x4600)
		return;

	add_opcode(gen_unary("not", opcode, not_handler), opcode);
}

void swap(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if ((opcode & 0xFFF8) != 0x4840)
		return;

	sprintf(func_name, "swap_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tuint32_t result = (((uint32_t)REG_D(%d))>>16)|(((uint32_t)REG_D(%d))<<16);\n", opcode&7, opcode&7);
	printf("\tSET_N_FLAG32(result);\n");
	printf("\tSET_Z_FLAG32(result);\n");
	printf("\tSET_V_FLAG(0);\n");
	printf("\tSET_C_FLAG(0);\n");
	printf("\tSET_DN_REG32(%d, result);\n", opcode&7);
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

void ext(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;
	int op_size;

	op_size = ((opcode>>6)&1)+1;
	if ((opcode & 0xFFB8) != 0x4880)
		return;

	sprintf(func_name, "ext_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tuint%d_t result = (int%d_t)REG_D(%d);\n", 8<<op_size, 8<<(op_size-1), opcode&7);
	printf("\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\tSET_V_FLAG(0);\n");
	printf("\tSET_C_FLAG(0);\n");
	printf("\tSET_DN_REG%d(%d, result);\n", 8<<op_size, opcode&7);
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

int tst_handler(int opcode)
{
	int op_size = (opcode >> 6) & 3;

	if (!ea_alterable_na(opcode)
	 || op_size == 3)
		return -1;

	if (is_checking(opcode))
		return 0;

	printf("\t{\n\t\tuint%d_t result = (uint%d_t)EV;\n", 8<<op_size, 8<<op_size);
	printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	return 0;
}

void tst(int opcode)
{
	if ((opcode & 0xFF00) != 0x4A00)
		return;

	add_opcode(gen_unary("tst", opcode, tst_handler), opcode);
}

void nop(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if (opcode != 0x4E71)
		return;

	sprintf(func_name, "nop_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

void stop(int opcode)
{
	char func_name[MAX_NAME];
	char wait_name[MAX_NAME];
	int func_id;

	if (opcode != 0x4E72)
		return;

	sprintf(func_name, "stop_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tif (!SUPERVISOR) PRIVILEGE_EXCEPTION;\n");

	FETCH_BUS("", "OP", 1);

	sprintf(wait_name, "%s_inf", func_name);
	printf("\tSR = OP & M68K_FLAG_ALL;\n");
	printf("\tTIMEOUT(1<<15, %s);\n}\n\n", wait_name);

	begin_function(wait_name);

	printf("\tTIMEOUT(1<<15, %s);\n}\n\n", wait_name);

	add_opcode(func_id, opcode);
}

void rte(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if (opcode != 0x4E73)
		return;

	sprintf(func_name, "rte_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tif (!SUPERVISOR) PRIVILEGE_EXCEPTION;\n");

	READ_BUS("", "REG_A(7)", "OP", 1);

	READ_BUS("_pc", "REG_A(7)+2", "PC", 2);

	printf("\tREG_A(7) += 6;\n");

	printf("\tSAVE_CURRENT_STACK;\n");
	printf("\tSR = OP & M68K_FLAG_ALL;\n");
	printf("\tGET_CURRENT_STACK;\n");

	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

void rts(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if (opcode != 0x4E75)
		return;

	sprintf(func_name, "rts_%04X", opcode);

	func_id = begin_function(func_name);

	READ_BUS("", "REG_A(7)", "PC", 2);

	printf("\tREG_A(7) += 4;\n");
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

void rtr(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if (opcode != 0x4E77)
		return;

	sprintf(func_name, "rtr_%04X", opcode);

	func_id = begin_function(func_name);

	READ_BUS("", "REG_A(7)", "OP", 1);

	printf("\tSET_DN_REG8(M68K_REG_SR, OP);\n");
	printf("\tSR &= M68K_FLAG_ALL;\n");
	printf("\tREG_A(7) += 2;\n");

	READ_BUS("_pc", "REG_A(7)", "PC", 2);

	printf("\tREG_A(7) += 4;\n");
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

int gen_quick(const char *mnemonic, int opcode, opcode_handler handler)
{
	char func_name[MAX_NAME];
	int func_id;
	int op_size;

	op_size = (opcode >> 6) & 3;
	if (!ea_alterable(opcode)
	 || op_size == 3)
		return invalid();

	if (ea_mode(opcode) == 1
	 && op_size == 0)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = begin_function(func_name);

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%02X", mnemonic, (op_size<<3)|((opcode>>9)&7));

		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
	}

	if (handler(opcode) < 0)
		return -1;
	if (ea_mode(opcode) < 2)
	{
		printf("\t\tSET_DN_REG%d(%d, result);\n\t}\n", 8<<(ea_mode(opcode)==1?2:op_size), opcode&0xF);
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

int addq_handler(int opcode)
{
	int value;
	int op_size = (opcode >> 6) & 3;

	value = (opcode>>9)&7;
	if (!value)
		value = 8;

	if (ea_mode(opcode) == 1)
		printf("\t{\n\t\tuint32_t result = (uint32_t)(EV + %d);\n", value);
	else
	{
		printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV + %d);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_C_FLAG((uint%d_t)result < (uint%d_t)%d?1:0);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_X_FLAG((uint%d_t)result < (uint%d_t)%d?1:0);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_V_FLAG%d(EV, %d, result);\n", 8<<op_size, value);
		printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
		printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	}
	return 0;
}

void addq(int opcode)
{
	if ((opcode & 0xF100) != 0x5000)
		return;

	add_opcode(gen_quick("addq", opcode, addq_handler), opcode);
}

int subq_handler(int opcode)
{
	int value;
	int op_size = (opcode >> 6) & 3;

	value = (opcode>>9)&7;
	if (!value)
		value = 8;

	if (ea_mode(opcode) == 1)
		printf("\t{\n\t\tuint32_t result = (uint32_t)(EV - %d);\n", value);
	else
	{
		printf("\t{\n\t\tuint%d_t result = (uint%d_t)(EV - %d);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_C_FLAG((uint%d_t)EV < (uint%d_t)%d?1:0);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_X_FLAG((uint%d_t)EV < (uint%d_t)%d?1:0);\n", 8<<op_size, 8<<op_size, value);
		printf("\t\tSET_V_FLAG%d(EV, -%d, result);\n", 8<<op_size, value);
		printf("\t\tSET_N_FLAG%d(result);\n", 8<<op_size);
		printf("\t\tSET_Z_FLAG%d(result);\n", 8<<op_size);
	}
	return 0;
}

void subq(int opcode)
{
	if ((opcode & 0xF100) != 0x5100)
		return;

	add_opcode(gen_quick("subq", opcode, subq_handler), opcode);
}

char* cc_names[] = {"t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"};
char* cc_up(int code)
{
	static char cc[3];
	int i;

	if (code > 0xF)
		return 0;
	for (i=0; cc_names[code][i]; ++i)
		cc[i] = cc_names[code][i]-'a'+'A';
	return cc;
}

void scc(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;
	int cc;
	char* cc_str;

	if ((opcode & 0xF0C0) != 0x50C0)
		return;

	if (!ea_alterable_na(opcode))
		return;

	cc = (opcode>>8)&0xF;

	sprintf(func_name, "s%s_%04X", cc_names[cc], opcode);

	func_id = begin_function(func_name);

	if (get_ea(func_name, opcode, 0, 1) < 0)
	{
		add_opcode(-1, opcode);
		return;
	}

	if (ea_address(opcode))
	{
		sprintf(func_name, "scc_common_%d", cc);

		if (READ_BUS("", "EA", "EV", 0) < 0)
		{
			add_opcode(func_id, opcode);
			return;
		}
	}

	printf("\tif (CONDITION_%s)\n", cc_up(cc));
	if (ea_mode(opcode) < 2)
	{
		printf("\t\tSET_DN_REG8(%d, 0xFF);\n", opcode&0xF);
		printf("\telse\n", opcode&0xF);
		printf("\t\tSET_DN_REG8(%d, 0);\n", opcode&0xF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		printf("\t\tEV = 0xFF;\n");
		printf("\telse\n");
		printf("\t\tEV = 0;\n");

		print_bus_wait("done_wait_wb", "done_write_wb");
	}

	add_opcode(func_id, opcode);
}

void dbcc(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;
	int cc;
	char* cc_str;

	if ((opcode & 0xF0F8) != 0x50C8)
		return;

	cc = (opcode>>8)&0xF;

	sprintf(func_name, "db%s_%04X", cc_names[cc], opcode);

	func_id = begin_function(func_name);

	FETCH_BUS("", "OP", 1);

	printf("\tif (!(CONDITION_%s))\n\t{\n", cc_up(cc));
	printf("\t\tSET_DN_REG16(%d, REG_D(%d)-1);\n", opcode&7, opcode&7);
	printf("\t\tif ((int16_t)(REG_D(%d)) != -1)\n", opcode&7);
	printf("\t\t\tPC += (int16_t)OP - 2;\n\t}\n");
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

void bcc(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;
	int cc;
	char* cc_str;

	if ((opcode & 0xF000) != 0x6000)
		return;

	cc = (opcode>>8)&0xF;
	if (cc == 0)
		cc_str = "ra";
	else if (cc == 1)
		cc_str = "sr";
	else
		cc_str = cc_names[cc];

	sprintf(func_name, "b%s_%04X", cc_str, opcode);

	func_id = begin_function(func_name);

	if ((opcode & 0xFF) == 0)
	{
		FETCH_BUS("", "OP", 1);
		if (cc > 1)
			printf("\tif (CONDITION_%c%c)\n", cc_names[cc][0]-'a'+'A', cc_names[cc][1]-'a'+'A');
		if (cc == 1) // bsr
		{
			printf("\tREG_A(7) -= 4;\n");
			WRITE_BUS("_pc", "REG_A(7)", "PC", 2);
		}
		printf("\t\tPC += (int16_t)OP - 2;\n");
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		if (cc > 1)
			printf("\tif (CONDITION_%c%c)\n", cc_names[cc][0]-'a'+'A', cc_names[cc][1]-'a'+'A');
		if (cc == 1) // bsr
		{
			printf("\tREG_A(7) -= 4;\n");
			WRITE_BUS("_pc", "REG_A(7)", "PC", 2);
		}
		printf("\t\tPC += (int8_t)%d;\n", opcode&0xFF);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	add_opcode(func_id, opcode);
}

void moveq(int opcode)
{
	char func_name[MAX_NAME];
	int func_id;

	if ((opcode & 0xF100) != 0x7000)
		return;

	sprintf(func_name, "moveq_%04X", opcode);

	func_id = begin_function(func_name);

	printf("\tREG_D(%d) = (uint32_t)(int8_t)0x%X;\n", (opcode >> 9)&7, opcode&0xFF);
	printf("\tSET_N_FLAG(%d);\n", (opcode&0x80)?1:0);
	printf("\tSET_Z_FLAG(%d);\n", (opcode&0xFF)?0:1);
	printf("\tSET_V_FLAG(0);\n");
	printf("\tSET_C_FLAG(0);\n");
	printf("\tFETCH_OPCODE;\n}\n\n");

	add_opcode(func_id, opcode);
}

int gen_move(const char *mnemonic, int opcode)
{
	char func_name[MAX_NAME];
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

	func_id = begin_function(func_name);

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_common_%02X", mnemonic, (op_size<<6)|op_dest);
		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
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
	int func_id;
	int op_size;

	op_size = 1;

	if (!ea_alterable(opcode)
	 || ea_mode(opcode) == 1)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = begin_function(func_name);

	if (ea_address(opcode))
	{
		if (get_ea(func_name, opcode, op_size, 1) < 0)
			return -1;
	}

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		sprintf(func_name, "%s_fsr_common", mnemonic);
		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
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

	func_id = begin_function(func_name);

	if (sr)
		printf("\tif (!SUPERVISOR) PRIVILEGE_EXCEPTION;\n");

	if (get_ea(func_name, opcode, op_size, 1) < 0)
		return -1;

	if (ea_address(opcode))
	{
		if (op_size)
			printf("\tif (EA&1) ADDRESS_EXCEPTION;\n");

		if (sr)
			sprintf(func_name, "%s_sr_common", mnemonic);
		else
			sprintf(func_name, "%s_tcr_common", mnemonic);
		if (READ_BUS("", "EA", "EV", op_size) < 0)
			return func_id;
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

	printf("#include \"m68k_opcode.h\"\n");
	printf("#include \"m68k_optable.h\"\n\n");

	reset_exception();
	address_exception();
	interrupt();

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


	for (i=0; i<0x10000; ++i)
	{
		valid[i] = invalid();
		ori(i);
		andi(i);
		subi(i);
		addi(i);
		eori(i);
		cmpi(i);
		btst(i);
		bchg(i);
		bclr(i);
		bset(i);
		negx(i);
		clr(i);
		neg(i);
		not(i);
		swap(i);
		ext(i);
		tst(i);
		nop(i);
		stop(i);
		rte(i);
		rts(i);
		rtr(i);
		addq(i);
		subq(i);
		scc(i);
		dbcc(i);
		bcc(i);
		moveq(i);
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
