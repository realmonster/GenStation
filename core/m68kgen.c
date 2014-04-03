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

int ea_alterable_array[] = {1,1,1,1,1,1,1,1,1,1,1,1};

int ea_alterable(int opcode)
{
	int mode = ea_mode(opcode);
	if (mode < 0)
		return 0;
	return ea_alterable_array[mode];
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
	char address[100];
	char func_name[100];
	char wait_name[100];
	char access_name[100];
	int func_id;

	if (!ea_alterable(opcode)
	 || (opcode & 0xC0) == 0xC0)
	 //|| ea_mode(opcode) == 1)
		return invalid();

	sprintf(func_name, "%s_%04X", mnemonic, opcode);

	func_id = declare_function(func_name);

	printf("M68K_FUNCTION(%s)\n{\n", func_name);

	WAIT_BUS("", "_read");

	if ((opcode & 0xC0) == 0x80)
		tmp = "<<16";
	else
		tmp = "";

	printf("\tm68k->operand = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])%s;\n", tmp);
	printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

	if ((opcode & 0xC0) == 0x80)
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
			address[0] = 0;
			break;

		case 1: // An
			printf("\tm68k->effective_value = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			address[0] = 0;
			break;

		case 2: // (An)
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
			break;

		case 3: // (An)+
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
			break;

		case 4: // -(An)
			sprintf(wait_name, "%s_wait_pre", func_name);
			printf("\tTIMEOUT(2, %s);\n}\n\n", wait_name);

			declare_function(wait_name);
			printf("M68K_FUNCTION(%s)\n{\n",wait_name);
			printf("\tm68k->reg[M68K_REG_A%d] -= ");
			switch (opcode & 0xC0)
			{
				case 0x00: printf("1;\n"); break;
				case 0x40: printf("2;\n"); break;
				case 0x80: printf("4;\n"); break;
			}
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
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

			sprintf(address, "m68k->effective_address");
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

			sprintf(address, "m68k->effective_address");
			break;

		case 7: // (xxx).W
			WAIT_BUS("_wait_w", "_read_w");

			printf("\tm68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			sprintf(address, "m68k->effective_address");
			break;

		case 8: // (xxx).L
			WAIT_BUS("_wait_l", "_read_l");

			printf("\tm68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;\n");
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			WAIT_BUS("_wait_l2", "_read_l2");

			printf("\tm68k->effective_address |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");

			sprintf(address, "m68k->effective_address");
			break;

		case 11: // #<data>
			WAIT_BUS("_wait_imm", "_read_imm");

			switch(opcode & 0xC0)
			{
				case 0x00:
					printf("\tm68k->effective_value = (int8_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;

				case 0x40:
					printf("\tm68k->effective_value = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;

				case 0x80:
					printf("\tm68k->effective_value = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;\n");
					printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

					WAIT_BUS("_wait_imm2", "_read_imm2");

					printf("\tm68k->effective_value |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n");
					break;
			}
			printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

			address[0] = 0;
			break;
	}

	if (address[0])
	{
		if (opcode & 0xC0)
		{
			printf(
"	if (%s&1)\n"
"		ADDRESS_EXCEPTION;\n",address);
		}
		WAIT_BUS("_wait_ea", "_read_ea");

		printf("\tm68k->effective_value = m68k->read_w(m68k, %s)%s;\n", address, tmp);

		if ((opcode & 0xC0) == 0x80)
		{
			WAIT_BUS("_wait_ea2", "_read_ea2");

			printf("\tm68k->operand |= (uint16_t)m68k->read_w(m68k, %s + 2);\n", address);
		}
	}
	if ((opcode & 0x38) == 0x18)
	{
		printf("\t%s += ", address);
		switch(opcode & 0xC0)
		{
			case 0x00: printf("1;\n"); break;
			case 0x40: printf("2;\n"); break;
			case 0x80: printf("4;\n"); break;
		}
	}
	printf("\tFETCH_OPCODE;\n}\n\n");
	return func_id;
}

int valid[0x10000];

int main(void)
{
	int i;
	FILE *f;

	declare_function("invalid");

	for (i=0; i<0x10000; ++i)
		valid[i] = invalid();

	printf("#include \"ori.h\"\n");
	printf("#include \"ori_f.h\"\n\n");
	for (i=0; i<0x100; ++i)
	{
		valid[i] = gen_immediate("ori", i);
		if (valid[i] < 0)
			printf("Error at ori_%04X", i);
	}

	f = fopen("ori_f.h","wb");
	for (i=0; i<func_count; ++i)
		fprintf(f, "M68K_FUNCTION(%s);\n", func_names[i]);
	fclose(f);
	
	//f = fopen("ori_ops.h","wb");
	printf("#include \"core/m68k.h\"\nm68k_function m68k_opcode_table[0x10000] = {\n");
	for (i=0; i<0x10000; ++i)
	{
		int id = valid[i];
		printf("%s,\n", func_names[id]);
	}
	printf("};\n");
	fclose(f);
}
