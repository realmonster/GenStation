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

#define GENERATE_FAILED -1

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

int ea_alterable_array[] = {1,1,1,1,1,1,1,1,1,0,0,0};

int ea_alterable(int opcode)
{
	int mode = ea_mode(opcode);
	if (mode < 0)
		return 0;
	return ea_alterable_array[mode];
}

int ea_address(int opcode)
{
	int mode = ea_mode(opcode);
	return (mode != 0) && (mode != 1) && (mode != 11);
}

char **func_names = 0;
int func_count = 0;

int func_by_name(const char* name)
{
	int i;
	for (i=0; i<func_count; ++i)
		if (!strcmp(name, func_names[i]))
			return i;
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

int invalid(void)
{
	return func_by_name("invalid");
}

int gen_immediate(const char *mnemonic, int opcode)
{
	char *tmp;
	char func_name[100];
	char wait_name[100];
	char access_name[100];
	int func_id;
	int op_size;

	op_size = (opcode >> 6) & 3;
	if (!ea_alterable(opcode)
	 || op_size == 3)
	 //|| ea_mode(opcode) == 1)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	WAIT_BUS("", "_read");

	if (op_size == 2)
		tmp = "<<16";
	else
		tmp = "";

	printf("\tm68k->operand = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])%s;\n", tmp);
	printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

	if (op_size == 2)
	{
		WAIT_BUS("_wait_2", "_read_2");

		printf("\tm68k->operand |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
		printf("\tm68k->reg[M68K_REG_PC] += 2;\n");
	}

	switch(ea_mode(opcode))
	{
		default:
			return -1;

		case 0: // Dn
			printf("\tm68k->effective_value = m68k->reg[M68K_REG_D%d];\n", opcode&7);
			break;

		case 1: // An
			printf("\tm68k->effective_value = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			break;

		case 2: // (An)
			printf("\tm68k->effective_address = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			break;

		case 3: // (An)+
			printf("\tm68k->effective_address = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			printf("\tm68k->reg[M68K_REG_A%d] += %d;\n", opcode&7, 1<<op_size);
			break;

		case 4: // -(An)
			sprintf(wait_name, "%s_wait_pre", func_name);
			printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

			declare_function(wait_name);
			printf("M68K_FUNCTION(%s)\n{\n",wait_name);

			printf("\tm68k->reg[M68K_REG_A%d] -= %d;\n", opcode&7, 1<<op_size);
			printf("\tm68k->effective_address = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			break;

		case 5: // (d16,An)
		case 9: // (d16,pc)
			WAIT_BUS("_wait_d16", "_read_d16");

			printf("\tm68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC])\n");
			if (ea_mode(opcode) == 5)
				printf("\t                        + m68k->reg[M68K_REG_A%d];\n", opcode&7);
			else
				printf("\t                        + m68k->reg[M68K_REG_PC];\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			break;

		case 6: // (d8,An,xn)
		case 10: // (d8,pc,xn)
			WAIT_BUS("_wait_d8", "_read_d8");

			printf("\tm68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");

			sprintf(wait_name, "%s_wait_d8_sum", func_name);
			declare_function(wait_name);

			printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

			printf("M68K_FUNCTION(%s)\n{\n", wait_name);
			printf(
"	if (m68k->effective_address & 0x800)\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_%c%c]\n"
"							+m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	else\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_%c%c]\n"
"							+(int16_t)m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	m68k->reg[M68K_REG_PC] += 2;\n"
	, ea_mode(opcode)==6?'A':'P', ea_mode(opcode)==6?'0'+(opcode&7):'C',
	  ea_mode(opcode)==6?'A':'P', ea_mode(opcode)==6?'0'+(opcode&7):'C');

			break;

		case 7: // (xxx).W
			WAIT_BUS("_wait_w", "_read_w");

			printf("\tm68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			break;

		case 8: // (xxx).L
			WAIT_BUS("_wait_l", "_read_l");

			printf("\tm68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			WAIT_BUS("_wait_l2", "_read_l2");

			printf("\tm68k->effective_address |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			break;

		case 11: // #<data>
			WAIT_BUS("_wait_imm", "_read_imm");

			switch(op_size)
			{
				case 0:
					printf("\tm68k->effective_value = (int8_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;

				case 1:
					printf("\tm68k->effective_value = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;

				case 2:
					printf("\tm68k->effective_value = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;\n");
					printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

					WAIT_BUS("_wait_imm2", "_read_imm2");

					printf("\tm68k->effective_value |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;
			}
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			break;
	}

	if (ea_address(opcode))
	{
		if (op_size)
		{
			printf(
"	if (m68k->effective_address&1)\n"
"		ADDRESS_EXCEPTION;\n");
		}
		WAIT_BUS("_wait_ea", "_read_ea");

		printf("\tm68k->effective_value = m68k->read_w(m68k, m68k->effective_address)%s;\n", tmp);

		if (op_size == 2)
		{
			WAIT_BUS("_wait_ea2", "_read_ea2");

			printf("\tm68k->effective_value |= (uint16_t)m68k->read_w(m68k, m68k->effective_address + 2);\n");
		}
	}

	printf("\t{\n\t\tuint%d_t result = m68k->effective_value | m68k->operand;\n", 8<<op_size);
	printf("\t\tSET_N_FLAG(result>>%d);\n", (8<<op_size)-1);
	printf("\t\tSET_Z_FLAG(result?0:1);\n");
	printf("\t\tSET_V_FLAG(0);\n");
	printf("\t\tSET_C_FLAG(0);\n");
	if (ea_mode(opcode) < 2)
	{
		printf("\t\tm68k->reg[M68K_REG_%c%d] = (m68k->reg[M68K_REG_%c%d]&(~((1<<%d)-1)))|(uint32_t)result;\n\t}\n", (ea_mode(opcode)?'A':'D'), opcode&7, (ea_mode(opcode)?'A':'D'), opcode&7, (8<<op_size)-1);
		printf("\tFETCH_OPCODE;\n}\n\n");
	}
	else
	{
		printf("\t\tm68k->effective_value = result;\n\t}\n");

		switch(op_size)
		{
			case 0: print_bus_wait("done_wait_wb", "done_write_wb"); break;
			case 1: print_bus_wait("done_wait_ww", "done_write_ww"); break;
			case 2: print_bus_wait("done_wait_wl", "done_write_wl"); break;
		}
	}
	return func_id;
}

int valid[0x10000];

int main(void)
{
	int i;
	FILE *f;

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
		valid[i] = invalid();

	printf("#include \"m68k_opcode.h\"\n");
	printf("#include \"m68k_optable.h\"\n\n");
	for (i=0; i<0x100; ++i)
	{
		valid[i] = gen_immediate("ori", i);
		if (valid[i] < 0)
			printf("Error at ori_%04X", i);
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
