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

FILE *f;

int invalid()
{
	return 0;
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

int ea_alterable_array[] = {1,1,1,1,1,1,1,1,1,1,1,1};

int ea_alterable(int opcode)
{
	int mode = ea_mode(opcode);
	if (mode < 0)
		return 0;
	return ea_alterable_array[mode];
}

int ori(int opcode)
{
	char *tmp;
	char address[100];

	if (!ea_alterable(opcode)
	 || (opcode & 0xC0) == 0xC0)
	 //|| ea_mode(opcode) == 1)
		return invalid();
	printf(
"M68K_FUNCTION(ori_%04X)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read);\n"
"}\n\n", opcode, opcode, opcode);

	fprintf(f, "M68K_FUNCTION(ori_%04X_read);\n", opcode);

	if ((opcode & 0xC0) == 0x80)
		tmp = "<<16";
	else
		tmp = "";

	printf(
"M68K_FUNCTION(ori_%04X_read)\n"
"{\n"
"	m68k->operand = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])%s;\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode, tmp);

	if ((opcode & 0xC0) == 0x80)
	{
		printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_2);\n"
"}\n\n", opcode, opcode);

		fprintf(f, "M68K_FUNCTION(ori_%04X_wait_2);\n", opcode);
		fprintf(f, "M68K_FUNCTION(ori_%04X_read_2);\n", opcode);

		printf(
"M68K_FUNCTION(ori_%04X_wait_2)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_2);\n"
"}\n\n", opcode, opcode, opcode);

		printf(
"M68K_FUNCTION(ori_%04X_read_2)\n"
"{\n"
"	m68k->operand |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode, opcode, opcode);
	}

	switch(opcode & 0x38)
	{
		case 0x00: // Dn
			printf("\tm68k->effective_value = m68k->reg[M68K_REG_D%d];\n", opcode&7);
			address[0] = 0;
			break;

		case 0x08: // An
			printf("\tm68k->effective_value = m68k->reg[M68K_REG_A%d];\n", opcode&7);
			address[0] = 0;
			break;

		case 0x10: // (An)
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
			break;

		case 0x18: // (An)+
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
			break;

		case 0x20: // -(An)
			printf("\tTIMEOUT(2, ori_%04X_wait_pre);\n}\n\n", opcode);

			fprintf(f, "M68K_FUNCTION(ori_%04X_wait_pre);\n", opcode);

			printf(
"M68K_FUNCTION(ori_%04X_wait_pre)\n"
"{\n"
"	m68k->reg[M68K_REG_A%d] -= ", opcode, opcode&7);
			switch (opcode & 0xC0)
			{
				case 0x00: printf("1;\n"); break;
				case 0x40: printf("2;\n"); break;
				case 0x80: printf("4;\n"); break;
			}
			sprintf(address, "m68k->reg[M68K_REG_A%d]", opcode&7);
			break;

		case 0x28: // (d16,An)
			printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d16);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d16);\n"
"}\n\n", opcode, opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d16);\n", opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_read_d16);\n", opcode);

			printf(
"M68K_FUNCTION(ori_%04X_wait_d16)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d16);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d16);\n"
"}\n\n", opcode, opcode, opcode);

			printf(
"M68K_FUNCTION(ori_%04X_read_d16)\n"
"{\n"
"	m68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode, opcode, opcode);

			sprintf(address, "m68k->effective_address");
			break;

		case 0x30: // (d8,An,xn)
			printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d8);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d8);\n"
"}\n\n", opcode, opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d8);\n", opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_read_d8);\n", opcode);

			printf(
"M68K_FUNCTION(ori_%04X_wait_d8)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d8);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d8);\n"
"}\n\n", opcode, opcode, opcode);

			printf(
"M68K_FUNCTION(ori_%04X_read_d8)\n"
"{\n"
"	m68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	TIMEOUT(2, ori_%04X_wait_d8_sum);\n"
"}\n\n", opcode, opcode);

			printf(
"M68K_FUNCTION(ori_%04X_wait_d8_sum)\n"
"{\n"

"	if (m68k->effective_address & 0x800)\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_A%d]\n"
"							+m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	else\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_A%d]\n"
"							+(int16_t)m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode, opcode&7, opcode&7);

			fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d8_sum);\n", opcode);

			sprintf(address, "m68k->effective_address");
			break;

		case 0x38:
			switch (opcode & 7)
			{
				case 0: // (xxx).W
					printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_w);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_w);\n"
"}\n\n", opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_w)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_w);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_w);\n"
"}\n\n", opcode, opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_w);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_w);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_w)\n"
"{\n"
"	m68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode);

					sprintf(address, "m68k->effective_address");
					break;

				case 1: // (xxx).L
					printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_l);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_l);\n"
"}\n\n", opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_l);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_l);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_l)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_l);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_l);\n"
"}\n\n", opcode, opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_l)\n"
"{\n"
"	m68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC])<<16;\n"
"	m68k->reg[M68K_REG_PC] += 2;\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_l_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_l_2);\n"
"}\n\n", opcode, opcode, opcode);


					printf(
"M68K_FUNCTION(ori_%04X_wait_l_2)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_l_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_l_2);\n"
"}\n\n", opcode, opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_l_2);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_l_2);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_l_2)\n"
"{\n"
"	m68k->effective_address |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
	, opcode);

					sprintf(address, "m68k->effective_address");
					break;

				case 2: // (d16,pc)
					printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d16);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d16);\n"
