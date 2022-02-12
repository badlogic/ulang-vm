#ifndef ULANG_H
#define ULANG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// types
#define ULANG_TRUE -1
#define ULANG_FALSE 0
typedef int ulang_bool;

typedef struct ulang_string {
	char *data;
	size_t length;
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

struct ulang_file;

typedef struct ulang_error {
	struct ulang_file *file;
	ulang_span span;
	ulang_string message;
	ulang_bool is_set;
} ulang_error;

typedef struct ulang_file {
	ulang_string fileName;
	char *data;
	size_t length;
	ulang_line *lines;
	size_t numLines;
} ulang_file;

typedef struct ulang_label {
	ulang_span label;
	size_t codeAddress;
} ulang_label;

typedef struct ulang_program {
	uint8_t *code;
	size_t codeLength;
	ulang_label *labels;
	size_t labelsLength;
} ulang_program;

typedef union ulang_value {
	int8_t b;
	int16_t s;
	int32_t i;
	uint32_t ui;
	float fl;
} ulang_value;

typedef struct ulang_vm {
	ulang_value registers[16];
	uint8_t *memory;
	size_t memorySizeBytes;
	uint8_t *stack;
	size_t stackSizeBytes;
} ulang_vm;

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

ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error);

void ulang_program_free(ulang_program *program);

// interpreter
void ulang_vm_init(ulang_vm *vm, size_t memorySizeBytes, size_t stackSizeBytes, ulang_program *program);

ulang_bool ulang_vm_step(ulang_vm *vm);

void ulang_vm_print(ulang_vm *vm);

void ulang_vm_free(ulang_vm *vm);


#ifdef __cplusplus
};
#endif
#endif
