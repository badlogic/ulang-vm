#include <ulang.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define STR(str) str, sizeof(str) - 1
#define STR_OBJ(str) (ulang_string){ str, sizeof(str) - 1 }
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

#define ARRAY_IMPLEMENT(name, itemType) \
    typedef struct name { size_t size; size_t capacity; itemType* items; } name; \
    name* name##_init(size_t initialCapacity) { \
        name* array = (name *)ulang_alloc(sizeof(name)); \
        array->size = 0; \
        array->capacity = initialCapacity; \
        array->items = (itemType*)ulang_alloc(sizeof(itemType) * initialCapacity); \
        return array; \
    } \
    void name##_free(name *self) { \
        ulang_free(self->items); \
        ulang_free(self); \
    } \
    void name##_init_inplace(name* array, size_t initialCapacity) { \
        array->size = 0; \
        array->capacity = initialCapacity; \
        array->items = (itemType*)ulang_alloc(sizeof(itemType) * initialCapacity); \
    } \
    void name##_free_inplace(name *self) { \
        ulang_free(self->items); \
    } \
    void name##_ensure(name* self, size_t numElements) { \
        if (self->size + numElements >= self->capacity) { \
            self->capacity = MAX(8, (size_t)((self->size + numElements) * 1.75f)); \
            self->items = (itemType*)ulang_realloc(self->items, sizeof(itemType) * self->capacity); \
        } \
    }\
    void name##_add(name* self, itemType value) { \
        name##_ensure(self, 1); \
        self->items[self->size++] = value; \
    }

typedef struct expression_value {
	value_type type;
	int32_t i;
	float f;
	uint32_t startToken;
	uint32_t endToken;
	ulang_bool unresolved;
} expression_value;

typedef enum patch_type {
	PT_VALUE,
	PT_OFFSET
} patch_type;

typedef struct patch {
	patch_type type;
	expression_value expr;
	size_t patchAddress;
} patch;

typedef enum token_type {
	TOKEN_INTEGER,
	TOKEN_FLOAT,
	TOKEN_STRING,
	TOKEN_IDENTIFIER,
	TOKEN_SPECIAL_CHAR,
	TOKEN_EOF
} token_type;

typedef struct token {
	ulang_span span;
	token_type type;
} token;

ARRAY_IMPLEMENT(token_array, token)

ARRAY_IMPLEMENT(patch_array, patch)

ARRAY_IMPLEMENT(constant_array, ulang_constant)

ARRAY_IMPLEMENT(label_array, ulang_label)

ARRAY_IMPLEMENT(byte_array, uint8_t)

ARRAY_IMPLEMENT(int_array, uint32_t)

typedef struct token_stream {
	ulang_file *file;
	token_array *tokens;
	size_t index;
} token_stream;

typedef struct compiler_context {
	token_array tokens;
	token_stream stream;
	patch_array patches;
	label_array labels;
	constant_array constants;
	byte_array code;
	byte_array data;
	int_array addressToLine;
	size_t numReservedBytes;
	ulang_error *error;
	ulang_bool resolveLabelsInExpressions;
} compiler_context;

typedef enum ulang_opcode {
	HALT,
	BREAK,
	ADD,
	ADD_VAL,
	SUB,
	SUB_VAL,
	MUL,
	MUL_VAL,
	DIV,
	DIV_VAL,
	DIV_UNSIGNED,
	DIV_UNSIGNED_VAL,
	REMAINDER,
	REMAINDER_VAL,
	REMAINDER_UNSIGNED,
	REMAINDER_UNSIGNED_VAL,
	ADD_FLOAT,
	ADD_FLOAT_VAL,
	SUB_FLOAT,
	SUB_FLOAT_VAL,
	MUL_FLOAT,
	MUL_FLOAT_VAL,
	DIV_FLOAT,
	DIV_FLOAT_VAL,
	COS,
	SIN,
	ATAN2,
	SQRT,
	POW,
	POW_VAL,
	RAND,
	INT_TO_FLOAT,
	FLOAT_TO_INT,
	NOT,
	NOT_VAL,
	AND,
	AND_VAL,
	OR,
	OR_VAL,
	XOR,
	XOR_VAL,
	SHL,
	SHL_VAL,
	SHR,
	SHR_VAL,
	SHRU,
	SHRU_VAL,
	CMP,
	CMP_REG_VAL,
	CMP_UNSIGNED,
	CMP_UNSIGNED_REG_VAL,
	CMP_FLOAT,
	CMP_FLOAT_REG_VAL,
	JUMP,
	JUMP_EQUAL,
	JUMP_NOT_EQUAL,
	JUMP_LESS,
	JUMP_GREATER,
	JUMP_LESS_EQUAL,
	JUMP_GREATER_EQUAL,
	MOVE_REG,
	MOVE_VAL,
	LOAD_REG,
	LOAD_VAL,
	STORE_REG,
	STORE_REG_REG,
	STORE_VAL,
	LOAD_BYTE_REG,
	LOAD_BYTE_VAL,
	STORE_BYTE_REG,
	STORE_BYTE_VAL,
	LOAD_SHORT_REG,
	LOAD_SHORT_VAL,
	STORE_SHORT_REG,
	STORE_SHORT_VAL,
	PUSH_REG,
	PUSH_VAL,
	STACKALLOC,
	POP_REG,
	POP_OFF,
	CALL_REG,
	CALL_VAL,
	RET,
	RETN,
	SYSCALL
} ulang_opcode;

typedef enum operand_type {
	UL_NIL = 0,  // No operand
	UL_REG, // Register
	UL_LBL_INT, // Label or int
	UL_LBL_INT_FLT, // label or int or float
	UL_INT, // int
	UL_FLT, // float
	UL_OFF, // Offset
} operand_type;

typedef struct opcode {
	ulang_opcode code;
	ulang_string name;
	operand_type operands[3];
	int numOperands;
	ulang_bool hasValueOperand;
	uint32_t index;
} opcode;

