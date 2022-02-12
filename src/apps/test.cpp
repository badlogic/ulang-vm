#include <cstdio>
#include "ulang.h"

typedef enum reg {
	R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, PC, SP
} reg;
const char *reg_names[] = {"r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14",
						   "pc", "sp"};

typedef enum check_type {
	NONE,
	MEMORY_BYTE,
	MEMORY_SHORT,
	MEMORY_INT,
	MEMORY_FLOAT,
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
		return ULANG_FALSE;
	}

	ulang_vm_init(&vm, 1024 * 1024, 4 * 1000, &program);
	while (ulang_vm_step(&vm));
	char checkErrorMessage[256] = {};

	for (int i = 0; i < MAX_CHECKS; i++) {
		check *check = &test->checks[i];
		switch (check->type) {
			case NONE:
				goto done;
			case MEMORY_BYTE: {
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
			case MEMORY_SHORT: {
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
			case MEMORY_INT: {
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
			case MEMORY_FLOAT: {
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
	return ULANG_TRUE;

	error:
	printf("Test #%zu\n---\n%s\n---\n", testNum, test->code);
	if (checkErrorMessage[0]) printf("Error: %s", checkErrorMessage);
	return ULANG_FALSE;
}

int main(int argc, char **argv) {
	test_case tests[] = {
			{"halt",             {{REG_INT, .reg = PC, .val_uint = 4}}},
			{"move 123, r1",     {{REG_INT, .reg = R1, .val_int = 123}}},
			{"move 123.456, r1", {{REG_FLOAT, .reg = R1, .val_float = 123.456f}}},
			//{"jump h\nbyte 123\nh: halt", {{MEMORY_BYTE, .address = 0, .val_int = 0}}},
			//{"halt",                      {{MEMORY_SHORT, .address = 0, .val_int = 0}}}
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