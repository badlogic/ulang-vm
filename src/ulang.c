#include <ulang.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define STR(str) str, sizeof(str) - 1
#define STR_OBJ(str) (ulang_string){ str, sizeof(str) - 1 }
#define MAX(a, b) (a > b ? a : b)

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

typedef enum ulang_opcode {
	HALT,
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
	CMP,
	CMP_VAL,
	CMP_UNSIGNED,
	CMP_UNSIGNED_VAL,
	CMP_FLOAT,
	CMP_FLOAT_VAL,
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
	RETURN,
	PORT_WRITE_REG,
	PORT_WRITE_VAL,
	PORT_READ_REG,
	PORT_READ_OFF,
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
} opcode;

opcode opcodes[] = {
		{HALT,                   STR_OBJ("halt")},
		{ADD,                    STR_OBJ("add"),         {UL_REG,         UL_REG,     UL_REG}},
		{ADD_VAL,                STR_OBJ("add"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{SUB,                    STR_OBJ("sub"),         {UL_REG,         UL_REG,     UL_REG}},
		{SUB_VAL,                STR_OBJ("sub"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{MUL,                    STR_OBJ("mul"),         {UL_REG,         UL_REG,     UL_REG}},
		{MUL_VAL,                STR_OBJ("mul"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{DIV,                    STR_OBJ("div"),         {UL_REG,         UL_REG,     UL_REG}},
		{DIV_VAL,                STR_OBJ("div"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{DIV_UNSIGNED,           STR_OBJ("divu"),        {UL_REG,         UL_REG,     UL_REG}},
		{DIV_UNSIGNED_VAL,       STR_OBJ("divu"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{REMAINDER,              STR_OBJ("rem"),         {UL_REG,         UL_REG,     UL_REG}},
		{REMAINDER_VAL,          STR_OBJ("rem"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{REMAINDER_UNSIGNED,     STR_OBJ("remu"),        {UL_REG,         UL_REG,     UL_REG}},
		{REMAINDER_UNSIGNED_VAL, STR_OBJ("remu"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{ADD_FLOAT,              STR_OBJ("addf"),        {UL_REG,         UL_REG,     UL_REG}},
		{ADD_FLOAT_VAL,          STR_OBJ("addf"),        {UL_REG,         UL_FLT,     UL_REG}},
		{SUB_FLOAT,              STR_OBJ("subf"),        {UL_REG,         UL_REG,     UL_REG}},
		{SUB_FLOAT_VAL,          STR_OBJ("subf"),        {UL_REG,         UL_FLT,     UL_REG}},
		{MUL_FLOAT,              STR_OBJ("mulf"),        {UL_REG,         UL_REG,     UL_REG}},
		{MUL_FLOAT_VAL,          STR_OBJ("mulf"),        {UL_REG,         UL_FLT,     UL_REG}},
		{DIV_FLOAT,              STR_OBJ("divf"),        {UL_REG,         UL_REG,     UL_REG}},
		{DIV_FLOAT_VAL,          STR_OBJ("divf"),        {UL_REG,         UL_FLT,     UL_REG}},
		{COS,                    STR_OBJ("cosf"),        {UL_REG,         UL_REG}},
		{SIN,                    STR_OBJ("sinf"),        {UL_REG,         UL_REG}},
		{ATAN2,                  STR_OBJ("atan2f"),      {UL_REG,         UL_REG,     UL_REG}},
		{SQRT,                   STR_OBJ("sqrtf"),       {UL_REG,         UL_REG}},
		{POW,                    STR_OBJ("powf"),        {UL_REG,         UL_REG,     UL_REG}},
		{POW_VAL,                STR_OBJ("powf"),        {UL_REG,         UL_FLT,     UL_REG}},
		{INT_TO_FLOAT,           STR_OBJ("i2f"),         {UL_REG,         UL_REG}},
		{FLOAT_TO_INT,           STR_OBJ("f2i"),         {UL_REG,         UL_REG}},

		{NOT,                    STR_OBJ("not"),         {UL_REG,         UL_REG}},
		{NOT_VAL,                STR_OBJ("not"),         {UL_LBL_INT,     UL_REG}},
		{AND,                    STR_OBJ("and"),         {UL_REG,         UL_REG,     UL_REG}},
		{AND_VAL,                STR_OBJ("and"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{OR,                     STR_OBJ("or"),          {UL_REG,         UL_REG,     UL_REG}},
		{OR_VAL,                 STR_OBJ("or"),          {UL_REG,         UL_LBL_INT, UL_REG}},
		{XOR,                    STR_OBJ("xor"),         {UL_REG,         UL_REG,     UL_REG}},
		{XOR_VAL,                STR_OBJ("xor"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{SHL,                    STR_OBJ("shl"),         {UL_REG,         UL_REG,     UL_REG}},
		{SHL_VAL,                STR_OBJ("shl"),         {UL_REG,         UL_OFF,     UL_REG}},
		{SHR,                    STR_OBJ("shr"),         {UL_REG,         UL_REG,     UL_REG}},
		{SHR_VAL,                STR_OBJ("shr"),         {UL_REG,         UL_OFF,     UL_REG}},

		{CMP,                    STR_OBJ("cmp"),         {UL_REG,         UL_REG,     UL_REG}},
		{CMP_VAL,                STR_OBJ("cmp"),         {UL_REG,         UL_LBL_INT, UL_REG}},
		{CMP_UNSIGNED,           STR_OBJ("cmpu"),        {UL_REG,         UL_REG,     UL_REG}},
		{CMP_UNSIGNED_VAL,       STR_OBJ("cmpu"),        {UL_REG,         UL_LBL_INT, UL_REG}},
		{CMP_FLOAT,              STR_OBJ("cmpf"),        {UL_REG,         UL_REG,     UL_REG}},
		{CMP_FLOAT_VAL,          STR_OBJ("cmpf"),        {UL_REG,         UL_FLT,     UL_REG}},


		{JUMP,                   STR_OBJ("jmp"),         {UL_LBL_INT}},
		{JUMP_EQUAL,             STR_OBJ("je"),          {UL_REG,         UL_LBL_INT}},
		{JUMP_NOT_EQUAL,         STR_OBJ("jne"),         {UL_REG,         UL_LBL_INT}},
		{JUMP_LESS,              STR_OBJ("jl"),          {UL_REG,         UL_LBL_INT}},
		{JUMP_GREATER,           STR_OBJ("jg"),          {UL_REG,         UL_LBL_INT}},
		{JUMP_LESS_EQUAL,        STR_OBJ("jle"),         {UL_REG,         UL_LBL_INT}},
		{JUMP_GREATER_EQUAL,     STR_OBJ("jge"),         {UL_REG,         UL_LBL_INT}},

		{MOVE_REG,               STR_OBJ("move"),        {UL_REG,         UL_REG}},
		{MOVE_VAL,               STR_OBJ("move"),        {UL_LBL_INT_FLT, UL_REG}},

		{LOAD_REG,               STR_OBJ("load"),        {UL_REG,         UL_OFF,     UL_REG}},
		{LOAD_VAL,               STR_OBJ("load"),        {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{STORE_REG,              STR_OBJ("store"),       {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_VAL,              STR_OBJ("store"),       {UL_REG,         UL_LBL_INT, UL_OFF}},

		{LOAD_BYTE_REG,          STR_OBJ("load_byte"),   {UL_REG,         UL_OFF,     UL_REG}},
		{LOAD_BYTE_VAL,          STR_OBJ("load_byte"),   {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{STORE_BYTE_REG,         STR_OBJ("store_byte"),  {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_BYTE_VAL,         STR_OBJ("store_byte"),  {UL_REG,         UL_LBL_INT, UL_OFF}},

		{LOAD_SHORT_REG,         STR_OBJ("load_short"),  {UL_LBL_INT,     UL_OFF,     UL_REG}},
		{LOAD_SHORT_VAL,         STR_OBJ("load_short"),  {UL_REG,         UL_OFF,     UL_REG}},
		{STORE_SHORT_REG,        STR_OBJ("store_short"), {UL_REG,         UL_REG,     UL_OFF}},
		{STORE_SHORT_VAL,        STR_OBJ("store_short"), {UL_REG,         UL_LBL_INT, UL_OFF}},

		{PUSH_REG,               STR_OBJ("push"),        {UL_REG}},
		{PUSH_VAL,               STR_OBJ("push"),        {UL_LBL_INT_FLT}},

		{STACKALLOC,             STR_OBJ("stackalloc"),  {UL_OFF}},

		{POP_REG,                STR_OBJ("pop"),         {UL_REG}},
		{POP_OFF,                STR_OBJ("pop"),         {UL_OFF}},

		{CALL_REG,               STR_OBJ("call"),        {UL_REG}},
		{CALL_VAL,               STR_OBJ("call"),        {UL_LBL_INT}},

		{RETURN,                 STR_OBJ("return"),      {UL_OFF}},

		{PORT_WRITE_REG,         STR_OBJ("port_write"),  {UL_REG,         UL_OFF}},
		{PORT_WRITE_VAL,         STR_OBJ("port_write"),  {UL_INT,         UL_OFF}},
		{PORT_READ_REG,          STR_OBJ("port_read"),   {UL_REG,         UL_REG}},
		{PORT_READ_OFF,          STR_OBJ("port_read"),   {UL_OFF,         UL_REG}},
};

static size_t opcodeLength = sizeof(opcodes) / sizeof(opcode);

typedef struct reg {
	ulang_string name;
	int index;
} reg;

reg registers[] = {
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

static ulang_bool string_equals(ulang_string *a, ulang_string *b) {
	if (a->length != b->length) return UL_FALSE;
	char *aData = a->data;
	char *bData = b->data;
	size_t length = a->length;
	for (size_t i = 0; i < length; i++) {
		if (aData[i] != bData[i]) return UL_FALSE;
	}
	return UL_TRUE;
}

static ulang_bool initialized = UL_FALSE;

static void init_opcodes_and_registers() {
	if (initialized) return;

	for (size_t i = 0; i < opcodeLength; i++) {
		opcode *opcode = &opcodes[i];
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

void *ulang_realloc(void *old, size_t numBytes) {
	return realloc(old, numBytes);
}

void ulang_free(void *ptr) {
	if (!ptr) return;
	frees++;
	free(ptr);
}

void ulang_print_memory() {
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

ulang_bool ulang_file_from_memory(const char *fileName, const char *content, ulang_file *file) {
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

void ulang_file_free(ulang_file *file) {
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

static void ulang_error_init(ulang_error *error, ulang_file *file, ulang_span span, const char *msg, ...) {
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
	error->span = span;
	error->is_set = UL_TRUE;
}

void ulang_error_free(ulang_error *error) {
	ulang_free(error->message.data);
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

	if (lineStart < file->length) {
		ulang_line line = {{data + lineStart, file->length - lineStart}, addedLines};
		file->lines[addedLines + 1] = line;
	}
}

void ulang_error_print(ulang_error *error) {
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
			printf("%s", i >= errorStart && i <= errorEnd ? "^" : (useTab ? "\t" : " "));
		}
		printf("\n");
	}
}

typedef struct {
	ulang_file *data;
	uint32_t index;
	uint32_t end;
	uint32_t line;
	uint32_t spanStart;
	uint32_t spanLineStart;
} character_stream;

static void character_stream_init(character_stream *stream, ulang_file *fileData) {
	stream->data = fileData;
	stream->index = 0;
	stream->line = 1;
	stream->end = (uint32_t) fileData->length;
	stream->spanStart = 0;
	stream->spanLineStart = 0;
}

static uint32_t next_utf8_character(const char *data, uint32_t *index) {
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
	} while (((data[*index]) & 0xC0) == 0x80 && data[*index]);
	character -= utf8Offsets[sz - 1];

	return character;
}

static ulang_bool has_more(character_stream *stream) {
	return stream->index < stream->data->length;
}

static uint32_t consume(character_stream *stream) {
	return next_utf8_character(stream->data->data, &stream->index);
}

static ulang_bool match(character_stream *stream, const char *needleData, ulang_bool consume) {
	uint32_t needleLength = 0;
	const char *sourceData = stream->data->data;
	for (uint32_t i = 0, j = stream->index; needleData[i] != 0; i++, needleLength++) {
		if (j >= stream->end) return UL_FALSE;
		uint32_t c = next_utf8_character(sourceData, &j);
		if ((unsigned char) needleData[i] != c) return UL_FALSE;
	}
	if (consume) stream->index += needleLength;
	return UL_TRUE;
}

static ulang_bool match_digit(character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return UL_FALSE;
	char c = stream->data->data[stream->index];
	if (c >= '0' && c <= '9') {
		if (consume) stream->index++;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool match_hex(character_stream *stream) {
	if (!has_more(stream)) return UL_FALSE;
	char c = stream->data->data[stream->index];
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
		stream->index++;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool match_identifier_start(character_stream *stream) {
	if (!has_more(stream)) return UL_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xc0) {
		stream->index = idx;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static ulang_bool match_identifier_part(character_stream *stream) {
	if (!has_more(stream)) return UL_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9') || c >= 0x80) {
		stream->index = idx;
		return UL_TRUE;
	}
	return UL_FALSE;
}

static void skip_white_space(character_stream *stream) {
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

static void start_span(character_stream *stream) {
	stream->spanStart = stream->index;
	stream->spanLineStart = stream->line;
}

static ulang_span end_span(character_stream *stream) {
	ulang_span span;
	span.data.data = stream->data->data + stream->spanStart;
	span.data.length = stream->index - stream->spanStart;
	span.startLine = stream->spanLineStart;
	span.endLine = stream->line;
	return span;
}

static ulang_bool span_matches(ulang_span *span, const char *needle, size_t length) {
	if (span->data.length != length) return UL_FALSE;

	const char *sourceData = span->data.data;
	for (uint32_t i = 0; i < length; i++) {
		if (sourceData[i] != needle[i]) return UL_FALSE;
	}
	return UL_TRUE;
}

typedef enum token_type {
	TOKEN_INTEGER,
	TOKEN_FLOAT,
	TOKEN_STRING,
	TOKEN_IDENTIFIER,
	TOKEN_SPECIAL_CHAR,
	TOKEN_EOF
} token_type;

typedef struct {
	ulang_span span;
	token_type type;
} token;

static ulang_bool has_more_tokens(character_stream *stream) {
	if (!has_more(stream)) return UL_FALSE;
	skip_white_space(stream);
	return has_more(stream);
}

static ulang_bool next_token(character_stream *stream, token *token, ulang_error *error) {
	if (!has_more_tokens(stream)) {
		token->span.startLine = 0;
		token->span.endLine = 0;
		token->type = TOKEN_EOF;
		return UL_TRUE;
	}
	start_span(stream);

	if (match(stream, "-", UL_TRUE) || match_digit(stream, UL_FALSE)) {
		token->type = TOKEN_INTEGER;
		if (match(stream, "0x", UL_TRUE)) {
			while (match_hex(stream));
		} else {
			while (match_digit(stream, UL_TRUE));
			if (match(stream, ".", UL_TRUE)) {
				token->type = TOKEN_FLOAT;
				while (match_digit(stream, UL_TRUE));
			}
		}
		if (match(stream, "b", UL_TRUE)) {
			if (token->type == TOKEN_FLOAT) {
				ulang_error_init(error, stream->data, end_span(stream), "Byte literal can not have a decimal point.");
				return UL_FALSE;
			}
			token->type = TOKEN_INTEGER;
		}
		token->span = end_span(stream);
		return UL_TRUE;
	}

	// String literal
	if (match(stream, "\"", UL_TRUE)) {
		ulang_bool matchedEndQuote = UL_FALSE;
		token->type = TOKEN_STRING;
		while (has_more(stream)) {
			// Note: escape sequences like \n are parsed in the AST
			if (match(stream, "\\", UL_TRUE)) {
				consume(stream);
			}
			if (match(stream, "\"", UL_TRUE)) {
				matchedEndQuote = UL_TRUE;
				break;
			}
			if (match(stream, "\n", UL_FALSE)) {
				ulang_error_init(error, stream->data, end_span(stream),
								 "String literal is not closed by double quote");
				return UL_FALSE;
			}
			consume(stream);
		}
		if (!matchedEndQuote) {
			ulang_error_init(error, stream->data, end_span(stream), "String literal is not closed by double quote");
			return UL_FALSE;
		}
		token->span = end_span(stream);
		return UL_TRUE;
	}

	// Identifier or keyword
	if (match_identifier_start(stream)) {
		while (match_identifier_part(stream));
		token->type = TOKEN_IDENTIFIER;
		token->span = end_span(stream);
		return UL_TRUE;
	}

	// Else check for "simple" tokens made up of
	// 1 character literals, like ".", ",", or ";",
	consume(stream);
	token->type = TOKEN_SPECIAL_CHAR;
	token->span = end_span(stream);
	return UL_TRUE;
}

static ulang_bool
next_token_matches(character_stream *stream, const char *needle, size_t size, ulang_bool consume) {
	character_stream stream_copy = *stream;
	// BOZO this needs to be cleaned up in case of error below
	ulang_error error;
	token token;
	if (!next_token(&stream_copy, &token, &error)) return UL_FALSE;
	if (token.type == TOKEN_EOF) return UL_FALSE;
	ulang_bool matches = span_matches(&token.span, needle, size);
	if (matches && consume) next_token(stream, &token, &error);
	return matches;
}

static ulang_bool
next_token_matches_type(character_stream *stream, token *token, token_type type, ulang_bool consume) {
	character_stream stream_copy = *stream;
	// BOZO this needs to be cleaned up in case of error below
	ulang_error error;
	if (!next_token(&stream_copy, token, &error)) return UL_FALSE;
	if (token->type != type) return UL_FALSE;
	if (consume) next_token(stream, token, &error);
	return UL_TRUE;
}

#define EXPECT_TOKEN(stream, str, error) { \
    token _token; \
    if (!next_token(&(stream), &_token, error)) goto _compilation_error; \
    if (_token.type == TOKEN_EOF) { \
        ulang_error_init(error, (stream).data, _token.span, "Unexpected end of file, expected '%.*s'", (str).length, (str).data); \
        goto _compilation_error; \
    } \
    if (!span_matches(&_token.span, (str).data, (str).length)) { \
        ulang_error_init(error, (stream).data, _token.span, "Expected %.*s", (str).length, (str).data); \
        goto _compilation_error; \
    } \
}

static reg *token_matches_register(token *token) {
	ulang_span *span = &token->span;
	for (size_t i = 0; i < (sizeof(registers) / sizeof(reg)); i++) {
		if (span_matches(span, registers[i].name.data, registers[i].name.length)) {
			return &registers[i];
		}
	}
	return NULL;
}

static opcode *token_matches_opcode(token *token) {
	ulang_span *span = &token->span;
	for (size_t i = 0; i < opcodeLength; i++) {
		if (span_matches(span, opcodes[i].name.data, opcodes[i].name.length)) {
			return &opcodes[i];
		}
	}
	return NULL;
}

int token_to_int(token *token) {
	ulang_string *str = &token->span.data;
	char c = str->data[str->length];
	str->data[str->length] = 0;
	long val = strtol(str->data, NULL, 0);
	str->data[str->length] = c;
	return (int) val;
}

static float token_to_float(token *token) {
	ulang_string *str = &token->span.data;
	char c = str->data[str->length];
	str->data[str->length] = 0;
	float val = strtof(str->data, NULL);
	str->data[str->length] = c;
	return val;
}

typedef struct patch {
	ulang_span label;
	size_t patchAddress;
} patch;
ARRAY_IMPLEMENT(patch_array, patch)

ARRAY_IMPLEMENT(label_array, ulang_label)

ARRAY_IMPLEMENT(byte_array, uint8_t)

static void emit_byte(byte_array *code, token *value, int repeat) {
	int val = token_to_int(value);
	val &= 0xff;
	for (int i = 0; i < repeat; i++) {
		byte_array_add(code, (uint8_t) val);
	}
}

static void emit_short(byte_array *code, token *value, int repeat) {
	int val = token_to_int(value);
	val &= 0xffff;
	byte_array_ensure(code, 2 * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], &val, 2);
		code->size += 2;
	}
}

static void emit_int(byte_array *code, token *value, int repeat) {
	int val = token_to_int(value);
	byte_array_ensure(code, 4 * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], &val, 4);
		code->size += 4;
	}
}

static void emit_float(byte_array *code, token *value, int repeat) {
	float val = token_to_float(value);
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
	byte_array_ensure(code, (str.length + 1) * repeat);
	for (int i = 0; i < repeat; i++) {
		memcpy(&code->items[code->size], str.data, str.length);
		code->items[code->size + str.length] = 0;
		code->size += str.length + 1;
	}
}

#define ENCODE_OP(word, op) word |= op
#define ENCODE_REG(word, reg, index) word |= (((reg) & 0xf) << (7 + 4 * (index)))
#define ENCODE_OFF(word, offset) word |= (((offset) & 0x1fff) << 19)

static ulang_bool
emit_op(ulang_file *file, opcode *op, token operands[3], patch_array *patches, byte_array *code, ulang_error *error) {
	uint32_t word1 = 0;
	uint32_t word2 = 0;

	ENCODE_OP(word1, op->code);

	int numEmittedRegs = 0;
	for (int i = 0; i < op->numOperands; i++) {
		token *operandToken = &operands[i];
		operand_type operandType = op->operands[i];
		switch (operandType) {
			case UL_REG:
				ENCODE_REG(word1, token_matches_register(operandToken)->index, numEmittedRegs);
				numEmittedRegs++;
				break;
			case UL_OFF:
				ENCODE_OFF(word1, token_to_int(operandToken));
				break;
			case UL_INT:
			case UL_FLT:
			case UL_LBL_INT:
			case UL_LBL_INT_FLT:
				switch (operandToken->type) {
					case TOKEN_INTEGER: {
						int value = token_to_int(operandToken);
						memcpy(&word2, &value, 4);
						break;
					}
					case TOKEN_FLOAT: {
						float value = token_to_float(operandToken);
						memcpy(&word2, &value, 4);
						break;
					}
					case TOKEN_IDENTIFIER: {
						patch p;
						p.label = operandToken->span;
						p.patchAddress = code->size + 4;
						patch_array_add(patches, p);
						word2 = 0xdeadbeef;
						break;
					}
					default:
						ulang_error_init(error, file, operandToken->span,
										 "Internal error, unexpected token type for value operand.");
						return UL_FALSE;
				}
				break;
			default:
				ulang_error_init(error, file, operandToken->span,
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

static void parse_repeat(character_stream *stream, int *numRepeat, ulang_error *error) {
	if (!next_token_matches(stream, STR("x"), UL_TRUE)) return;
	token token;
	if (!next_token_matches_type(stream, &token, TOKEN_INTEGER, UL_TRUE)) {
		ulang_error_init(error, stream->data, token.span, "Expected an integer value after 'x'.");
		return;
	}
	*numRepeat = token_to_int(&token);
	if (*numRepeat < 0) {
		ulang_error_init(error, stream->data, token.span, "Number of repetitions can not be negative.");
		return;
	}
}

static void set_label_targets(label_array *labels, ulang_label_target target, size_t address) {
	for (int i = (int) labels->size - 1; i >= 0; i--) {
		if (labels->items[i].target != UL_LT_UNINITIALIZED) break;
		labels->items[i].target = target;
		labels->items[i].address = address;
	}
}

ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error) {
	character_stream stream;
	character_stream_init(&stream, file);
	init_opcodes_and_registers();
	error->is_set = UL_FALSE;

	patch_array patches;
	patch_array_init_inplace(&patches, 16);
	label_array labels;
	label_array_init_inplace(&labels, 16);
	byte_array code;
	byte_array_init_inplace(&code, 16);
	byte_array data;
	byte_array_init_inplace(&data, 16);
	size_t numReservedBytes = 0;

	while (has_more_tokens(&stream)) {
		token tok;
		if (!next_token(&stream, &tok, error)) goto _compilation_error;

		opcode *op = token_matches_opcode(&tok);
		if (!op) {
			if (tok.type != TOKEN_IDENTIFIER) {
				ulang_error_init(error, file, tok.span, "Expected a label, data, or an instruction.");
				goto _compilation_error;
			}

			if (span_matches(&tok.span, STR("byte"))) {
				while (-1) {
					token value;
					if (!next_token(&stream, &value, error)) goto _compilation_error;
					if (value.type != TOKEN_INTEGER && value.type != TOKEN_STRING) {
						ulang_error_init(error, file, value.span, "Expected an integer value or string literal.");
						goto _compilation_error;
					}

					int numRepeat = 1;
					parse_repeat(&stream, &numRepeat, error);
					if (error->is_set) goto _compilation_error;

					set_label_targets(&labels, UL_LT_DATA, data.size);
					if (value.type == TOKEN_INTEGER) emit_byte(&data, &value, numRepeat);
					else emit_string(&data, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (span_matches(&tok.span, STR("short"))) {
				while (-1) {
					token value;
					if (!next_token(&stream, &value, error)) goto _compilation_error;
					if (value.type != TOKEN_INTEGER) {
						ulang_error_init(error, file, value.span, "Expected an integer value.");
						goto _compilation_error;
					}

					int numRepeat = 1;
					parse_repeat(&stream, &numRepeat, error);
					if (error->is_set) goto _compilation_error;

					set_label_targets(&labels, UL_LT_DATA, data.size);
					emit_short(&data, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (span_matches(&tok.span, STR("int"))) {
				while (-1) {
					token value;
					if (!next_token(&stream, &value, error)) goto _compilation_error;
					if (value.type != TOKEN_INTEGER) {
						ulang_error_init(error, file, value.span, "Expected an integer value.");
						goto _compilation_error;
					}

					int numRepeat = 1;
					parse_repeat(&stream, &numRepeat, error);
					if (error->is_set) goto _compilation_error;

					set_label_targets(&labels, UL_LT_DATA, data.size);
					emit_int(&data, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (span_matches(&tok.span, STR("float"))) {
				while (-1) {
					token value;
					if (!next_token(&stream, &value, error)) goto _compilation_error;
					if (value.type != TOKEN_FLOAT) {
						ulang_error_init(error, file, value.span, "Expected a floating point value.");
						goto _compilation_error;
					}

					int numRepeat = 1;
					parse_repeat(&stream, &numRepeat, error);
					if (error->is_set) goto _compilation_error;

					set_label_targets(&labels, UL_LT_DATA, data.size);
					emit_float(&data, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), UL_TRUE)) break;
				}
				continue;
			}

			if (span_matches(&tok.span, STR("reserve"))) {
				size_t typeSize = 0;
				if (next_token_matches(&stream, STR("byte"), UL_TRUE)) typeSize = 1;
				else if (next_token_matches(&stream, STR("short"), UL_TRUE)) typeSize = 2;
				else if (next_token_matches(&stream, STR("int"), UL_TRUE)) typeSize = 4;
				else if (next_token_matches(&stream, STR("float"), UL_TRUE)) typeSize = 4;
				else {
					token token;
					next_token(&stream, &token, error);
					if (error->is_set) goto _compilation_error;
					ulang_error_init(error, file, token.span, "Expected byte, short, int, or float.");
					goto _compilation_error;
				}

				EXPECT_TOKEN(stream, STR_OBJ("x"), error);
				token value;
				if (!next_token(&stream, &value, error)) goto _compilation_error;
				if (value.type != TOKEN_INTEGER) {
					ulang_error_init(error, file, value.span, "Expected a number.");
					goto _compilation_error;
				}

				size_t numBytes = typeSize * token_to_int(&value);
				if (numBytes <= 0) {
					ulang_error_init(error, file, value.span, "Number of reserved bytes must be > 0.");
					goto _compilation_error;
				}
				set_label_targets(&labels, UL_LT_RESERVED_DATA, numReservedBytes);
				numReservedBytes += numBytes;
			}

			// Otherwise, we have a label
			EXPECT_TOKEN(stream, STR_OBJ(":"), error)
			ulang_label label = {tok.span, UL_LT_UNINITIALIZED, 0};
			label_array_add(&labels, label);
		} else {
			token operands[3];
			for (int i = 0; i < op->numOperands; i++) {
				token *operand = &operands[i];
				if (!next_token(&stream, operand, error)) goto _compilation_error;
				if (operand->type == TOKEN_EOF) {
					ulang_error_init(error, file, operand->span, "Expected an operand");
					goto _compilation_error;
				}

				if (i < op->numOperands - 1) EXPECT_TOKEN(stream, STR_OBJ(","), error)
			}

			opcode *fittingOp = NULL;
			while (-1) {
				fittingOp = op;
				for (int i = 0; i < op->numOperands; i++) {
					token *operand = &operands[i];
					operand_type operandType = op->operands[i];
					reg *r = token_matches_register(operand);
					if (operandType == UL_REG) {
						if (!r) {
							if (!error->is_set) ulang_error_init(error, file, operand->span, "Expected a register");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_LBL_INT_FLT) {
						if (!(operand->type == TOKEN_INTEGER ||
							  operand->type == TOKEN_FLOAT ||
							  operand->type == TOKEN_IDENTIFIER) || r) {
							if (!error->is_set)
								ulang_error_init(error, file, operand->span, "Expected an int, float, or a label");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_LBL_INT) {
						if (!(operand->type == TOKEN_INTEGER ||
							  operand->type == TOKEN_IDENTIFIER) || r) {
							if (!error->is_set)
								ulang_error_init(error, file, operand->span, "Expected an int or a label");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_OFF || operandType == UL_INT) {
						if (operand->type != TOKEN_INTEGER) {
							if (!error->is_set) ulang_error_init(error, file, operand->span, "Expected an int");
							fittingOp = NULL;
							break;
						}
					} else if (operandType == UL_FLT) {
						if (operand->type != TOKEN_FLOAT) {
							// convert ints to float
							if (operand->type == TOKEN_INTEGER) {
								operand->type = TOKEN_FLOAT;
							} else {
								if (!error->is_set) ulang_error_init(error, file, operand->span, "Expected a float");
								fittingOp = NULL;
							}
							break;
						}
					} else {
						if (!error->is_set)
							ulang_error_init(error, file, operand->span, "Internal error, unknown operand type.");
					}
				}
				if (fittingOp) break;
				if (op->code + 1 == opcodeLength) break;
				if (!string_equals(&op->name, &opcodes[op->code + 1].name)) break;
				op = &opcodes[op->code + 1];
			}
			if (!fittingOp) goto _compilation_error;
			if (error->is_set) ulang_error_free(error);

			set_label_targets(&labels, UL_LT_CODE, code.size);
			if (!emit_op(file, fittingOp, operands, &patches, &code, error)) goto _compilation_error;
		}
	}

	for (size_t i = 0; i < patches.size; i++) {
		patch *p = &patches.items[i];
		ulang_label *label = NULL;
		for (size_t j = 0; j < labels.size; j++) {
			if (string_equals(&labels.items[j].label.data, &p->label.data)) {
				label = &labels.items[j];
				break;
			}
		}
		if (!label) {
			ulang_error_init(error, file, p->label, "Unknown label.");
			goto _compilation_error;
		}

		uint32_t labelAddress = (uint32_t) label->address;
		switch (label->target) {
			case UL_LT_UNINITIALIZED:
				ulang_error_init(error, file, p->label, "Internal error: Uninitialized label target.");
				goto _compilation_error;
			case UL_LT_CODE:
				break;
			case UL_LT_DATA:
				labelAddress += code.size;
				break;
			case UL_LT_RESERVED_DATA:
				labelAddress += code.size + numReservedBytes;
				break;
		}
		memcpy(&code.items[p->patchAddress], &labelAddress, 4);
	}

	patch_array_free_inplace(&patches);
	program->code = code.items;
	program->codeLength = code.size;
	program->data = data.items;
	program->dataLength = data.size;
	program->labels = labels.items;
	program->labelsLength = labels.size;
	return UL_TRUE;

	_compilation_error:
	patch_array_free_inplace(&patches);
	label_array_free_inplace(&labels);
	byte_array_free_inplace(&code);
	byte_array_free_inplace(&data);
	return UL_FALSE;
}

void ulang_program_free(ulang_program *program) {
	ulang_free(program->code);
	ulang_free(program->data);
	ulang_free(program->labels);
}

// BOZO need to throw an error in case memory sizes are bollocks
void ulang_vm_init(ulang_vm *vm, ulang_program *program) {
	vm->memorySizeBytes = 1024 * 1024 * 32;
	vm->memory = ulang_alloc(vm->memorySizeBytes);
	memset(vm->registers, 0, sizeof(ulang_value) * 16);
	memset(vm->memory, 0, vm->memorySizeBytes);
	memcpy(vm->memory, program->code, program->codeLength);
	memcpy(vm->memory + program->codeLength, program->data, program->dataLength);
	vm->registers[15].ui = vm->memorySizeBytes - 4;
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
#define REG1_F regs[DECODE_REG(word, 0)].fl
#define REG2_F regs[DECODE_REG(word, 1)].fl
#define REG3_F regs[DECODE_REG(word, 2)].fl
#define VAL *((int32_t *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define VAL_U *((uint32_t *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define VAL_F *((float *) &vm->memory[regs[14].ui]); regs[14].ui += 4
#define SIGNUM(v) (((v) < 0) ? -1 : (((v) > 0) ? 1 : 0))
#define PC regs[14].ui
#define SP regs[15].ui

ulang_bool ulang_vm_step(ulang_vm *vm) {
	ulang_value *regs = vm->registers;
	uint32_t word;
	memcpy(&word, &vm->memory[PC], 4);
	PC += 4;
	ulang_opcode op = DECODE_OP(word);

	switch (op) {
		case HALT:
			return UL_FALSE;
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
		case INT_TO_FLOAT:
			REG2_F = REG1;
			break;
		case FLOAT_TO_INT:
			REG2 = REG1_F;
			break;
		case CMP:
			REG3 = SIGNUM(REG1 - REG2);
			break;
		case CMP_VAL: {
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
		case CMP_UNSIGNED_VAL: {
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
		case CMP_FLOAT_VAL: {
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
			uint32_t addr = VAL_U + DECODE_OFF(word);
			memcpy(&REG2_U, &vm->memory[addr], 4);
			break;
		}
		case STORE_REG: {
			uint32_t val = REG1_U;
			uint32_t addr = REG2_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &val, 4);
			break;
		}
		case STORE_VAL: {
			uint32_t val = VAL_U;
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &val, 4);
			break;
		}
		case LOAD_BYTE_REG: {
			uint32_t addr = REG1_U + DECODE_OFF(word);
			REG2_U = vm->memory[addr];
			break;
		}
		case LOAD_BYTE_VAL: {
			uint32_t addr = VAL_U + DECODE_OFF(word);
			REG2_U = vm->memory[addr];
			break;
		}
		case STORE_BYTE_REG: {
			uint8_t val = (uint8_t) REG1_U;
			uint32_t addr = REG1_U + DECODE_OFF(word);
			vm->memory[addr] = val;
			break;
		}
		case STORE_BYTE_VAL: {
			uint32_t val = (uint8_t) VAL_U;
			uint32_t addr = REG1_U + DECODE_OFF(word);
			vm->memory[addr] = val;
			break;
		}
		case LOAD_SHORT_REG: {
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&REG2_U, &vm->memory[addr], 2);
			break;
		}
		case LOAD_SHORT_VAL: {
			uint32_t addr = VAL_U + DECODE_OFF(word);
			memcpy(&REG2_U, &vm->memory[addr], 2);
			break;
		}
		case STORE_SHORT_REG: {
			uint16_t val = (uint16_t) REG1_U;
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &val, 2);
			break;
		}
		case STORE_SHORT_VAL: {
			uint16_t val = (uint16_t) VAL_U;
			uint32_t addr = REG1_U + DECODE_OFF(word);
			memcpy(&vm->memory[addr], &val, 2);
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
			uint32_t numBytes = DECODE_OFF(word);
			SP -= numBytes;
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
			SP += 4;
			memcpy(vm->memory + SP, &PC, 4);
			PC = REG1_U;
			break;
		}
		case CALL_VAL: {
			SP += 4;
			memcpy(vm->memory + SP, &PC, 4);
			PC = VAL_U;
			break;
		}
		case RETURN: {
			uint32_t addr;
			memcpy(&addr, &vm->memory[SP], 4);
			SP -= 4;
			PC = addr;
			break;
		}
		case PORT_WRITE_REG:
			break;
		case PORT_WRITE_VAL:
			break;
		case PORT_READ_REG:
			break;
		case PORT_READ_OFF:
			break;
		default:
			vm->registers[14].ui -= 4; // reset PC to the unknown instruction.
			return UL_FALSE;
	}
	return UL_TRUE;
}

void ulang_vm_print(ulang_vm *vm) {
	ulang_value *regs = vm->registers;
	for (int i = 0; i < 16; i++) {
		printf("%.*s: %i (0x%x), %f\n", (int) registers[i].name.length, registers[i].name.data, regs[i].i, regs[i].i,
			   regs[i].fl);
	}
}

void ulang_vm_free(ulang_vm *vm) {
	ulang_free(vm->memory);
}