"}\n\n", opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d16);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_d16);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_d16)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d16);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d16);\n"
"}\n\n", opcode, opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_d16)\n"
"{\n"
"	m68k->effective_address = (int16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode, opcode, opcode);

					sprintf(address, "m68k->effective_address");
					break;

				case 3: // (d8,pc,xn)
					printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d8);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d8);\n"
"}\n\n", opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d8);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_d8);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_d8)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_d8);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_d8);\n"
"}\n\n", opcode, opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_d8)\n"
"{\n"
"	m68k->effective_address = m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
"	TIMEOUT(2, ori_%04X_wait_d8_sum);\n"
"}\n\n", opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_d8_sum)\n"
"{\n"

"	if (m68k->effective_address & 0x800)\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_PC]\n"
"							+m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	else\n"
"		m68k->effective_address = (uint32_t)(int8_t)m68k->effective_address\n"
"							+m68k->reg[M68K_REG_PC]\n"
"							+(int16_t)m68k->reg[M68K_REG_D0+(m68k->effective_address>>12)&0xF];\n"
"	m68k->reg[M68K_REG_PC] += 2;\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_d8_sum);\n", opcode);

					sprintf(address, "m68k->effective_address");
					break;

				case 4: // #<data>
					printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_imm);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_imm);\n"
"}\n\n", opcode, opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_wait_imm);\n", opcode);
					fprintf(f, "M68K_FUNCTION(ori_%04X_read_imm);\n", opcode);

					printf(
"M68K_FUNCTION(ori_%04X_wait_imm)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_imm);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_imm);\n"
"}\n\n", opcode, opcode, opcode);

					printf(
"M68K_FUNCTION(ori_%04X_read_imm)\n"
"{\n", opcode);

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
							printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_imm_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_imm_2);\n"
"}\n\n", opcode, opcode);
							fprintf(f, "M68K_FUNCTION(ori_%04X_wait_imm_2);\n", opcode);
							fprintf(f, "M68K_FUNCTION(ori_%04X_read_imm_2);\n", opcode);

							printf(
"M68K_FUNCTION(ori_%04X_wait_imm_2)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_imm_2);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_imm_2);\n"
"}\n\n", opcode, opcode, opcode);

							printf(
"M68K_FUNCTION(ori_%04X_read_imm_2)\n"
"{\n"
"	m68k->effective_value |= (uint16_t)m68k->read_w(m68k, m68k->reg[M68K_REG_PC]);\n"
	, opcode);
							break;
					}
					printf("\tm68k->reg[M68K_REG_PC] += 2;\n");

					address[0] = 0;
					break;

				default:
					return 0;
			}
			break;

		default:
			return 0;
	}

	if (address[0])
	{
		if ((opcode & 0xC0) != 0x40)
		{
			printf(
"	if (%s&1)\n"
"		ADDRESS_EXCEPTION;\n",address);
		}
		printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_ea);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_ea);\n"
"}\n\n", opcode, opcode);
		fprintf(f, "M68K_FUNCTION(ori_%04X_wait_ea);\n", opcode);
		fprintf(f, "M68K_FUNCTION(ori_%04X_read_ea);\n", opcode);

		printf(
"M68K_FUNCTION(ori_%04X_wait_ea)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_ea);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_ea);\n"
"}\n\n", opcode, opcode, opcode);

		printf(
"M68K_FUNCTION(ori_%04X_read_ea)\n"
"{\n"
"	m68k->effective_value = m68k->read_w(m68k, %s)%s;\n"
	, opcode, address, tmp);

		if ((opcode & 0xC0) == 0x80)
		{
			printf(
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_4);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_4);\n"
"}\n\n", opcode, opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_wait_4);\n", opcode);
			fprintf(f, "M68K_FUNCTION(ori_%04X_read_4);\n", opcode);

			printf(
"M68K_FUNCTION(ori_%04X_wait_4)\n"
"{\n"
"	if (BUS_BUSY)\n"
"		TIMEOUT(BUS_WAIT_TIME, ori_%04X_wait_4);\n"
"	else\n"
"		TIMEOUT(READ_WAIT_TIME, ori_%04X_read_4);\n"
"}\n\n", opcode, opcode, opcode);

			printf(
"M68K_FUNCTION(ori_%04X_read_4)\n"
"{\n"
"	m68k->operand |= (uint16_t)m68k->read_w(m68k, %s + 2);\n"
		, opcode, address);
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
	return 1;
}

int valid[0x10000];

int main()
{
	int i,r;

	memset(valid, 0, sizeof(valid));
	f = fopen("ori_f.h","wb");
	printf("#include \"ori.h\"\n");
	printf("#include \"ori_f.h\"\n\n");
	for (i=0; i<0x100; ++i)
		valid[i] = ori(i);
	fclose(f);
	
	f = fopen("ori_ops.h","wb");
	printf("#include \"code/m68k.h\"\nm68k_function m68k_opcode_table[0x10000] = {\n");
	for (i=0; i<0x10000; ++i)
	{
		if (valid[i])
			printf("ori_%04X,\n",i);
		else
			printf("invalid,\n");
	}
	printf("}\n");
	fclose(f);
}
