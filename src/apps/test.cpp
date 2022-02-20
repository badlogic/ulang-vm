#include <cstdio>
#include <cmath>
#include <cstring>
#include "ulang.h"

typedef enum reg {
	R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, PC, SP
} reg;
const char *reg_names[] = {"r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14",
						   "pc", "sp"};

typedef enum check_type {
	NONE,
	MEM_BYTE,
	MEM_SHORT,
	MEM_INT,
	MEM_FLOAT,
	REG_INT,
	REG_FLOAT
} check_type;

typedef struct check {
	check_type type;
	union {
		uint32_t reg;
		uint32_t address;
	};
	union {
		int32_t val_int;
		uint32_t val_uint;
		float val_float;
	};
} check;

#define MAX_CHECKS 10

typedef struct test_case {
	const char *code;
	check checks[MAX_CHECKS];
} test_case;

ulang_bool test(size_t testNum, test_case *test) {
	ulang_file file = {};
	ulang_program program = {};
	ulang_error error = {};
	ulang_vm vm = {};

	ulang_file_from_memory("test.ul", test->code, &file);
	ulang_compile(&file, &program, &error);
	if (error.is_set) {
		printf("Test #%zu: compilation error\n", testNum);
		ulang_error_print(&error);
		return UL_FALSE;
	}

//	if (!strcmp(file.data, "load a, 1, r1\nhalt\na: byte 1 int 123\n")) {
//		printf("");
//	}

	ulang_vm_init(&vm, &program);
	while (ulang_vm_step(&vm));
	char checkErrorMessage[256] = {};

	for (int i = 0; i < MAX_CHECKS; i++) {
		check *check = &test->checks[i];
		switch (check->type) {
			case NONE:
				goto done;
			case MEM_BYTE: {
				int32_t memValue = vm.memory[check->address];
				int32_t expValue = check->val_int;
				if (memValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage,
							 "Byte value at address [%u]: %i (0x%x) != %i (0x%x)\n",
							 check->address, memValue, memValue, expValue, expValue);
					goto error;
				}
				break;
			}
			case MEM_SHORT: {
				int32_t memValue = *(int16_t *) &vm.memory[check->address];
				int32_t expValue = check->val_int;
				if (memValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage,
							 "Short value at address [%u]: %i (0x%x) != %i (0x%x)\n",
							 check->address, memValue, memValue, expValue, expValue);
					goto error;
				}
				break;
			}
			case MEM_INT: {
				int32_t memValue = *(int32_t *) &vm.memory[check->address];
				int32_t expValue = check->val_int;
				if (memValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage,
							 "Int value at address [%u]: %i (0x%x) != %i (0x%x)\n",
							 check->address, memValue, memValue, expValue, expValue);
					goto error;
				}
				break;
			}
			case MEM_FLOAT: {
				float memValue = *(float *) &vm.memory[check->address];
				float expValue = check->val_float;
				if (memValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage,
							 "Float value at address [%u]: %f != %f\n",
							 check->address, memValue, expValue);
					goto error;
				}
				break;
			}
			case REG_INT: {
				int32_t regValue = vm.registers[check->reg].i;
				int32_t expValue = check->val_int;
				if (regValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage,
							 "Value in register [%s]: %i (0x%x) != %i (0x%x)\n",
							 reg_names[check->reg], regValue, regValue, expValue, expValue);
					goto error;
				}
				break;
			}
			case REG_FLOAT: {
				float regValue = vm.registers[check->reg].fl;
				float expValue = check->val_float;
				if (regValue != expValue) {
					snprintf(checkErrorMessage, sizeof checkErrorMessage, "Value in register [%s]: %f != %f)\n",
							 reg_names[check->reg], regValue, expValue);
					goto error;
				}
				break;
			}
		}
	}
	done:
	ulang_vm_free(&vm);
	ulang_error_free(&error);
	ulang_program_free(&program);
	ulang_file_free(&file);
	return UL_TRUE;

	error:
	printf("Test #%zu\n---\n%s\n---\n", testNum, test->code);
	if (checkErrorMessage[0]) printf("Error: %s", checkErrorMessage);
	return UL_FALSE;
}