static opcode opcodes[] = {
		{HALT,                   STR_OBJ("halt")},
		{BREAK,                  STR_OBJ("brk"),        {UL_REG,         UL_LBL_INT_FLT}},
		{ADD,                    STR_OBJ("add"),        {UL_REG,         UL_REG,     UL_REG}},
		{ADD_VAL,                STR_OBJ("add"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{SUB,                    STR_OBJ("sub"),        {UL_REG,         UL_REG,     UL_REG}},
		{SUB_VAL,                STR_OBJ("sub"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{MUL,                    STR_OBJ("mul"),        {UL_REG,         UL_REG,     UL_REG}},
		{MUL_VAL,                STR_OBJ("mul"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{DIV,                    STR_OBJ("div"),        {UL_REG,         UL_REG,     UL_REG}},
		{DIV_VAL,                STR_OBJ("div"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{DIV_UNSIGNED,           STR_OBJ("divu"),       {UL_REG,         UL_REG,     UL_REG}},
		{DIV_UNSIGNED_VAL,       STR_OBJ("divu"),       {UL_REG,         UL_LBL_INT, UL_REG}},
		{REMAINDER,              STR_OBJ("rem"),        {UL_REG,         UL_REG,     UL_REG}},
		{REMAINDER_VAL,          STR_OBJ("rem"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{REMAINDER_UNSIGNED,     STR_OBJ("remu"),       {UL_REG,         UL_REG,     UL_REG}},
		{REMAINDER_UNSIGNED_VAL, STR_OBJ("remu"),       {UL_REG,         UL_LBL_INT, UL_REG}},
		{ADD_FLOAT,              STR_OBJ("addf"),       {UL_REG,         UL_REG,     UL_REG}},
		{ADD_FLOAT_VAL,          STR_OBJ("addf"),       {UL_REG,         UL_FLT,     UL_REG}},
		{SUB_FLOAT,              STR_OBJ("subf"),       {UL_REG,         UL_REG,     UL_REG}},
		{SUB_FLOAT_VAL,          STR_OBJ("subf"),       {UL_REG,         UL_FLT,     UL_REG}},
		{MUL_FLOAT,              STR_OBJ("mulf"),       {UL_REG,         UL_REG,     UL_REG}},
		{MUL_FLOAT_VAL,          STR_OBJ("mulf"),       {UL_REG,         UL_FLT,     UL_REG}},
		{DIV_FLOAT,              STR_OBJ("divf"),       {UL_REG,         UL_REG,     UL_REG}},
		{DIV_FLOAT_VAL,          STR_OBJ("divf"),       {UL_REG,         UL_FLT,     UL_REG}},
		{COS,                    STR_OBJ("cosf"),       {UL_REG,         UL_REG}},
		{SIN,                    STR_OBJ("sinf"),       {UL_REG,         UL_REG}},
		{ATAN2,                  STR_OBJ("atan2f"),     {UL_REG,         UL_REG,     UL_REG}},
		{SQRT,                   STR_OBJ("sqrtf"),      {UL_REG,         UL_REG}},
		{POW,                    STR_OBJ("powf"),       {UL_REG,         UL_REG,     UL_REG}},
		{POW_VAL,                STR_OBJ("powf"),       {UL_REG,         UL_FLT,     UL_REG}},
		{RAND,                   STR_OBJ("rand"),       {UL_REG}},
		{INT_TO_FLOAT,           STR_OBJ("i2f"),        {UL_REG,         UL_REG}},
		{FLOAT_TO_INT,           STR_OBJ("f2i"),        {UL_REG,         UL_REG}},

		{NOT,                    STR_OBJ("not"),        {UL_REG,         UL_REG}},
		{NOT_VAL,                STR_OBJ("not"),        {UL_LBL_INT,     UL_REG}},
		{AND,                    STR_OBJ("and"),        {UL_REG,         UL_REG,     UL_REG}},
		{AND_VAL,                STR_OBJ("and"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{OR,                     STR_OBJ("or"),         {UL_REG,         UL_REG,     UL_REG}},
		{OR_VAL,                 STR_OBJ("or"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{XOR,                    STR_OBJ("xor"),        {UL_REG,         UL_REG,     UL_REG}},
		{XOR_VAL,                STR_OBJ("xor"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{SHL,                    STR_OBJ("shl"),        {UL_REG,         UL_REG,     UL_REG}},
		{SHL_VAL,                STR_OBJ("shl"),        {UL_REG,         UL_OFF,     UL_REG}},
		{SHR,                    STR_OBJ("shr"),        {UL_REG,         UL_REG,     UL_REG}},
		{SHR_VAL,                STR_OBJ("shr"),        {UL_REG,         UL_OFF,     UL_REG}},
		{SHRU,                   STR_OBJ("shru"),       {UL_REG,         UL_REG,     UL_REG}},
		{SHRU_VAL,               STR_OBJ("shru"),       {UL_REG,         UL_OFF,     UL_REG}},

		{CMP,                    STR_OBJ("cmp"),        {UL_REG,         UL_REG,     UL_REG}},
		{CMP_REG_VAL,            STR_OBJ("cmp"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{CMP_UNSIGNED,           STR_OBJ("cmpu"),       {UL_REG,         UL_REG,     UL_REG}},
		{CMP_UNSIGNED_REG_VAL,   STR_OBJ("cmpu"),       {UL_REG,         UL_LBL_INT, UL_REG}},
		{CMP_FLOAT,              STR_OBJ("cmpf"),       {UL_REG,         UL_REG,     UL_REG}},
		{CMP_FLOAT_REG_VAL,      STR_OBJ("cmpf"),       {UL_REG,         UL_FLT,     UL_REG}},


		{JUMP,                   STR_OBJ("jmp"),        {UL_LBL_INT}},
		{JUMP_EQUAL,             STR_OBJ("je"),         {UL_REG,         UL_LBL_INT}},
		{JUMP_NOT_EQUAL,         STR_OBJ("jne"),        {UL_REG,         UL_LBL_INT}},
		{JUMP_LESS,              STR_OBJ("jl"),         {UL_REG,         UL_LBL_INT}},
		{JUMP_GREATER,           STR_OBJ("jg"),         {UL_REG,         UL_LBL_INT}},
		{JUMP_LESS_EQUAL,        STR_OBJ("jle"),        {UL_REG,         UL_LBL_INT}},
		{JUMP_GREATER_EQUAL,     STR_OBJ("jge"),        {UL_REG,         UL_LBL_INT}},

		{MOVE_REG,               STR_OBJ("mov"),        {UL_REG,         UL_REG}},
		{MOVE_VAL,               STR_OBJ("mov"),        {UL_LBL_INT_FLT, UL_REG}},

		{LOAD_REG,               STR_OBJ("ld"),         {UL_REG,         UL_OFF,     UL_REG}},
		{LOAD_VAL,               STR_OBJ("ld"),         {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{STORE_REG,              STR_OBJ("sto"),        {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_REG_REG,          STR_OBJ("sto"),        {UL_REG,         UL_REG,     UL_REG}},
		{STORE_VAL,              STR_OBJ("sto"),        {UL_REG,         UL_LBL_INT, UL_OFF}},

		{LOAD_BYTE_REG,          STR_OBJ("ldb"),        {UL_REG,         UL_OFF,     UL_REG}},
		{LOAD_BYTE_VAL,          STR_OBJ("ldb"),        {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{STORE_BYTE_REG,         STR_OBJ("stob"),       {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_BYTE_VAL,         STR_OBJ("stob"),       {UL_REG,         UL_LBL_INT, UL_OFF}},

		{LOAD_SHORT_REG,         STR_OBJ("lds"),        {UL_REG,         UL_OFF,     UL_REG}},
		{LOAD_SHORT_VAL,         STR_OBJ("lds"),        {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{STORE_SHORT_REG,        STR_OBJ("stos"),       {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_SHORT_VAL,        STR_OBJ("stos"),       {UL_REG,         UL_LBL_INT, UL_OFF}},

		{PUSH_REG,               STR_OBJ("push"),       {UL_REG}},
		{PUSH_VAL,               STR_OBJ("push"),       {UL_LBL_INT_FLT}},

		{STACKALLOC,             STR_OBJ("stackalloc"), {UL_OFF}},

		{POP_REG,                STR_OBJ("pop"),        {UL_REG}},
		{POP_OFF,                STR_OBJ("pop"),        {UL_OFF}},

		{CALL_REG,               STR_OBJ("call"),       {UL_REG}},
		{CALL_VAL,               STR_OBJ("call"),       {UL_LBL_INT}},

		{RET,                    STR_OBJ("ret")},
		{RETN,                   STR_OBJ("retn"),       {UL_OFF}},


		{SYSCALL,                STR_OBJ("syscall"),    {UL_OFF}},
};

static size_t opcodeLength = sizeof(opcodes) / sizeof(opcode);

typedef struct reg {
	ulang_string name;
	int index;
} reg;

static reg registers[] = {
		{{STR("r1")}},
		{{STR("r2")}},
		{{STR("r3")}},
		{{STR("r4")}},
		{{STR("r5")}},
		{{STR("r6")}},
		{{STR("r7")}},
		{{STR("r8")}},
		{{STR("r9")}},
		{{STR("r10")}},
		{{STR("r11")}},
		{{STR("r12")}},
		{{STR("r13")}},
		{{STR("r14")}},
		{{STR("pc")}},
		{{STR("sp")}}
};

static ulang_bool initialized = UL_FALSE;

static void init_opcodes_and_registers() {
	if (initialized) return;

	for (size_t i = 0; i < opcodeLength; i++) {
		opcode *opcode = &opcodes[i];
		opcode->index = i;
		for (int j = 0; j < 3; j++) {
			if (opcode->operands[j] == UL_NIL) break;
			if (opcode->operands[j] == UL_LBL_INT || opcode->operands[j] == UL_LBL_INT_FLT ||
				opcode->operands[j] == UL_INT || opcode->operands[j] == UL_FLT)
				opcode->hasValueOperand = UL_TRUE;
			opcode->numOperands++;
		}
	}

	for (size_t i = 0; i < (sizeof(registers) / sizeof(reg)); i++) {
		registers[i].index = (int) i;
	}
	initialized = UL_TRUE;
}

static int allocs = 0;
static int frees = 0;

void *ulang_alloc(size_t numBytes) {
	allocs++;
	return malloc(numBytes);
}

EMSCRIPTEN_KEEPALIVE void *ulang_calloc(size_t numBytes) {
	allocs++;
	void *result = malloc(numBytes);
	memset(result, 0, numBytes);
	return result;
}

void *ulang_realloc(void *old, size_t numBytes) {
	return realloc(old, numBytes);
}

EMSCRIPTEN_KEEPALIVE void ulang_free(void *ptr) {
	if (!ptr) return;
	frees++;
	free(ptr);
}

EMSCRIPTEN_KEEPALIVE void ulang_print_memory() {
	printf("Allocations: %i\nFrees: %i\n", allocs, frees);
}

ulang_bool ulang_file_read(const char *fileName, ulang_file *file) {
	FILE *stream = fopen(fileName, "rb");
	if (stream == NULL) {
		return UL_FALSE;
	}

	// BOZO error handling, files > 2GB
	fseek(stream, 0L, SEEK_END);
	file->length = ftell(stream);
	fseek(stream, 0L, SEEK_SET);

	file->data = (char *) ulang_alloc(file->length);
	fread(file->data, sizeof(char), file->length, stream);
	fclose(stream);

	size_t fileNameLength = strlen(fileName) + 1;
	file->fileName.data = (char *) ulang_alloc(fileNameLength);
	file->fileName.length = fileNameLength - 1;
	memcpy(file->fileName.data, fileName, fileNameLength);

	file->lines = NULL;
	file->numLines = 0;
	return UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE ulang_bool ulang_file_from_memory(const char *fileName, const char *content, ulang_file *file) {
	size_t len = strlen(content);
	file->data = ulang_alloc(len + 1);
	memcpy(file->data, content, len);
	file->data[len] = 0;
	file->length = len;
	size_t fileNameLength = strlen(fileName) + 1;
	file->fileName.data = (char *) ulang_alloc(fileNameLength);
	file->fileName.length = fileNameLength - 1;
	memcpy(file->fileName.data, fileName, fileNameLength);
	file->lines = NULL;
	file->numLines = 0;
	return UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE void ulang_file_free(ulang_file *file) {
	ulang_free(file->fileName.data);
	ulang_free(file->data);
	file->fileName.data = NULL;
	file->fileName.length = 0;
	file->data = NULL;
	if (file->lines) {
		ulang_free(file->lines);
		file->lines = NULL;
		file->numLines = 0;
	}
}

void ulang_error_init(ulang_error *error, ulang_file *file, ulang_span *span, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	char scratch[1];
	int len = vsnprintf(scratch, 1, msg, args);
	va_end(args);

	char *buffer = (char *) ulang_alloc(len + 1);
	va_start(args, msg);
	vsnprintf(buffer, len + 1, msg, args);
	va_end(args);

	error->file = file;
	error->message.data = buffer;
	error->message.length = len;
	error->span = *span;
	error->is_set = UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE void ulang_error_free(ulang_error *error) {
	if (error->is_set) ulang_free(error->message.data);
	error->is_set = UL_FALSE;
	error->file = NULL;
	error->message.data = NULL;
	error->message.length = 0;
}

static void ulang_file_get_lines(ulang_file *file) {
	if (file->lines != NULL) return;

	char *data = file->data;
	size_t dataLength = file->length;
	size_t numLines = 0;
	for (size_t i = 0, n = dataLength; i < n; i++) {
		uint8_t c = data[i];
		if (c == '\n') numLines++;
	}
	if (dataLength > 0) numLines++;
	file->numLines = numLines;
	file->lines = ulang_alloc(sizeof(ulang_line) * (numLines + 2));

	uint32_t lineStart = 0;
	size_t addedLines = 0;
	for (size_t i = 0, n = dataLength; i < n; i++) {
		uint8_t c = data[i];
		if (c == '\n') {
			ulang_line line = {{data + lineStart, i - lineStart}, (uint32_t) addedLines + 1};
			file->lines[addedLines + 1] = line;
			lineStart = (uint32_t) i + 1;
			addedLines++;
		}
	}

	if (addedLines < numLines) {
		ulang_line line = {{data + lineStart, file->length - lineStart}, addedLines + 1};
		file->lines[addedLines + 1] = line;
	}
}

EMSCRIPTEN_KEEPALIVE void ulang_error_print(ulang_error *error) {
	ulang_file *source = error->file;
	ulang_file_get_lines(source);
	ulang_line *line = &source->lines[error->span.startLine];

	printf("Error (%.*s:%i): %.*s\n", (int) source->fileName.length, source->fileName.data, line->lineNumber,
		   (int) error->message.length,
		   error->message.data);

	if (line->data.length > 0) {
		printf("%.*s\n", (int) line->data.length, line->data.data);
		int32_t errorStart = (int32_t) (error->span.data.data - line->data.data);
		int32_t errorEnd = errorStart + (int32_t) error->span.data.length - 1;
		for (int32_t i = 0, n = (int32_t) line->data.length; i < n; i++) {
			ulang_bool useTab = line->data.data[i] == '\t';
			// BOZO non ASCII characters use > 1 bytes, so the ^ doesn't match up. E.g. 'ö' > '^^'
			printf("%s", i >= errorStart && i <= errorEnd ? "^" : (useTab ? "\t" : " "));
		}
		printf("\n");
	}
}

static char *string_concat(char *a, size_t lena, char *b, size_t lenb, size_t *newLen) {
	char *str = (char *)ulang_alloc(lena + lenb + 1);
	memcpy(str, a, lena);
	memcpy(str + lena, b, lenb);
	str[lena + lenb] = 0;
	if (a != NULL) ulang_free(a);
	*newLen = lena + lenb;
	return str;
}

ulang_bool ulang_string_equals(ulang_string *a, ulang_string *b) {
	if (a->length != b->length) return UL_FALSE;
	char *aData = a->data;
	char *bData = b->data;
	size_t length = a->length;
	for (size_t i = 0; i < length; i++) {
		if (aData[i] != bData[i]) return UL_FALSE;
	}
	return UL_TRUE;
}

ulang_bool ulang_span_matches(ulang_span *span, const char *needle, size_t length) {
	if (span->data.length != length) return UL_FALSE;

	const char *sourceData = span->data.data;
	for (uint32_t i = 0; i < length; i++) {
		if (sourceData[i] != needle[i]) return UL_FALSE;
	}
	return UL_TRUE;
}

typedef struct {
	ulang_file *data;
	uint32_t index;
	uint32_t end;
	uint32_t line;
} char_stream;

static void char_stream_init(char_stream *stream, ulang_file *fileData) {
	stream->data = fileData;
	stream->index = 0;
	stream->line = 1;
	stream->end = (uint32_t) fileData->length;
}

static uint32_t next_utf8_character(const char *data, uint32_t *index, uint32_t end) {
	static const uint32_t utf8Offsets[6] = {
			0x00000000UL, 0x00003080UL, 0x000E2080UL,
			0x03C82080UL, 0xFA082080UL, 0x82082080UL
	};

	uint32_t character = 0;
	int sz = 0;
	do {
		character <<= 6;
		character += data[(*index)++];
		sz++;
	} while (*index != end && ((data[*index]) & 0xC0) == 0x80);
	character -= utf8Offsets[sz - 1];

	return character;
}

static ulang_bool char_stream_has_more(char_stream *stream) {
	return stream->index < stream->data->length;
}

static uint32_t char_stream_consume(char_stream *stream) {
	return next_utf8_character(stream->data->data, &stream->index, stream->end);
}

static ulang_bool char_stream_match(char_stream *stream, const char *needleData, ulang_bool consume) {
	uint32_t needleLength = 0;
	const char *sourceData = stream->data->data;
	for (uint32_t i = 0, j = stream->index; needleData[i] != 0; i++, needleLength++) {
		if (j >= stream->end) return UL_FALSE;
		uint32_t c = next_utf8_character(sourceData, &j, stream->end);
		if ((unsigned char) needleData[i] != c) return UL_FALSE;
	}
	if (consume) stream->index += needleLength;
	return UL_TRUE;
}

static ulang_bool char_stream_match_digit(char_stream *stream, ulang_bool consume) {
	if (!char_stream_has_more(stream)) return UL_FALSE;
	char c = stream->data->data[stream->index];
	if (c >= '0' && c <= '9') {
		if (consume) stream->index++;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool char_stream_match_hex(char_stream *stream) {
	if (!char_stream_has_more(stream)) return UL_FALSE;
	char c = stream->data->data[stream->index];
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
		stream->index++;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool char_stream_match_identifier_start(char_stream *stream) {
	if (!char_stream_has_more(stream)) return UL_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx, stream->end);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xc0) {
		stream->index = idx;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool char_stream_match_identifier_part(char_stream *stream) {
	if (!char_stream_has_more(stream)) return UL_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx, stream->end);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9') || c >= 0x80) {
		stream->index = idx;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static void char_stream_skip_white_space(char_stream *stream) {
	const char *sourceData = stream->data->data;
	while (UL_TRUE) {
		if (stream->index >= stream->end) return;
		char c = sourceData[stream->index];
		switch (c) {
			case '#': {
				while (stream->index < stream->end && c != '\n') {
					c = sourceData[stream->index];
					stream->index++;
				}
				stream->line++;
				continue;
			}
			case ' ':
			case '\r':
			case '\t': {
				stream->index++;
				continue;
			}
			case '\n': {
				stream->index++;
				stream->line++;
				continue;
			}
			default:
				return;
		}
	}
}

static void char_stream_start_span_inplace(char_stream *stream, ulang_span *span) {
	span->endLine = stream->index;
	span->startLine = stream->line;
}

static ulang_span *char_stream_end_span_inplace(char_stream *stream, ulang_span *span) {
	span->data.data = stream->data->data + span->endLine;
	span->data.length = stream->index - span->endLine;
	span->endLine = stream->line;
	return span;
}

static const char *token_type_to_string(token_type type) {
	switch (type) {
		case TOKEN_INTEGER:
			return "int";
		case TOKEN_FLOAT:
			return "float";
		case TOKEN_STRING:
			return "string";
		case TOKEN_IDENTIFIER:
			return "identifier";
		case TOKEN_SPECIAL_CHAR:
			return "special char";
		case TOKEN_EOF:
			return "end of file";
		default:
			return "unknown";
	}
}

static reg *token_matches_register(token *token) {
	ulang_span *span = &token->span;
	for (size_t i = 0; i < (sizeof(registers) / sizeof(reg)); i++) {
		if (ulang_span_matches(span, registers[i].name.data, registers[i].name.length)) {
			return &registers[i];
		}
	}
	return NULL;
}

static opcode *token_matches_opcode(token *token) {
	ulang_span *span = &token->span;
	for (size_t i = 0; i < opcodeLength; i++) {
		if (ulang_span_matches(span, opcodes[i].name.data, opcodes[i].name.length)) {
			return &opcodes[i];
		}
	}
	return NULL;
}

int token_to_int(token *token) {
	ulang_string *str = &token->span.data;
	char c = str->data[str->length];
	str->data[str->length] = 0;
	int val = (int) strtoll(str->data, NULL, 0);
	str->data[str->length] = c;
	return val;
}

static float token_to_float(token *token) {
	ulang_string *str = &token->span.data;
	char c = str->data[str->length];
	str->data[str->length] = 0;
	float val = strtof(str->data, NULL);
	str->data[str->length] = c;
	return val;
}

static ulang_bool tokenize(ulang_file *file, token_array *tokens, ulang_error *error) {
	char_stream stream;
	char_stream_init(&stream, file);

	while (char_stream_has_more(&stream)) {
		char_stream_skip_white_space(&stream);
		if (!char_stream_has_more(&stream)) break;
		token_type type;
		ulang_span span;
		char_stream_start_span_inplace(&stream, &span);

		if (char_stream_match_digit(&stream, UL_FALSE)) {
			type = TOKEN_INTEGER;
			if (char_stream_match(&stream, "0x", UL_TRUE)) {
				while (char_stream_match_hex(&stream));
			} else {
				while (char_stream_match_digit(&stream, UL_TRUE));
				if (char_stream_match(&stream, ".", UL_TRUE)) {
					type = TOKEN_FLOAT;
					while (char_stream_match_digit(&stream, UL_TRUE));
				}
			}
			if (char_stream_match(&stream, "b", UL_TRUE)) {
				if (type == TOKEN_FLOAT) {
					ulang_error_init(error, stream.data, char_stream_end_span_inplace(&stream, &span), "Byte literal can not have a decimal point.");
					return UL_FALSE;
				}
				type = TOKEN_INTEGER;
			}
			char_stream_end_span_inplace(&stream, &span);
			token_array_add(tokens, (token) {span, type});
			continue;
		}

		// String literal
		if (char_stream_match(&stream, "\"", UL_TRUE)) {
			ulang_bool matchedEndQuote = UL_FALSE;
			type = TOKEN_STRING;
			while (char_stream_has_more(&stream)) {
				if (char_stream_match(&stream, "\\", UL_TRUE)) {
					char_stream_consume(&stream);
				}
				if (char_stream_match(&stream, "\"", UL_TRUE)) {
					matchedEndQuote = UL_TRUE;
					break;
				}
				if (char_stream_match(&stream, "\n", UL_FALSE)) {
					ulang_error_init(error, stream.data, char_stream_end_span_inplace(&stream, &span),
									 "String literal is not closed by double quote");
					return UL_FALSE;
				}
				char_stream_consume(&stream);
			}
			if (!matchedEndQuote) {
				ulang_error_init(error, stream.data, char_stream_end_span_inplace(&stream, &span), "String literal is not closed by double quote");
				return UL_FALSE;
			}
			char_stream_end_span_inplace(&stream, &span);
			token_array_add(tokens, (token) {span, type});
			continue;
		}

		// Identifier or keyword
		if (char_stream_match_identifier_start(&stream)) {
			while (char_stream_match_identifier_part(&stream));
			type = TOKEN_IDENTIFIER;
			char_stream_end_span_inplace(&stream, &span);
			token_array_add(tokens, (token) {span, type});
			continue;
		}

		// Else check for "simple" tokens made up of
		// 1 character literals, like ".", ",", or ";",
		char_stream_consume(&stream);
		type = TOKEN_SPECIAL_CHAR;
		char_stream_end_span_inplace(&stream, &span);
		token_array_add(tokens, (token) {span, type});
	}
	return UL_TRUE;
}

static ulang_bool token_stream_has_more(token_stream *stream) {
	return stream->index < stream->tokens->size;
}

static token *token_stream_consume(token_stream *stream) {
	if (!token_stream_has_more(stream)) return NULL;
	return &stream->tokens->items[stream->index++];
}

static token *token_stream_peek(token_stream *stream) {
	if (!token_stream_has_more(stream)) return NULL;
	return &stream->tokens->items[stream->index];
}

static token *token_stream_match(token_stream *stream, token_type type, ulang_bool consume) {
	if (stream->index >= stream->tokens->size) return UL_FALSE;
	token *token = &stream->tokens->items[stream->index];
	if (token->type == type) {
		if (consume) stream->index++;
		return token;
	}
	return NULL;
}

static token *token_stream_match_string(token_stream *stream, const char *text, size_t len, ulang_bool consume) {
	if (stream->index >= stream->tokens->size) return UL_FALSE;
	token *token = &stream->tokens->items[stream->index];
	if (ulang_span_matches(&token->span, text, len)) {
		if (consume) stream->index++;
		return token;
	}
	return NULL;
}

static token *
token_stream_expect(token_stream *stream, token_type type, ulang_error *error) {
	if (!token_stream_match(stream, type, UL_TRUE)) {
		token *lastToken = stream->index < stream->tokens->size ? &stream->tokens->items[stream->index] : NULL;

		if (lastToken == NULL) {
			ulang_file_get_lines(stream->file);
			ulang_line *lastLine = &stream->file->lines[stream->file->numLines];
			ulang_span span = (ulang_span) {lastLine->data, lastLine->lineNumber, lastLine->lineNumber};
			ulang_error_init(error, stream->file, &span, "Expected a '%s' token, but reached the end of the source.",
							 token_type_to_string(type));
			return NULL;
		} else {
			ulang_error_init(error, stream->file, &lastToken->span, "Expected a '%s' token, but got a '%.*s' token", token_type_to_string(type),
							 lastToken->span.data.length, lastToken->span.data.data);
		}
		return NULL;
	} else {
		return &stream->tokens->items[stream->index - 1];
	}
}

static token *
token_stream_expect_string(token_stream *stream, char *str, size_t len, const char *message, ulang_error *error) {
	if (!token_stream_match_string(stream, str, len, UL_TRUE)) {
		token *lastToken = stream->index < stream->tokens->size ? &stream->tokens->items[stream->index] : NULL;

		if (lastToken == NULL) {
			ulang_file_get_lines(stream->file);
			ulang_line *lastLine = &stream->file->lines[stream->file->numLines];
			ulang_span span = (ulang_span) {lastLine->data, lastLine->lineNumber, lastLine->lineNumber};
			ulang_error_init(error, stream->file, &span, "Expected '%.*s' %s, but reached the end of the source.",
							 len, str, message);
			return NULL;
		} else {
			ulang_error_init(error, stream->file, &lastToken->span, "Expected '%.*s' %s, but got '%.*s'", len, str, message,
							 lastToken->span.data.length, lastToken->span.data.data);
		}
		return NULL;
	} else {
		return &stream->tokens->items[stream->index - 1];
	}
}

static ulang_string unaryOperators[] = {STR_OBJ("~"), STR_OBJ("+"), STR_OBJ("-"), {0}};

static ulang_string binaryOperators[][4] = {
		{STR_OBJ("|"), STR_OBJ("&"), STR_OBJ("^"), {0}},
		{STR_OBJ("+"), STR_OBJ("-"), {0}},
		{STR_OBJ("/"), STR_OBJ("*"), STR_OBJ("%"), {0}}
};

static ulang_bool parse_expression(compiler_context *ctx, expression_value *value, ulang_span *span);

static ulang_bool parse_unary_operator(compiler_context *ctx, expression_value *value) {
	ulang_string *op = unaryOperators;
	while (op->data) {
		if (token_stream_match_string(&ctx->stream, op->data, op->length, UL_FALSE)) break;
		op++;
	}
	if (op->data) {
		token *opToken = token_stream_consume(&ctx->stream);
		expression_value exprValue = { 0 };
		if (!parse_unary_operator(ctx, &exprValue) || ctx->error->is_set) return UL_FALSE;
		value->unresolved = exprValue.unresolved;
		switch (opToken->span.data.data[0]) {
			case '~':
				if (exprValue.type == UL_FLOAT) {
					ulang_error_init(ctx->error, ctx->stream.file, &opToken->span, "Operator ~ can not be used with float values.");
					return UL_FALSE;
				}
				value->type = UL_INTEGER;
				value->i = ~exprValue.i;
				value->f = (float) value->i;
				break;
			case '+':
				if (exprValue.type == UL_INTEGER) {
					value->type = UL_INTEGER;
					value->i = exprValue.i;
					value->f = (float) value->i;
				} else {
					value->type = UL_FLOAT;
					value->f = exprValue.f;
				}
				break;
			case '-':
				if (exprValue.type == UL_INTEGER) {
					value->type = UL_INTEGER;
					value->i = -exprValue.i;
					value->f = (float) value->i;
				} else {
					value->type = UL_FLOAT;
					value->f = -exprValue.f;
				}
				break;
		}
		return UL_TRUE;
	} else {
		if (token_stream_match_string(&ctx->stream, STR("("), UL_TRUE)) {
			if (!parse_expression(ctx, value, NULL) || ctx->error->is_set) return UL_FALSE;
			if (!token_stream_expect_string(&ctx->stream, STR(")"), "to close parenthesized expression", ctx->error)) return UL_FALSE;
			return UL_TRUE;
		} else {
			token *literal = token_stream_consume(&ctx->stream);
			if (!literal) {
				literal = &ctx->stream.tokens->items[ctx->stream.tokens->size - 1];
				ulang_error_init(ctx->error, ctx->stream.file, &literal->span, "Expected an integer, a float, a constant, or a label.");
				return UL_FALSE;
			}
			switch (literal->type) {
				case TOKEN_INTEGER:
					value->type = UL_INTEGER;
					value->i = token_to_int(literal);
					value->f = (float) value->i;
					return UL_TRUE;
				case TOKEN_FLOAT:
					value->type = UL_FLOAT;
					value->f = token_to_float(literal);
					return UL_TRUE;
				case TOKEN_IDENTIFIER:
					// Registers aren't allowed in expressions
					if (token_matches_register(literal)) {
						ulang_error_init(ctx->error, ctx->stream.file, &literal->span, "Registers are not allowed in expressions.");
						return UL_FALSE;
					}

					// Check if we have a constant by that name
					for (size_t i = 0; i < ctx->constants.size; i++) {
						ulang_constant *cnst = &ctx->constants.items[i];
						if (ulang_string_equals(&literal->span.data, &cnst->name.data)) {
							value->type = cnst->type;
							if (value->type == UL_INTEGER) {
								value->i = cnst->i;
								value->f = (float) cnst->i;
							} else {
								value->i = (int)cnst->f;
								value->f = cnst->f;
							}
							return UL_TRUE;
						}
					}
					// Otherwise, we assume it's a label. Depending on the resolution setting in the context,
					// we either postpone resolution, or try to resolve on the spot. The latter happens
					// in ulang_compile when patches and their expressions are resolved.
					if (!ctx->resolveLabelsInExpressions) {
						value->unresolved = UL_TRUE;
						value->type = UL_INTEGER;
						value->i = value->f = 0;
						return UL_TRUE;
					} else {
						ulang_label *label = NULL;
						for (size_t j = 0; j < ctx->labels.size; j++) {
							if (ulang_string_equals(&ctx->labels.items[j].label.data, &literal->span.data)) {
								label = &ctx->labels.items[j];
								break;
							}
						}
						if (!label) {
							ulang_error_init(ctx->error, ctx->stream.file, &literal->span, "Unknown label.");
							return UL_FALSE;
						}

						uint32_t labelAddress = (uint32_t) label->address;
						switch (label->target) {
							case UL_LT_UNINITIALIZED:
								ulang_error_init(ctx->error, ctx->stream.file, &literal->span, "Internal error: Uninitialized label target.");
								return UL_FALSE;
							case UL_LT_CODE:
								break;
							case UL_LT_DATA:
								labelAddress += ctx->code.size;
								break;
							case UL_LT_RESERVED_DATA:
								labelAddress += ctx->code.size + ctx->data.size;
								break;
						}
						value->unresolved = UL_FALSE;
						value->type = UL_INTEGER;
						value->i = (int)labelAddress; // BOZO we aren't going above 2^31 for label addresses.
						value->f = (float)labelAddress;
						return UL_TRUE;
					}
				default:
					ulang_error_init(ctx->error, ctx->stream.file, &literal->span, "Expected an integer, a float, a constant, or a label.");
					return UL_FALSE;
			}
		}
	}
}

static ulang_bool parse_binary_operator(compiler_context *ctx, expression_value *value, uint32_t level) {
	uint32_t nextLevel = level + 1;
	expression_value left = { 0 };
	if (nextLevel == 3) {
		if (!parse_unary_operator(ctx, &left) || ctx->error->is_set) return UL_FALSE;
	} else {
		if (!parse_binary_operator(ctx, &left, nextLevel) || ctx->error->is_set) return UL_FALSE;
	}

	value->unresolved |= left.unresolved;

	while (token_stream_has_more(&ctx->stream)) {
		ulang_string *op = binaryOperators[level];
		while (op->data) {
			if (token_stream_match_string(&ctx->stream, op->data, op->length, UL_FALSE)) break;
			op++;
		}
		if (op->data == NULL) break;

		token *opToken = token_stream_consume(&ctx->stream);
		expression_value right = { 0 };
		if (nextLevel == 3) {
			if (!parse_unary_operator(ctx, &right) || ctx->error->is_set) return UL_FALSE;
		} else {
			if (!parse_binary_operator(ctx, &right, nextLevel) || ctx->error->is_set) return UL_FALSE;
		}

		value->unresolved |= right.unresolved;

		switch (opToken->span.data.data[0]) {
			case '|':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					ulang_error_init(ctx->error, ctx->stream.file, &opToken->span, "Operator | can not be used with float values.");
					return UL_FALSE;
				}
				value->type = UL_INTEGER;
				value->i = left.i | right.i;
				break;
			case '&':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					ulang_error_init(ctx->error, ctx->stream.file, &opToken->span, "Operator & can not be used with float values.");
					return UL_FALSE;
				}
				value->type = UL_INTEGER;
				value->i = left.i & right.i;
				break;
			case '^':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					ulang_error_init(ctx->error, ctx->stream.file, &opToken->span, "Operator ^ can not be used with float values.");
					return UL_FALSE;
				}
				value->type = UL_INTEGER;
				value->i = left.i ^ right.i;
				break;
			case '+':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					value->type = UL_FLOAT;
					value->f = left.f + right.f;
				} else {
					value->type = UL_INTEGER;
					value->i = left.i + right.i;
					value->f = (float) value->i;
				}
				break;
			case '-':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					value->type = UL_FLOAT;
					value->f = left.f - right.f;
				} else {
					value->type = UL_INTEGER;
					value->i = left.i - right.i;
					value->f = (float) value->i;
				}
				break;
			case '*':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					value->type = UL_FLOAT;
					value->f = left.f * right.f;
				} else {
					value->type = UL_INTEGER;
					value->i = left.i * right.i;
					value->f = (float) value->i;
				}
				break;
			case '/':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					value->type = UL_FLOAT;
					value->f = left.f / right.f;
				} else {
					value->type = UL_INTEGER;
					value->i = left.i / right.i;
					value->f = (float) value->i;
				}
				break;
			case '%':
				if (left.type == UL_FLOAT || right.type == UL_FLOAT) {
					ulang_error_init(ctx->error, ctx->stream.file, &opToken->span, "Operator % can not be used with float values.");
					return UL_FALSE;
				} else {
					value->type = UL_INTEGER;
					value->i = left.i % right.i;
					value->f = (float) value->i;
				}
				break;
		}

		left = *value;
	}
	value->type = left.type;
	value->f = left.f;
	value->i = left.i;
	return UL_TRUE;
}

static ulang_bool parse_expression(compiler_context *ctx, expression_value *value, ulang_span *span) {
	value->startToken = ctx->stream.index;
	value->unresolved = UL_FALSE;
	ulang_span *startSpan = &ctx->stream.tokens->items[ctx->stream.index].span;
	ulang_bool result = parse_binary_operator(ctx, value, 0);
	if (result) value->endToken = ctx->stream.index;
	if (span) {
		ulang_span *endSpan = &ctx->stream.tokens->items[ctx->stream.index - 1].span;
		span->data = startSpan->data;
		span->data.length = endSpan->data.data == startSpan->data.data ? startSpan->data.length : endSpan->data.data - startSpan->data.data;
		span->startLine = startSpan->startLine;
		span->endLine = endSpan->endLine;
	}
	return result;
}

static void parse_repeat(compiler_context *ctx, int *numRepeat) {
	if (!token_stream_match_string(&ctx->stream, STR("x"), UL_TRUE)) return;
	ulang_span span;
	expression_value value;
	if (!parse_expression(ctx, &value, &span)) return;
	if (value.type != UL_INTEGER) {
		ulang_error_init(ctx->error, ctx->stream.file, &span, "Expected an integer value after 'x'.");
		return;
	}
	*numRepeat = value.i;
	if (*numRepeat < 0) {
		ulang_error_init(ctx->error, ctx->stream.file, &span, "Number of repetitions can not be negative.");
		return;
	}
}

static void emit_byte(byte_array *code, expression_value *value, int repeat) {
	int val = value->i;
	val &= 0xff;
	for (int i = 0; i < repeat; i++) {
		byte_array_add(code, (uint8_t) val);
	}
}

static void emit_short(byte_array *code, expression_value *value, int repeat) {
	int val = value->i;
	val &= 0xffff;
	byte_array_ensure(code, 2 * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], &val, 2);
		code->size += 2;
	}
}

static void emit_int(byte_array *code, expression_value *value, int repeat) {
	int val = value->i;
	byte_array_ensure(code, 4 * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], &val, 4);
		code->size += 4;
	}
}

static void emit_float(byte_array *code, expression_value *value, int repeat) {
	float val = value->f;
	byte_array_ensure(code, 4 * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], &val, 4);
		code->size += 4;
	}
}

static void emit_string(byte_array *code, token *value, int repeat) {
	ulang_string str = value->span.data;
	str.data++;
	str.length -= 2;

	char *cString = ulang_alloc(str.length);
	size_t i, j;
	for (i = 0, j = 0; i < str.length; i++, j++) {
		char c = str.data[i];
		if (c == '\\') {
			i++;
			if (i == str.length) break;
			c = str.data[i];
			switch (c) {
				case 'n':
					cString[j] = '\n';
					break;
				case 'r':
					cString[j] = '\r';
					break;
				case 't':
					cString[j] = '\t';
					break;
				default:
					cString[j] = c;
					break;
			}
		} else {
			cString[j] = c;
		}
	}

	byte_array_ensure(code, j * repeat);
	for (i = 0; i < (size_t) repeat; i++) {
		memcpy(&code->items[code->size], cString, j);
		code->size += j;
	}
	ulang_free(cString);
}

#define ENCODE_OP(word, op) word |= op
#define ENCODE_REG(word, reg, index) word |= (((reg) & 0xf) << (7 + 4 * (index)))
#define ENCODE_OFF(word, offset) word |= (((offset) & 0x1fff) << 19)

static ulang_bool
emit_op(ulang_file *file, opcode *op, token operands[3], expression_value operandValues[3], patch_array *patches, byte_array *code, ulang_error *error) {
	uint32_t word1 = 0;
	uint32_t word2 = 0;

	ENCODE_OP(word1, op->code);

	int numEmittedRegs = 0;
	for (int i = 0; i < op->numOperands; i++) {
		token *operandToken = &operands[i];
		expression_value *operandValue = &operandValues[i];
		operand_type operandType = op->operands[i];
		switch (operandType) {
			case UL_REG:
				ENCODE_REG(word1, token_matches_register(operandToken)->index, numEmittedRegs);
				numEmittedRegs++;
				break;
			case UL_OFF:
				ENCODE_OFF(word1, operandValue->i);
				break;
			case UL_INT:
			case UL_FLT:
			case UL_LBL_INT:
			case UL_LBL_INT_FLT: {
				if (operandValue->unresolved) {
					patch p;
					p.type = PT_VALUE;
					p.expr = *operandValue;
					p.patchAddress = code->size + 4;
					patch_array_add(patches, p);
					word2 = 0xdeadbeef;
					break;
				}

				switch (operandToken->type) {
					case TOKEN_INTEGER: {
						int value = operandValue->i;
						memcpy(&word2, &value, 4);
						break;
					}
					case TOKEN_FLOAT: {
						float value = operandValue->f;
						memcpy(&word2, &value, 4);
						break;
					}
					default:
						ulang_error_init(error, file, &operandToken->span,
										 "Internal error, unexpected token type for value operand.");
						return UL_FALSE;
				}
				break;
			}
			default:
				ulang_error_init(error, file, &operandToken->span,
								 "Internal error, unknown operand type.");
				return UL_FALSE;
		}
	}

	byte_array_ensure(code, 4 + (op->hasValueOperand ? 4 : 0));
	memcpy(&code->items[code->size], &word1, 4);
	code->size += 4;
	if (op->hasValueOperand) {
		memcpy(&code->items[code->size], &word2, 4);
		code->size += 4;
	}

	return UL_TRUE;
}

static void set_label_targets(label_array *labels, ulang_label_target target, size_t address) {
	for (int i = (int) labels->size - 1; i >= 0; i--) {
		if (labels->items[i].target != UL_LT_UNINITIALIZED) break;
		labels->items[i].target = target;
		labels->items[i].address = address;
	}
}

EMSCRIPTEN_KEEPALIVE ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error) {
	error->is_set = UL_FALSE;
	init_opcodes_and_registers();

	compiler_context ctx = { .error = error };
	ctx.resolveLabelsInExpressions = UL_FALSE;

	// tokenize
	token_array_init_inplace(&ctx.tokens, MAX(16, file->length / 100));
	if (!tokenize(file, &ctx.tokens, error)) {
		token_array_free_inplace(&ctx.tokens);
		return UL_FALSE;
	}
	ctx.stream = (token_stream){file, &ctx.tokens, 0};
	patch_array_init_inplace(&ctx.patches, 16);
	label_array_init_inplace(&ctx.labels, 16);
	constant_array_init_inplace(&ctx.constants, 16);
	byte_array_init_inplace(&ctx.code, 16);
	byte_array_init_inplace(&ctx.data, 16);
	int_array_init_inplace(&ctx.addressToLine, 16);

	while (token_stream_has_more(&ctx.stream)) {
		token *tok = token_stream_consume(&ctx.stream);
		opcode *op = token_matches_opcode(tok);
		if (!op) {
			if (tok->type != TOKEN_IDENTIFIER) {
				ulang_error_init(error, file, &tok->span, "Expected a label, data, or an instruction.");
				goto _compilation_error;
			}

			if (ulang_span_matches(&tok->span, STR("byte"))) {
				while (-1) {
					token *value;
					if ((value = token_stream_match(&ctx.stream, TOKEN_STRING, UL_TRUE))) {
						int numRepeat = 1;
						parse_repeat(&ctx, &numRepeat);
						if (error->is_set) goto _compilation_error;
						set_label_targets(&ctx.labels, UL_LT_DATA, ctx.data.size);
						emit_string(&ctx.data, value, numRepeat);
					} else {
						expression_value exprValue;
						ulang_span span = {0};
						if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
						if (exprValue.type != UL_INTEGER) {
							ulang_error_init(error, file, &span, "Expression must evaluate to an integer value.");
							goto _compilation_error;
						}
						if (exprValue.unresolved) {
							ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
							goto _compilation_error;
						}
						int numRepeat = 1;
						parse_repeat(&ctx, &numRepeat);
						if (error->is_set) goto _compilation_error;
						set_label_targets(&ctx.labels, UL_LT_DATA, ctx.data.size);
						emit_byte(&ctx.data, &exprValue, numRepeat);
					}
					if (!token_stream_match_string(&ctx.stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (ulang_span_matches(&tok->span, STR("short"))) {
				while (-1) {
					expression_value exprValue;
					ulang_span span = {0};
					if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
					if (exprValue.type != UL_INTEGER) {
						ulang_error_init(error, file, &span, "Expression must evaluate to an integer value.");
						goto _compilation_error;
					}
					if (exprValue.unresolved) {
						ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
						goto _compilation_error;
					}
					int numRepeat = 1;
					parse_repeat(&ctx, &numRepeat);
					if (error->is_set) goto _compilation_error;
					set_label_targets(&ctx.labels, UL_LT_DATA, ctx.data.size);
					emit_short(&ctx.data, &exprValue, numRepeat);
					if (!token_stream_match_string(&ctx.stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (ulang_span_matches(&tok->span, STR("int"))) {
				while (-1) {
					expression_value exprValue;
					ulang_span span = {0};
					if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
					if (exprValue.type != UL_INTEGER) {
						ulang_error_init(error, file, &span, "Expression must evaluate to an integer value.");
						goto _compilation_error;
					}
					if (exprValue.unresolved) {
						ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
						goto _compilation_error;
					}
					int numRepeat = 1;
					parse_repeat(&ctx, &numRepeat);
					if (error->is_set) goto _compilation_error;
					set_label_targets(&ctx.labels, UL_LT_DATA, ctx.data.size);
					emit_int(&ctx.data, &exprValue, numRepeat);
					if (!token_stream_match_string(&ctx.stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (ulang_span_matches(&tok->span, STR("float"))) {
				while (-1) {
					expression_value exprValue;
					ulang_span span = {0};
					if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
					if (exprValue.type != UL_FLOAT) {
						ulang_error_init(error, file, &span, "Expression must evaluate to a float value.");
						goto _compilation_error;
					}
					if (exprValue.unresolved) {
						ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
						goto _compilation_error;
					}
					int numRepeat = 1;
					parse_repeat(&ctx, &numRepeat);
					if (error->is_set) goto _compilation_error;
					set_label_targets(&ctx.labels, UL_LT_DATA, ctx.data.size);
					emit_float(&ctx.data, &exprValue, numRepeat);
					if (!token_stream_match_string(&ctx.stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (ulang_span_matches(&tok->span, STR("reserve"))) {
				size_t typeSize = 0;
				if (token_stream_match_string(&ctx.stream, STR("byte"), UL_TRUE)) typeSize = 1;
				else if (token_stream_match_string(&ctx.stream, STR("short"), UL_TRUE)) typeSize = 2;
				else if (token_stream_match_string(&ctx.stream, STR("int"), UL_TRUE)) typeSize = 4;
				else if (token_stream_match_string(&ctx.stream, STR("float"), UL_TRUE)) typeSize = 4;
				else {
					token *token = token_stream_consume(&ctx.stream);
					if (!token) token = &ctx.stream.tokens->items[ctx.stream.index - 1];
					ulang_error_init(error, file, &token->span, "Expected byte, short, int, or float.");
					goto _compilation_error;
				}

				if (!token_stream_expect_string(&ctx.stream, STR("x"), "after 'reserve <type>'", error)) goto _compilation_error;
				expression_value exprValue;
				ulang_span span = {0};
				if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
				if (exprValue.type != UL_INTEGER) {
					ulang_error_init(error, file, &span, "Expression must evaluate to an integer value.");
					goto _compilation_error;
				}
				if (exprValue.unresolved) {
					ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
					goto _compilation_error;
				}

				size_t numBytes = typeSize * exprValue.i;
				if (numBytes <= 0) {
					ulang_error_init(error, file, &span, "Number of reserved bytes must be > 0.");
					goto _compilation_error;
				}
				set_label_targets(&ctx.labels, UL_LT_RESERVED_DATA, ctx.numReservedBytes);
				ctx.numReservedBytes += numBytes;
				continue;
			}

			if (ulang_span_matches(&tok->span, STR("const"))) {
				token *name = token_stream_expect(&ctx.stream, TOKEN_IDENTIFIER, ctx.error);
				if (!name) goto _compilation_error;
				expression_value exprValue;
				ulang_span span = {0};
				if (!parse_expression(&ctx, &exprValue, &span)) goto _compilation_error;
				if (exprValue.unresolved) {
					ulang_error_init(error, file, &span, "Expression either contains an undefined constant, or a label.");
					goto _compilation_error;
				}
				ulang_constant constant = { exprValue.type };
				constant.name = name->span;
				if (exprValue.type == UL_INTEGER) constant.i = exprValue.i;
				else constant.f = exprValue.f;
				constant_array_add(&ctx.constants, constant);
				continue;
			}

			// Otherwise, we must have a label
			if (!token_stream_expect_string(&ctx.stream, STR(":"), "after label", error)) goto _compilation_error;
			ulang_label label = {tok->span, UL_LT_UNINITIALIZED, 0};
			label_array_add(&ctx.labels, label);
		} else {
			token operands[3];
			expression_value operandExpressions[3];
			for (int i = 0; i < op->numOperands; i++) {
				token *operand = token_stream_match(&ctx.stream, TOKEN_IDENTIFIER, UL_TRUE);
				if (operand && token_matches_register(operand)) {
					operands[i] = *operand;
					operandExpressions[i] = (expression_value) {0};
				} else {
					if (operand) ctx.stream.index--;
					if (!parse_expression(&ctx, &operandExpressions[i], &operands[i].span)) goto _compilation_error;
					operands[i].type = operandExpressions[i].type == UL_FLOAT ? TOKEN_FLOAT : TOKEN_INTEGER;
				}

				if (i < op->numOperands - 1) {
					if (!token_stream_expect_string(&ctx.stream, STR(","), "before the next argument", error)) goto _compilation_error;
				}
			}

			opcode *firstOp = op;
			opcode *fittingOp = NULL;
			while (-1) {
				fittingOp = op;
				for (int i = 0; i < op->numOperands; i++) {
					token *operand = &operands[i];
					operand_type operandType = op->operands[i];
					reg *r = token_matches_register(operand);
					if (operandType == UL_REG) {
						if (!r) {
							if (!error->is_set) ulang_error_init(error, file, &operand->span, "Expected a register");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_LBL_INT_FLT) {
						if (!(operand->type == TOKEN_INTEGER ||
							  operand->type == TOKEN_FLOAT ||
							  operand->type == TOKEN_IDENTIFIER) || r) {
							if (!error->is_set)
								ulang_error_init(error, file, &operand->span, "Expected an int, float, or a label");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_LBL_INT) {
						if (!(operand->type == TOKEN_INTEGER ||
							  operand->type == TOKEN_IDENTIFIER) || r) {
							if (!error->is_set)
								ulang_error_init(error, file, &operand->span, "Expected an int or a label");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_OFF || operandType == UL_INT) {
						if (operand->type != TOKEN_INTEGER) {
							if (!error->is_set) ulang_error_init(error, file, &operand->span, "Expected an int");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_FLT) {
						if (operand->type != TOKEN_FLOAT) {
							// convert ints to float
							if (operand->type == TOKEN_INTEGER) {
								operand->type = TOKEN_FLOAT;
							} else {
								if (!error->is_set) ulang_error_init(error, file, &operand->span, "Expected a float");
								fittingOp = NULL;
							}
							break;
						}
					} else {
						if (!error->is_set)
							ulang_error_init(error, file, &operand->span, "Internal error, unknown operand type.");
					}
				}
				if (fittingOp) break;
				if (op->index + 1 == opcodeLength) break;
				if (!ulang_string_equals(&op->name, &opcodes[op->index + 1].name)) break;
				op = &opcodes[op->index + 1];
			}
			if (!fittingOp && firstOp != op) {
				token *lastToken = &ctx.tokens.items[ctx.stream.index - 1];
				ulang_span span = {0};
				span.startLine = tok->span.startLine;
				span.endLine = lastToken->span.endLine;
				span.data.data = tok->span.data.data;
				span.data.length = lastToken->span.data.data - tok->span.data.data + 1;
				if (error->is_set) ulang_error_free(error);
				char *alternatives = NULL;
				size_t len = 0;
				op = firstOp;
				while (-1) {
					alternatives = string_concat(alternatives, len, op->name.data, op->name.length, &len);
					alternatives = string_concat(alternatives, len, STR(" "), &len);
					for (int i = 0; i < op->numOperands; i++) {
						switch(op->operands[i]) {
							case UL_NIL:
								break;
							case UL_REG:
								alternatives = string_concat(alternatives, len, STR("<register>"), &len);
								break;
							case UL_LBL_INT:
								alternatives = string_concat(alternatives, len, STR("<label|int>"), &len);
								break;
							case UL_LBL_INT_FLT:
								alternatives = string_concat(alternatives, len, STR("<label|int|float>"), &len);
								break;
							case UL_INT:
								alternatives = string_concat(alternatives, len, STR("<int>"), &len);
								break;
							case UL_FLT:
								alternatives = string_concat(alternatives, len, STR("<float>"), &len);
								break;
							case UL_OFF:
								alternatives = string_concat(alternatives, len, STR("<offset>"), &len);
								break;
						}
						if (i < op->numOperands - 1) alternatives = string_concat(alternatives, len, STR(", "), &len);
					}
					alternatives = string_concat(alternatives, len, STR("\n"), &len);
					if (!ulang_string_equals(&op->name, &opcodes[op->index + 1].name)) break;
					op = &opcodes[op->index + 1];
				}
				ulang_error_init(ctx.error, ctx.stream.file, &span, "No matching instructions for the given argument types. Possible alternatives:\n%s", alternatives);
				ulang_free(alternatives);
				goto _compilation_error;
			}
			if (error->is_set) ulang_error_free(error);

			set_label_targets(&ctx.labels, UL_LT_CODE, ctx.code.size);
			int_array_add(&ctx.addressToLine, tok->span.startLine);
			if (fittingOp->hasValueOperand) int_array_add(&ctx.addressToLine, tok->span.startLine);
			if (!emit_op(file, fittingOp, operands, operandExpressions, &ctx.patches, &ctx.code, error)) goto _compilation_error;
		}
	}

	ctx.resolveLabelsInExpressions = UL_TRUE;
	for (size_t i = 0; i < ctx.patches.size; i++) {
		patch *p = &ctx.patches.items[i];
		ctx.stream.index = p->expr.startToken;
		ulang_span span;
		expression_value expr;
		if (!parse_expression(&ctx, &expr, &span)) goto _compilation_error;

		if (p->type == PT_VALUE) {
			if (p->expr.type == UL_INTEGER) memcpy(&ctx.code.items[p->patchAddress], &expr.i, 4);
			else memcpy(&ctx.code.items[p->patchAddress], &expr.f, 4);
		} else {
			ulang_error_init(ctx.error, ctx.stream.file, &span, "Labels in offsets not supported.");
		}
	}

	patch_array_free_inplace(&ctx.patches);
	token_array_free_inplace(&ctx.tokens);
	program->code = ctx.code.items;
	program->codeLength = ctx.code.size;
	program->data = ctx.data.items;
	program->dataLength = ctx.data.size;
	program->reservedBytes = ctx.numReservedBytes;
	program->labels = ctx.labels.items;
	program->labelsLength = ctx.labels.size;
	program->constants = ctx.constants.items;
	program->constantsLength = ctx.constants.size;
	program->addressToLine = ctx.addressToLine.items;
	program->addressToLineLength = ctx.addressToLine.size;
	program->file = file;
	return UL_TRUE;

	_compilation_error:
	patch_array_free_inplace(&ctx.patches);
	label_array_free_inplace(&ctx.labels);
	constant_array_free_inplace(&ctx.constants);
	byte_array_free_inplace(&ctx.code);
	byte_array_free_inplace(&ctx.data);
	int_array_free_inplace(&ctx.addressToLine);
	token_array_free_inplace(&ctx.tokens);
	return UL_FALSE;
}

EMSCRIPTEN_KEEPALIVE void ulang_program_free(ulang_program *program) {
	ulang_free(program->code);
	ulang_free(program->data);
	ulang_free(program->labels);
	ulang_free(program->constants);
	ulang_free(program->addressToLine);
}

ulang_bool ulang_vm_debug(ulang_vm *vm) {
	do {
		ulang_vm_print(vm);
		prompt:
		printf("> ");

		char buffer[256];
		token_array tokens = {0};
		ulang_error error = {0};
		token_array_init_inplace(&tokens, 5);

		if (!fgets(buffer, 256, stdin)) continue;
		ulang_file input = (ulang_file) {{"input", 5}, buffer, strlen(buffer)};
		if (!tokenize(&input, &tokens, &error)) {
			ulang_error_print(&error);
			ulang_error_free(&error);
			token_array_free_inplace(&tokens);
			goto prompt;
		}

		if (tokens.size == 0) {
			token_array_free_inplace(&tokens);
			goto prompt;
		}

		token_stream stream = {&input, &tokens, 0};
		token *cmd = token_stream_consume(&stream);
		compiler_context ctx = {
				.stream = { stream.file, stream.tokens, stream.index },
				.error = &error,
				.tokens = { tokens.size, tokens.capacity, tokens.items },
		};

		if (ulang_span_matches(&cmd->span, STR("h"))) {
			printf("   s                         step one instruction\n");
			printf("   c                         continue execution\n");
			printf("   r <addr> <num> <b|i|f>?   read <num> words starting at address <addr>\n");
			printf("                             (b)yte, (i)nt, and (f)loat specify the word type\n");
			printf("   w <num> <addr> <b|i|f>?   write the word <num> to address <addr>\n");
			printf("                             (b)yte, (i)nt, and (f)loat specify the word type\n");
			printf("   l <label>?                 print the address of all labels, or the specified <label>\n");
			printf("   p                         print the registers and stack\n");
			token_array_free_inplace(&tokens);
			goto prompt;
		}

		if (ulang_span_matches(&cmd->span, STR("s"))) {
			if (!ulang_vm_step(vm)) {
				token_array_free_inplace(&tokens);
				return UL_FALSE;
			}
			continue;
		}

		if (ulang_span_matches(&cmd->span, STR("c"))) {
			token_array_free_inplace(&tokens);
			return UL_TRUE;
		}

		if (ulang_span_matches(&cmd->span, STR("p"))) {
			token_array_free_inplace(&tokens);
			continue;
		}

		if (ulang_span_matches(&cmd->span, STR("r"))) {
			ulang_span span;
			expression_value addr;
			if (!parse_expression(&ctx, &addr, &span)) {
				ulang_error_print(&error);
				ulang_error_free(&error);
				token_array_free_inplace(&tokens);
				goto prompt;
			}
			if (addr.type != UL_INTEGER) {
				printf("Error: <addr> must be an integer value.\n");
				token_array_free_inplace(&tokens);
				goto prompt;
			}
			expression_value numWords;
			if (!parse_expression(&ctx, &numWords, &span)) {
				ulang_error_print(&error);
				ulang_error_free(&error);
				token_array_free_inplace(&tokens);
				goto prompt;
			}
			if (numWords.type != UL_INTEGER) {
				printf("Error: <num> must be an integer value.\n");
				token_array_free_inplace(&tokens);
				goto prompt;
			}

			int type = 1; // 0 = byte, 1 = int, 2 = float
			if (token_stream_match_string(&stream, STR("b"), UL_TRUE)) type = 0;
			else if (token_stream_match_string(&stream, STR("i"), UL_TRUE)) type = 1;
			else if (token_stream_match_string(&stream, STR("f"), UL_TRUE)) type = 2;

			uint8_t *mem = vm->memory + addr.i;
			for (int i = 0; i < numWords.i; i++) {
				ulang_value val = {0};
				switch (type) {
					case 0:
						memcpy(&val, mem, 1);
						printf("mem[%p]: 0x%x %i\n", (void *) (mem - vm->memory), val.i, val.i);
						mem += 1;
						break;
					case 1:
						memcpy(&val, mem, 4);
						printf("mem[%p]: 0x%x %i\n", (void *) (mem - vm->memory), val.i, val.i);
						mem += 4;
						break;
					case 2:
						memcpy(&val, mem, 4);
						printf("mem[%p]: %f\n", (void *) (mem - vm->memory), val.f);
						mem += 4;
						break;
				}
			}
			token_array_free_inplace(&tokens);
			goto prompt;
		}

		if (ulang_span_matches(&cmd->span, STR("w"))) {
			ulang_span span;
			expression_value num;
			if (!parse_expression(&ctx, &num, &span)) {
				ulang_error_print(&error);
				ulang_error_free(&error);
				token_array_free_inplace(&tokens);
				goto prompt;
			}
			if (num.type == UL_FLOAT) memcpy(&num.i, &num.f, 4);
			expression_value addr;
			if (!parse_expression(&ctx, &addr, &span)) {
				ulang_error_print(&error);
				ulang_error_free(&error);
				token_array_free_inplace(&tokens);
				goto prompt;
			}
			if (addr.type != UL_INTEGER) {
				printf("Error: <addr> must be an integer value.\n");
				token_array_free_inplace(&tokens);
				goto prompt;
			}

			int type = 1; // 0 = byte, 1 = int, 2 = float
			if (token_stream_match_string(&stream, STR("b"), UL_TRUE)) type = 0;
			else if (token_stream_match_string(&stream, STR("i"), UL_TRUE)) type = 1;
			else if (token_stream_match_string(&stream, STR("f"), UL_TRUE)) type = 2;

			uint8_t *mem = vm->memory + addr.i;
			switch (type) {
				case 0:
					memcpy(mem, &num.i, 1);
					break;
				default:
					memcpy(mem, &num.i, 4);
					break;
			}
			token_array_free_inplace(&tokens);
			goto prompt;
		}

		if (ulang_span_matches(&cmd->span, STR("l"))) {
			token *label = token_stream_consume(&stream);
			if (label && label->type != TOKEN_IDENTIFIER) label = NULL;
			ulang_label *labels = vm->program->labels;
			for (int i = 0; i < (int) vm->program->labelsLength; i++) {
				if (label && !ulang_span_matches(&label->span, labels[i].label.data.data, labels[i].label.data.length)) continue;
				size_t addr = labels[i].address;
				switch (labels[i].target) {
					case UL_LT_UNINITIALIZED:
					case UL_LT_CODE:
						break;
					case UL_LT_DATA:
						addr += vm->program->codeLength;
						break;
					case UL_LT_RESERVED_DATA:
						addr += vm->program->codeLength + vm->program->dataLength;
						break;
				}
				printf("%.*s: %zu %p\n", labels[i].label.data.length, labels[i].label.data.data, addr, (void *) addr);
			}
			goto prompt;
		}

		printf("Error: unknown command");
		token_array_free_inplace(&tokens);
		goto prompt;
	} while (-1);
	return UL_TRUE;
}

// BOZO need to throw an error in case memory sizes are bollocks
EMSCRIPTEN_KEEPALIVE void ulang_vm_init(ulang_vm *vm, ulang_program *program) {
	vm->memorySizeBytes = UL_VM_MEMORY_SIZE;
	vm->memory = ulang_alloc(vm->memorySizeBytes);
	memset(vm->registers, 0, sizeof(ulang_value) * 16);
	memset(vm->syscalls, 0, sizeof(ulang_syscall) * 256);
	memset(vm->memory, 0, vm->memorySizeBytes);
	memcpy(vm->memory, program->code, program->codeLength);
	memcpy(vm->memory + program->codeLength, program->data, program->dataLength);
	vm->registers[15].ui = vm->memorySizeBytes;
	vm->program = program;
}

#define DECODE_OP(word) ((word) & 0x7f)
#define DECODE_REG(word, index) (((word) >> (7 + 4 * (index))) & 0xf)
#define DECODE_OFF(word) (((word) >> 19) & 0x1fff)
#define REG1 regs[DECODE_REG(word, 0)].i
#define REG2 regs[DECODE_REG(word, 1)].i
#define REG3 regs[DECODE_REG(word, 2)].i
#define REG1_U regs[DECODE_REG(word, 0)].ui
#define REG2_U regs[DECODE_REG(word, 1)].ui
#define REG3_U regs[DECODE_REG(word, 2)].ui
#define REG1_F regs[DECODE_REG(word, 0)].f
#define REG2_F regs[DECODE_REG(word, 1)].f
#define REG3_F regs[DECODE_REG(word, 2)].f
#define VAL *((int32_t *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define VAL_U *((uint32_t *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define VAL_F *((float *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define SIGNUM(v) (((v) < 0) ? -1 : (((v) > 0) ? 1 : 0))
#define PC regs[14].ui
#define SP regs[15].ui

EMSCRIPTEN_KEEPALIVE ulang_bool ulang_vm_step(ulang_vm *vm) {
	ulang_value *regs = vm->registers;
	uint32_t word;
	memcpy(&word, &vm->memory[PC], 4);
	PC += 4;
	ulang_opcode op = DECODE_OP(word);

	switch (op) {
		case HALT:
			return UL_FALSE;
		case BREAK: {
			uint32_t val = VAL_U;
			if (val == REG1_U) {
				if (!vm->syscalls[0](0, vm)) return UL_FALSE;
			}
			break;
		}
		case ADD:
			REG3 = REG1 + REG2;
			break;
		case ADD_VAL:
			REG2 = REG1 + VAL;
			break;
		case SUB:
			REG3 = REG1 - REG2;
			break;
		case SUB_VAL:
			REG2 = REG1 - VAL;
			break;
		case MUL:
			REG3 = REG1 * REG2;
			break;
		case MUL_VAL:
			REG2 = REG1 * VAL;
			break;
		case DIV:
			REG3 = REG1 / REG2;
			break;
		case DIV_VAL:
			REG2 = REG1 / VAL;
			break;
		case DIV_UNSIGNED:
			REG3_U = REG1_U / REG2_U;
			break;
		case DIV_UNSIGNED_VAL:
			REG2_U = REG1_U / VAL_U;
			break;
		case REMAINDER:
			REG3 = REG1 % REG2;
			break;
		case REMAINDER_VAL:
			REG2 = REG1 % VAL;
			break;
		case REMAINDER_UNSIGNED:
			REG3_U = REG1_U % REG2_U;
			break;
		case REMAINDER_UNSIGNED_VAL:
			REG2_U = REG1_U % VAL_U;
			break;
		case ADD_FLOAT:
			REG3_F = REG1_F + REG2_F;
			break;
		case ADD_FLOAT_VAL:
			REG2_F = REG1_F + VAL_F;
			break;
		case SUB_FLOAT:
			REG3_F = REG1_F - REG2_F;
			break;
		case SUB_FLOAT_VAL:
			REG2_F = REG1_F - VAL_F;
			break;
		case MUL_FLOAT:
			REG3_F = REG1_F * REG2_F;
			break;
		case MUL_FLOAT_VAL:
			REG2_F = REG1_F * VAL_F;
			break;
		case DIV_FLOAT:
			REG3_F = REG1_F / REG2_F;
			break;
		case DIV_FLOAT_VAL:
			REG2_F = REG1_F / VAL_F;
			break;
		case COS:
			REG2_F = cosf(REG1_F);
			break;
		case SIN:
			REG2_F = sinf(REG1_F);
			break;
		case ATAN2:
			REG3_F = atan2f(REG1_F, REG2_F);
			break;
		case SQRT:
			REG2_F = sqrtf(REG1_F);
			break;
		case POW:
			REG3_F = powf(REG1_F, REG2_F);
			break;
		case POW_VAL: {
			float val = VAL_F;
			REG2_F = powf(REG1_F, val);
			break;
		}
		case RAND: {
			REG1_F = (float) rand() / (float) (RAND_MAX);
			break;
		}
		case INT_TO_FLOAT:
			REG2_F = REG1;
			break;
		case FLOAT_TO_INT:
			REG2 = REG1_F;
			break;
		case CMP:
			REG3 = SIGNUM(REG1 - REG2);
			break;
		case CMP_REG_VAL: {
			int32_t val = VAL;
			REG2 = SIGNUM(REG1 - val);
			break;
		}
		case CMP_UNSIGNED: {
			uint32_t reg1 = REG1_U;
			uint32_t reg2 = REG2_U;
			if (reg1 < reg2) REG3 = -1;
			else if (reg1 > reg2) REG3 = 1;
			else
				REG3 = 0;
			break;
		}
		case CMP_UNSIGNED_REG_VAL: {
			uint32_t reg1 = REG1_U;
			uint32_t val = VAL;
			if (reg1 < val) REG2 = -1;
			else if (reg1 > val) REG2 = 1;
			else
				REG2 = 0;
			break;
		}
		case CMP_FLOAT: {
			float reg1 = REG1_F;
			float reg2 = REG2_F;
			if (reg1 < reg2) REG3 = -1;
			else if (reg1 > reg2) REG3 = 1;
			else
				REG3 = 0;
			break;
		}
		case CMP_FLOAT_REG_VAL: {
			float reg1 = REG1_F;
			float val = VAL_F;
			if (reg1 < val) REG2 = -1;
			else if (reg1 > val) REG2 = 1;
			else
				REG2 = 0;
			break;
		}
		case NOT:
			REG2 = ~REG1;
			break;
		case NOT_VAL:
			REG1 = ~VAL;
			break;
		case AND:
			REG3 = REG1 & REG2;
			break;
		case AND_VAL:
			REG2 = REG1 & VAL;
			break;
		case OR:
			REG3 = REG1 | REG2;
			break;
		case OR_VAL:
			REG2 = REG1 | VAL;
			break;
		case XOR:
			REG3 = REG1 ^ REG2;
			break;
		case XOR_VAL:
			REG2 = REG1 ^ VAL;
			break;
		case SHL:
			REG3 = REG1 << REG2;
			break;
		case SHL_VAL:
			REG2 = REG1 << DECODE_OFF(word);
			break;
		case SHR:
			REG3 = REG1 >> REG2;
			break;
		case SHR_VAL:
			REG2 = REG1 >> DECODE_OFF(word);
			break;
		case SHRU:
			REG3_U = REG1_U >> REG2_U;
			break;
		case SHRU_VAL:
			REG2_U = REG1_U >> DECODE_OFF(word);
			break;
		case JUMP: {
			uint32_t addr = VAL_U;
			PC = addr;
			break;
		}
		case JUMP_EQUAL: {
			uint32_t addr = VAL_U;
			if (REG1 == 0) PC = addr;
			break;
		}
		case JUMP_NOT_EQUAL: {
			uint32_t addr = VAL_U;
			if (REG1 != 0) PC = addr;
			break;
		}
		case JUMP_LESS: {
			uint32_t addr = VAL_U;
			if (REG1 < 0) PC = addr;
			break;
		}
		case JUMP_GREATER: {
			uint32_t addr = VAL_U;
			if (REG1 > 0) PC = addr;
			break;
		}
		case JUMP_LESS_EQUAL: {
			uint32_t addr = VAL_U;
			if (REG1 <= 0) PC = addr;
			break;
		}
		case JUMP_GREATER_EQUAL: {
			uint32_t addr = VAL_U;
			if (REG1 >= 0) PC = addr;
			break;
		}
		case MOVE_REG:
			REG2 = REG1;
			break;
		case MOVE_VAL:
			REG1 = VAL;
			break;
		case LOAD_REG: {
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&REG2_U, &vm->memory[addr], 4);
			break;
		}
		case LOAD_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			memcpy(&REG2_U, &vm->memory[addr], 4);
			break;
		}
		case STORE_REG: {
			uint32_t addr = REG2_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &REG1_U, 4);
			break;
		}
		case STORE_REG_REG: {
			int32_t addr = REG2 + REG3;
			memcpy(&vm->memory[addr], &REG1, 4);
			break;
		}
		case STORE_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			memcpy(&vm->memory[addr], &REG1_U, 4);
			break;
		}
		case LOAD_BYTE_REG: {
			uint32_t addr = REG1_U + DECODE_OFF(word);
			REG2_U = vm->memory[addr];
			break;
		}
		case LOAD_BYTE_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			REG2_U = vm->memory[addr];
			break;
		}
		case STORE_BYTE_REG: {
			uint32_t addr = REG2_U + DECODE_OFF(word);
			vm->memory[addr] = (uint8_t) REG1_U;
			break;
		}
		case STORE_BYTE_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			vm->memory[addr] = (uint8_t) REG1_U;
			break;
		}
		case LOAD_SHORT_REG: {
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&REG2_U, &vm->memory[addr], 2);
			break;
		}
		case LOAD_SHORT_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			memcpy(&REG2_U, &vm->memory[addr], 2);
			break;
		}
		case STORE_SHORT_REG: {
			uint32_t addr = REG2_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &REG1_U, 2);
			break;
		}
		case STORE_SHORT_VAL: {
			uint32_t addr = DECODE_OFF(word) + VAL_U;
			memcpy(&vm->memory[addr], &REG1_U, 2);
			break;
		}
		case PUSH_REG: {
			SP -= 4;
			memcpy(vm->memory + SP, &regs[DECODE_REG(word, 0)].ui, 4);
			break;
		}
		case PUSH_VAL: {
			SP -= 4;
			uint32_t val = VAL_U;
			memcpy(vm->memory + SP, &val, 4);
			break;
		}
		case STACKALLOC: {
			uint32_t numWords = DECODE_OFF(word);
			SP -= numWords << 2;
			break;
		}
		case POP_REG: {
			memcpy(&regs[DECODE_REG(word, 0)].ui, vm->memory + SP, 4);
			SP += 4;
			break;
		}
		case POP_OFF: {
			memcpy(&regs[DECODE_REG(word, 0)].ui, vm->memory + SP, 4);
			SP += DECODE_OFF(word) << 2;
			break;
		}
		case CALL_REG: {
			SP -= 4;
			memcpy(vm->memory + SP, &PC, 4);
			PC = REG1_U;
			break;
		}
		case CALL_VAL: {
			uint32_t addr = VAL_U;
			SP -= 4;
			memcpy(vm->memory + SP, &PC, 4);
			PC = addr;
			break;
		}
		case RET: {
			uint32_t addr;
			memcpy(&addr, &vm->memory[SP], 4);
			SP += 4;
			PC = addr;
			break;
		}
		case RETN: {
			uint32_t addr;
			memcpy(&addr, &vm->memory[SP], 4);
			SP += 4 + DECODE_OFF(word) * 4;
			PC = addr;
			break;
		}
		case SYSCALL: {
			uint32_t intNum = DECODE_OFF(word);
			if (intNum < 0 || intNum > 255 || !vm->syscalls[intNum])
				break;
			if (!vm->syscalls[intNum](intNum, vm)) return UL_FALSE;
			break;
		}
		default:
			vm->registers[14].ui -= 4; // reset PC to the unknown instruction.
			return UL_FALSE;
	}
	return UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE ulang_bool ulang_vm_step_n(ulang_vm *vm, uint32_t numInstructions) {
	while (numInstructions--) {
		if (!ulang_vm_step(vm)) return UL_FALSE;
	}
	return UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE int32_t ulang_vm_step_n_bp(ulang_vm *vm, uint32_t numInstructions, uint32_t *breakpoints, uint32_t numBreakpoints) {
	while (numInstructions--) {
		uint32_t pc = vm->registers[14].ui;
		for (size_t i = 0; i < numBreakpoints; i++) {
			if (breakpoints[i] == pc) return i + 1;
		}
		if (!ulang_vm_step(vm)) return UL_FALSE;
	}
	return UL_TRUE;
}

EMSCRIPTEN_KEEPALIVE void ulang_vm_print(ulang_vm *vm) {
	ulang_value *regs = vm->registers;
	for (int i = 0; i < 16; i++) {
		printf("%.*s: %i (0x%x), %f ", (int) registers[i].name.length, registers[i].name.data, regs[i].i, regs[i].i,
			   regs[i].f);
		if ((i + 1) % 4 == 0) printf("\n");
	}

	for (int i = 0; i < 5; i++) {
		uint32_t stackAddr = vm->registers[15].ui;
		stackAddr += i * 4;
		if (stackAddr > UL_VM_MEMORY_SIZE - 4) break;
		int32_t val_i;
		float val_f;
		memcpy(&val_i, &vm->memory[stackAddr], 4);
		memcpy(&val_f, &vm->memory[stackAddr], 4);
		printf("sp+%i: %i (0x%x), %f\n", i * 4, val_i, val_i, val_f);
	}

	if (vm->program && vm->program->file) {
		ulang_file_get_lines(vm->program->file);
		uint32_t pc = (vm->registers[14].ui) >> 2;
		if (pc < 0 || pc >= vm->program->addressToLineLength) return;
		int lineNum = (int) vm->program->addressToLine[pc];

		for (int i = MAX(1, lineNum - 2); i < MIN((int) vm->program->file->numLines, lineNum + 3); i++) {
			ulang_line line = vm->program->file->lines[i];
			printf("(%.*s:%i): ", (int) vm->program->file->fileName.length, vm->program->file->fileName.data, line.lineNumber);
			printf("%s %.*s\n", i == lineNum ? "-->" : "   ", (int) line.data.length, line.data.data);
		}
	}
}

EMSCRIPTEN_KEEPALIVE int32_t ulang_vm_pop_int(ulang_vm *vm) {
	int32_t val;
	memcpy(&val, vm->memory + vm->registers[15].ui, 4);
	vm->registers[15].ui += 4;
	return val;
}

EMSCRIPTEN_KEEPALIVE uint32_t ulang_vm_pop_uint(ulang_vm *vm) {
	uint32_t val;
	memcpy(&val, vm->memory + vm->registers[15].ui, 4);
	vm->registers[15].ui += 4;
	return val;
}

EMSCRIPTEN_KEEPALIVE float ulang_vm_pop_float(ulang_vm *vm) {
	float val;
	memcpy(&val, vm->memory + vm->registers[15].ui, 4);
	vm->registers[15].ui += 4;
	return val;
}

EMSCRIPTEN_KEEPALIVE void ulang_vm_push_int(ulang_vm *vm, int32_t val) {
	vm->registers[15].ui -= 4;
	memcpy(vm->memory + vm->registers[15].ui, &val, 4);
}

EMSCRIPTEN_KEEPALIVE void ulang_vm_push_uint(ulang_vm *vm, uint32_t val) {
	vm->registers[15].ui -= 4;
	memcpy(vm->memory + vm->registers[15].ui, &val, 4);
}

EMSCRIPTEN_KEEPALIVE void ulang_vm_push_float(ulang_vm *vm, float val) {
	vm->registers[15].ui -= 4;
	memcpy(vm->memory + vm->registers[15].ui, &val, 4);
}

EMSCRIPTEN_KEEPALIVE void ulang_vm_free(ulang_vm *vm) {
	ulang_free(vm->memory);
	if (vm->error.is_set) ulang_error_free(&vm->error);
}

typedef enum UlangType {
	UL_TYPE_FILE,
	UL_TYPE_ERROR,
	UL_TYPE_PROGRAM,
	UL_TYPE_VM
} UlangType;

EMSCRIPTEN_KEEPALIVE int ulang_sizeof(UlangType type) {
	switch (type) {
		case UL_TYPE_FILE:
			return sizeof(ulang_file);
		case UL_TYPE_ERROR:
			return sizeof(ulang_error);
		case UL_TYPE_PROGRAM:
			return sizeof(ulang_program);
		case UL_TYPE_VM:
			return sizeof(ulang_vm);
	}
}

EMSCRIPTEN_KEEPALIVE void ulang_print_offsets() {
	printf("ulang_string (size=%lu)\n", sizeof(ulang_string));
	printf("   data: %lu\n", offsetof(ulang_string, data));
	printf("   length: %lu\n", offsetof(ulang_string, length));

	printf("ulang_span (size=%lu)\n", sizeof(ulang_span));
	printf("   data: %lu\n", offsetof(ulang_span, data));
	printf("   startLine: %lu\n", offsetof(ulang_span, startLine));
	printf("   endLine: %lu\n", offsetof(ulang_span, endLine));

	printf("ulang_line (size=%lu)\n", sizeof(ulang_line));
	printf("   data: %lu\n", offsetof(ulang_line, data));
	printf("   lineNumber: %lu\n", offsetof(ulang_line, lineNumber));

	printf("ulang_file (size=%lu)\n", sizeof(ulang_file));
	printf("   fileName: %lu\n", offsetof(ulang_file, fileName));
	printf("   data: %lu\n", offsetof(ulang_file, data));
	printf("   length: %lu\n", offsetof(ulang_file, length));
	printf("   lines: %lu\n", offsetof(ulang_file, lines));
	printf("   numLines: %lu\n", offsetof(ulang_file, numLines));

	printf("ulang_error (size=%lu)\n", sizeof(ulang_error));
	printf("   file: %lu\n", offsetof(ulang_error, file));
	printf("   span: %lu\n", offsetof(ulang_error, span));
	printf("   message: %lu\n", offsetof(ulang_error, message));
	printf("   is_set: %lu\n", offsetof(ulang_error, is_set));

	printf("ulang_label (size=%lu)\n", sizeof(ulang_label));
	printf("   label: %lu\n", offsetof(ulang_label, label));
	printf("   target: %lu\n", offsetof(ulang_label, target));
	printf("   target: %lu\n", offsetof(ulang_label, address));

	printf("ulang_constant (size=%lu)\n", sizeof(ulang_constant));
	printf("   type: %lu\n", offsetof(ulang_constant, type));
	printf("   name: %lu\n", offsetof(ulang_constant, name));
	printf("   i: %lu\n", offsetof(ulang_constant, i));
	printf("   f: %lu\n", offsetof(ulang_constant, f));

	printf("ulang_program (size=%lu)\n", sizeof(ulang_program));
	printf("   code: %lu\n", offsetof(ulang_program, code));
	printf("   codeLength: %lu\n", offsetof(ulang_program, codeLength));
	printf("   data: %lu\n", offsetof(ulang_program, data));
	printf("   dataLength: %lu\n", offsetof(ulang_program, dataLength));
	printf("   reservedBytes: %lu\n", offsetof(ulang_program, reservedBytes));
	printf("   labels: %lu\n", offsetof(ulang_program, labels));
	printf("   labelsLength: %lu\n", offsetof(ulang_program, labelsLength));
	printf("   constants: %lu\n", offsetof(ulang_program, constants));
	printf("   constantsLength: %lu\n", offsetof(ulang_program, constantsLength));
	printf("   file: %lu\n", offsetof(ulang_program, file));
	printf("   addressToLine: %lu\n", offsetof(ulang_program, addressToLine));
	printf("   addressToLineLength: %lu\n", offsetof(ulang_program, addressToLineLength));

	printf("ulang_vm (size=%lu)\n", sizeof(ulang_vm));
	printf("   registers: %lu\n", offsetof(ulang_vm, registers));
	printf("   memory: %lu\n", offsetof(ulang_vm, memory));
	printf("   memorySizeBytes: %lu\n", offsetof(ulang_vm, memorySizeBytes));
	printf("   syscalls: %lu\n", offsetof(ulang_vm, syscalls));
	printf("   error: %lu\n", offsetof(ulang_vm, error));
	printf("   program: %lu\n", offsetof(ulang_vm, program));
}

EMSCRIPTEN_KEEPALIVE uint8_t *ulang_argb_to_rgba(uint8_t *argb, uint8_t *rgba, size_t numPixels) {
	numPixels <<= 2;
	for (size_t i = 0; i < numPixels; i += 4) {
		uint8_t b = argb[i];
		uint8_t g = argb[i + 1];
		uint8_t r = argb[i + 2];
		uint8_t a = argb[i + 3];
		rgba[i] = r;
		rgba[i + 1] = g;
		rgba[i + 2] = b;
		rgba[i + 3] = a;
	}
	return rgba;
}
