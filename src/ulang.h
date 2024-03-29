#ifndef ULANG_H
#define ULANG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// defines & types
#define UL_TRUE -1
#define UL_FALSE 0
typedef int ulang_bool;
#define UL_VM_MEMORY_SIZE (1024 * 1024 * 32)

typedef struct ulang_string {
	char *data;
	uint32_t length;
} ulang_string;

typedef struct ulang_span {
	ulang_string data;
	uint32_t startLine;
	uint32_t endLine;
} ulang_span;

typedef struct ulang_line {
	ulang_string data;
	uint32_t lineNumber;
} ulang_line;

typedef struct ulang_file {
	ulang_string fileName;
	char *data;
	size_t length;
	ulang_line *lines;
	size_t numLines;
} ulang_file;

typedef struct ulang_error {
	ulang_file *file;
	ulang_span span;
	ulang_string message;
	ulang_bool is_set;
} ulang_error;


typedef enum ulang_label_target {
	UL_LT_UNINITIALIZED,
	UL_LT_CODE,
	UL_LT_DATA,
	UL_LT_RESERVED_DATA
} ulang_label_target;

typedef struct ulang_label {
	ulang_span label;
	ulang_label_target target;
	size_t address;
} ulang_label;

typedef enum value_type {
    UL_INTEGER,
	UL_FLOAT
} value_type;

typedef struct ulang_constant {
	value_type type;
	ulang_span name;
	union {
		int32_t i;
		float f;
	};
} ulang_constant;

typedef struct ulang_program {
	uint8_t *code;
	size_t codeLength;
	uint8_t *data;
	size_t dataLength;
	size_t reservedBytes;
	ulang_label *labels;
	size_t labelsLength;
	ulang_constant *constants;
	size_t constantsLength;
	ulang_file **files;
	size_t filesLength;
	uint32_t *addressToLine;
	uint32_t addressToLineLength;
	ulang_file **addressToFile;
	uint32_t addressToFileLength;
} ulang_program;

typedef union ulang_value {
	int8_t b;
	int16_t s;
	int32_t i;
	uint32_t ui;
	float f;
} ulang_value;

struct ulang_vm;

typedef ulang_bool (*ulang_syscall)(uint32_t intNum, struct ulang_vm *vm);
typedef ulang_bool (*ulang_file_read_function)(const char *filename, ulang_file *file);

typedef struct ulang_vm {
	ulang_value registers[16];
	uint8_t *memory;
	size_t memorySizeBytes;
	ulang_syscall syscalls[256];
	ulang_error error;
	ulang_program *program;
} ulang_vm;

// string, span
ulang_bool ulang_string_equals(ulang_string *a, ulang_string *b);

ulang_bool ulang_span_matches(ulang_span *span, const char *needle, size_t length);

// memory
void *ulang_alloc(size_t num_bytes);

void *ulang_realloc(void *old, size_t num_bytes);

void ulang_free(void *ptr);

void ulang_print_memory();

// i/o
ulang_bool ulang_file_read(const char *fileName, ulang_file *file);

ulang_bool ulang_file_from_memory(const char *fileName, const char *content, ulang_file *file);

void ulang_file_free(ulang_file *file);

void ulang_error_print(ulang_error *error);

void ulang_error_free(ulang_error *error);

// compilation

ulang_bool ulang_compile(const char *filename, ulang_file_read_function fileReadFunction, ulang_program *program, ulang_error *error);

void ulang_program_free(ulang_program *program);

// interpreter
void ulang_vm_init(ulang_vm *vm, ulang_program *program);

ulang_bool ulang_vm_step(ulang_vm *vm);

ulang_bool ulang_vm_step_n(ulang_vm *vm, uint32_t numInstructions);

int32_t ulang_vm_step_n_bp(ulang_vm *vm, uint32_t numInstructions, uint32_t *breakpoints, uint32_t numBreakpoints);

ulang_bool ulang_vm_debug(ulang_vm *vm);

int32_t ulang_vm_pop_int(ulang_vm *vm);

uint32_t ulang_vm_pop_uint(ulang_vm *vm);

float ulang_vm_pop_float(ulang_vm *vm);

void ulang_vm_push_int(ulang_vm *vm, int32_t val);

void ulang_vm_push_uint(ulang_vm *vm, uint32_t val);

void ulang_vm_push_float(ulang_vm *vm, float val);

void ulang_vm_print(ulang_vm *vm);

void ulang_vm_free(ulang_vm *vm);


#ifdef __cplusplus
};
#endif
#endif