int main(int argc, char **argv) {
	test_case tests[] = {
			{"halt",                                                           {{REG_INT, .reg = PC, .val_uint = 4}}},

			// Test move first, as we need it for the other tests
			{"move 123, r1",                                                   {{REG_INT, .reg = R1, .val_int = 123}}},
			{"move 123.456, r1",                                               {{REG_FLOAT, .reg = R1, .val_float = 123.456f}}},
			{"move 123, r1\nmove r1, r2",                                      {{REG_INT, .reg = R1, .val_int = 123}}},
			{"move 123.456, r1\nmove r1, r2",                                  {{REG_INT, .reg = R1, .val_float = 123.456}}},

			// add
			{"move 0xffff, r1\nmove 43234, r2\nadd r1, r2, r3",                {{REG_INT, .reg = R3, .val_int = 0xffff +
																												43234}}},
			{"move -34234, r1\nmove 4323, r2\nadd r1, r2, r3",                 {{REG_INT, .reg = R3, .val_int = -34234 +
																												4323}}},
			{"add r1, 123, r2",                                                {{REG_INT, .reg = R2, .val_int = 123}}},

			// sub
			{"move 0xffff, r1\nmove 43234, r2\nsub r1, r2, r3",                {{REG_INT, .reg = R3, .val_int = 0xffff -
																												43234}}},
			{"move -34234, r1\nmove 4323, r2\nsub r1, r2, r3",                 {{REG_INT, .reg = R3, .val_int = -34234 -
																												4323}}},
			{"sub r1, 123, r2",                                                {{REG_INT, .reg = R2, .val_int = -123}}},

			// mul
			{"move 0xffff, r1\nmove 4323, r2\nmul r1, r2, r3",                 {{REG_INT, .reg = R3, .val_int = 0xffff *
																												4323}}},
			{"move -34234, r1\nmove 4323, r2\nmul r1, r2, r3",                 {{REG_INT, .reg = R3, .val_int = -34234 *
																												4323}}},
			{"move 3421, r1\nmul r1, 123, r2",                                 {{REG_INT, .reg = R2, .val_int = 3421 *
																												123}}},

			// div
			{"move 0xffff, r1\nmove 7, r2\ndiv r1, r2, r3",                    {{REG_INT, .reg = R3, .val_int = 0xffff /
																												7}}},
			{"move -34234, r1\nmove 7, r2\ndiv r1, r2, r3",                    {{REG_INT, .reg = R3, .val_int = -34234 /
																												7}}},
			{"move 3421, r1\ndiv r1, 7, r2",                                   {{REG_INT, .reg = R2, .val_int = 3421 /
																												7}}},

			// divu
			{"move 0xffff, r1\nmove 7, r2\ndivu r1, r2, r3",                   {{REG_INT, .reg = R3, .val_uint =
			0xffff /
			7}}},
			{"move -34234, r1\nmove 7, r2\ndivu r1, r2, r3",                   {{REG_INT, .reg = R3, .val_uint =
			((uint32_t) -34234) /
			7}}},
			{"move 3421, r1\ndivu r1, 7, r2",                                  {{REG_INT, .reg = R2, .val_uint = 3421 /
																												 7}}},

			// rem
			{"move 0xffff, r1\nmove 7, r2\nrem r1, r2, r3",                    {{REG_INT, .reg = R3, .val_int = 0xffff %
																												7}}},
			{"move -34234, r1\nmove 7, r2\nrem r1, r2, r3",                    {{REG_INT, .reg = R3, .val_int = -34234 %
																												7}}},
			{"move 3421, r1\nrem r1, 7, r2",                                   {{REG_INT, .reg = R2, .val_int = 3421 %
																												7}}},

			// remu
			{"move 0xffff, r1\nmove 7, r2\nremu r1, r2, r3",                   {{REG_INT, .reg = R3, .val_uint =
			0xffff %
			7}}},
			{"move -34234, r1\nmove 7, r2\nremu r1, r2, r3",                   {{REG_INT, .reg = R3, .val_uint =
			(uint32_t) -34234 %
			7}}},
			{"move 3421, r1\nremu r1, 7, r2",                                  {{REG_INT, .reg = R2, .val_uint = 3421 %
																												 7}}},

			// addf
			{"move 123.456, r1\nmove -234.567, r2\naddf r1, r2, r3",           {{REG_FLOAT, .reg = R3, .val_float =
			123.456f +
			-234.567f}}},
			{"addf r1, 123.456, r2",                                           {{REG_FLOAT, .reg = R2, .val_float = 123.456}}},
			{"addf r1, 123, r2",                                               {{REG_FLOAT, .reg = R2, .val_float = 123}}},

			// subf
			{"move 123.456, r1\nmove 234.567, r2\nsubf r1, r2, r3",            {{REG_FLOAT, .reg = R3, .val_float =
			123.456f -
			234.567f}}},
			{"subf r1, 123.456, r2",                                           {{REG_FLOAT, .reg = R2, .val_float = -123.456f}}},

			// mulf
			{"move 123.456, r1\nmove 234.567, r2\nmulf r1, r2, r3",            {{REG_FLOAT, .reg = R3, .val_float =
			123.456f *
			234.567f}}},
			{"move 123.456, r1\nmulf r1, 234.567, r2",                         {{REG_FLOAT, .reg = R2, .val_float =
			123.456f *
			234.567f}}},

			// divf
			{"move 123.456, r1\nmove 234.567, r2\ndivf r1, r2, r3",            {{REG_FLOAT, .reg = R3, .val_float =
			123.456f /
			234.567f}}},
			{"move 123.456, r1\ndivf r1, 234.567, r2",                         {{REG_FLOAT, .reg = R2, .val_float =
			123.456f /
			234.567f}}},
			// cos, sin, atan2, sqrt, pow
			{"move 123.456, r1\ncosf r1, r2",                                  {{REG_FLOAT, .reg = R2, .val_float = cosf(
					123.456f)}}},
			{"move 123.456, r1\nsinf r1, r2",                                  {{REG_FLOAT, .reg = R2, .val_float = sinf(
					123.456f)}}},
			{"move 123.456, r1\nmove 234.567, r2\natan2f r1, r2, r3",          {{REG_FLOAT, .reg = R3, .val_float = atan2f(
					123.456f, 234.567f)}}},
			{"move 123.456, r1\nsqrtf r1, r2",                                 {{REG_FLOAT, .reg = R2, .val_float = sqrtf(
					123.456f)}}},
			{"move 123.456, r1\nmove 234.567, r2\npowf r1, r2, r3",            {{REG_FLOAT, .reg = R3, .val_float = powf(
					123.456f, 234.567f)}}},
			{"move 123.456, r1\npowf r1, 4, r3",                               {{REG_FLOAT, .reg = R3, .val_float = powf(
					123.456f, 4.0f)}}},
			{"move 123, r1\ni2f r1, r1",                                       {{REG_FLOAT, .reg = R1, .val_float = 123}}},
			{"move 123.456, r1\nf2i r1, r1",                                   {{REG_FLOAT, .reg = R1, .val_int = 123}}},

			// not, and, or, xor, shl, shr
			{"move 0xf0f0f, r1\nnot r1, r1",                                   {{REG_INT, .reg = R1, .val_int = ~0xf0f0f}}},
			{"not 0xf0f0f, r1",                                                {{REG_INT, .reg = R1, .val_int = ~0xf0f0f}}},
			{"move 0xf0f0f, r1\nmove 0xf0f0, r2\nand r1, r2, r3",              {{REG_INT, .reg = R3, .val_int =
			0xf0f0f &
			0xf0f0}}},
			{"move 0xf0f0f, r1\nand r1, 0xf0f0, r3",                           {{REG_INT, .reg = R3, .val_int =
			0xf0f0f &
			0xf0f0}}},
			{"move 0xf0f0f, r1\nmove 0xf0f0, r2\nor r1, r2, r3",               {{REG_INT, .reg = R3, .val_int =
			0xf0f0f |
			0xf0f0}}},
			{"move 0xf0f0f, r1\nor r1, 0xf0f0, r3",                            {{REG_INT, .reg = R3, .val_int =
			0xf0f0f |
			0xf0f0}}},
			{"move 0xf0f0f, r1\nmove 0xf0f0, r2\nxor r1, r2, r3",              {{REG_INT, .reg = R3, .val_int =
			0xf0f0f ^
			0xf0f0}}},
			{"move 0xf0f0f, r1\nxor r1, 0xf0f0, r3",                           {{REG_INT, .reg = R3, .val_int =
			0xf0f0f ^
			0xf0f0}}},
			{"move 0xf0f0f, r1\nmove 4, r2\nshl r1, r2, r3",                   {{REG_INT, .reg = R3, .val_int = 0xf0f0f
					<< 4}}},
			{"move 0xf0f0f, r1\nshl r1, 4, r3",                                {{REG_INT, .reg = R3, .val_int = 0xf0f0f
					<< 4}}},
			{"move 0xf0f0f, r1\nmove 4, r2\nshr r1, r2, r3",                   {{REG_INT, .reg = R3, .val_int = 0xf0f0f
					>> 4}}},
			{"move 0xf0f0f, r1\nshr r1, 4, r3",                                {{REG_INT, .reg = R3, .val_int = 0xf0f0f
					>> 4}}},

			// cmp
			{"move -11, r1\nmove 22, r2\ncmp r1, r2, r3",                      {{REG_INT, .reg = R3, .val_int = -1}}},
			{"move 33, r1\nmove -31, r2\ncmp r1, r2, r3",                      {{REG_INT, .reg = R3, .val_int = 1}}},
			{"move 1, r1\nmove 1, r2\ncmp r1, r2, r3",                         {{REG_INT, .reg = R3, .val_int = 0}}},
			{"move 1, r1\ncmp r1, 2, r3",                                      {{REG_INT, .reg = R3, .val_int = -1}}},
			{"move 1, r1\ncmp r1, -1, r3",                                     {{REG_INT, .reg = R3, .val_int = 1}}},
			{"move 1, r1\ncmp r1, 1, r3",                                      {{REG_INT, .reg = R3, .val_int = 0}}},

			// cmpu
			{"move -11, r1\nmove 22, r2\ncmpu r1, r2, r3",                     {{REG_INT, .reg = R3, .val_int = 1}}},
			{"move 33, r1\nmove -31, r2\ncmpu r1, r2, r3",                     {{REG_INT, .reg = R3, .val_int = -1}}},
			{"move 1, r1\nmove 1, r2\ncmpu r1, r2, r3",                        {{REG_INT, .reg = R3, .val_int = 0}}},
			{"move 2, r1\ncmpu r1, 1, r3",                                     {{REG_INT, .reg = R3, .val_int = 1}}},
			{"move 1, r1\ncmpu r1, -1, r3",                                    {{REG_INT, .reg = R3, .val_int = -1}}},
			{"move 1, r1\ncmpu r1, 1, r3",                                     {{REG_INT, .reg = R3, .val_int = 0}}},

			// cmpf
			{"move -134.3, r1\nmove 22.34, r2\ncmpf r1, r2, r3",               {{REG_INT, .reg = R3, .val_int = -1}}},
			{"move 33.43, r1\nmove -31.234, r2\ncmpf r1, r2, r3",              {{REG_INT, .reg = R3, .val_int = 1}}},
			{"move 1.432, r1\nmove 1.432, r2\ncmpf r1, r2, r3",                {{REG_INT, .reg = R3, .val_int = 0}}},

			// jmp, je, jne, jl, jg, jle, jge
			{"jmp l\nhalt\nl: move 123, r1\n",                                 {{REG_INT, .reg = PC, .val_uint = 24}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 1, r1\nje r1, l\nhalt\nl: move 123, r1\n",   {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 2, r2\nje r2, l\nmove 123, r1\nl: halt\n",   {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 2, r1\njne r1, l\nhalt\nl: move 123, r1\n",  {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 1, r2\njne r2, l\nmove 123, r1\nl: halt\n",  {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 2, r1\njl r1, l\nhalt\nl: move 123, r1\n",   {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, -1, r2\njl r2, l\nmove 123, r1\nl: halt\n",  {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 2, r1\ncmp r1, 1, r1\njg r1, l\nhalt\nl: move 123, r1\n",   {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move -1, r1\ncmp r1, 1, r2\njg r2, l\nmove 123, r1\nl: halt\n",  {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 1, r1\njle r1, l\nhalt\nl: move 123, r1\n",  {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move -1, r1\ncmp r1, 1, r1\njle r1, l\nhalt\nl: move 123, r1\n", {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, -1, r2\njle r2, l\nmove 123, r1\nl: halt\n", {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, 1, r1\njge r1, l\nhalt\nl: move 123, r1\n",  {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move 1, r1\ncmp r1, -1, r1\njge r1, l\nhalt\nl: move 123, r1\n", {{REG_INT, .reg = PC, .val_uint = 40}, {REG_INT, .reg = R1, .val_int = 123}}},
			{"move -1, r1\ncmp r1, 1, r2\njge r2, l\nmove 123, r1\nl: halt\n", {{REG_INT, .reg = PC, .val_uint = 36}, {REG_INT, .reg = R1, .val_int = 123}}},


			// load, store
			{"load a, 1, r1\nhalt\na: byte 1 int 123\n",                       {{REG_INT, .reg = PC, .val_uint = 12}, {REG_INT, .reg = R1, .val_int = 123}}},

			//{"jump h\nbyte 123\nh: halt", {{MEM_BYTE, .address = 0, .val_int = 0}}},
			//{"halt",                      {{MEM_SHORT, .address = 0, .val_int = 0}}}
	};
	for (size_t i = 0; i < sizeof(tests) / sizeof(test_case); i++) {
		test_case *t = &tests[i];
		if (!test(i, t)) {
			return -1;
		}
		printf("Test #%zu: OK\n", i);
	}

	ulang_print_memory();
	return 0;

	(void) argc;
	(void) argv;
}
