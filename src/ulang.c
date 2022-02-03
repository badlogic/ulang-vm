#include <ulang.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ULANG_STR(str) str, sizeof(str) - 1
#define ULANG_STR_OBJ(str) (ulang_string){ str, sizeof(str) - 1 }
#define ULANG_MAX(a, b) (a > b ? a : b)

#define ULANG_ARRAY_IMPLEMENT(name, itemType) \
    typedef struct name { size_t size; size_t capacity; itemType* items; } name; \
    name* name##_new(size_t initialCapacity) { \
        name* array = (name *)ulang_alloc(sizeof(name)); \
        array->size = 0; \
        array->capacity = initialCapacity; \
        array->items = (itemType*)ulang_alloc(sizeof(itemType) * initialCapacity); \
        return array; \
    } \
    void name##_dispose(name *self) { \
        ulang_free(self->items); \
        ulang_free(self); \
    } \
    void name##_add(name* self, itemType value) { \
        if (self->size == self->capacity) { \
            self->capacity = ULANG_MAX(8, (size_t)(self->size * 1.75f)); \
            self->items = (itemType*)ulang_realloc(self->items, sizeof(itemType) * self->capacity); \
        } \
        self->items[self->size++] = value; \
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
		return ULANG_FALSE;
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
	return ULANG_TRUE;
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

typedef struct {
	ulang_file *data;
	uint32_t index;
	uint32_t end;
	uint32_t line;
	uint32_t spanStart;
	uint32_t spanLineStart;
} ulang_character_stream;

static void ulang_character_stream_init(ulang_character_stream *stream, ulang_file *fileData) {
	stream->data = fileData;
	stream->index = 0;
	stream->line = 1;
	stream->end = (uint32_t) fileData->length;
	stream->spanStart = 0;
	stream->spanLineStart = 0;
}

static ulang_bool has_more(ulang_character_stream *stream) {
	return stream->index < stream->data->length;
}

static uint32_t consume(ulang_character_stream *stream) {
	return next_utf8_character(stream->data->data, &stream->index);
}

static uint32_t peek(ulang_character_stream *stream) {
	uint32_t i = stream->index;
	return next_utf8_character(stream->data->data, &i);
}

static ulang_bool match(ulang_character_stream *stream, const char *needleData, ulang_bool consume) {
	uint32_t needleLength = 0;
	const char *sourceData = stream->data->data;
	for (uint32_t i = 0, j = stream->index; needleData[i] != 0; i++, needleLength++) {
		if (j >= stream->end) return ULANG_FALSE;
		uint32_t c = next_utf8_character(sourceData, &j);
		if ((unsigned char) needleData[i] != c) return ULANG_FALSE;
	}
	if (consume) stream->index += needleLength;
	return ULANG_TRUE;
}

static ulang_bool match_digit(ulang_character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return ULANG_FALSE;
	char c = stream->data->data[stream->index];
	if (c >= '0' && c <= '9') {
		if (consume) stream->index++;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_hex(ulang_character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	char c = stream->data->data[stream->index];
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
		stream->index++;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_identifier_start(ulang_character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xc0) {
		stream->index = idx;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_identifier_part(ulang_character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9') || c >= 0x80) {
		stream->index = idx;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static void skip_white_space(ulang_character_stream *stream) {
	const char *sourceData = stream->data->data;
	while (ULANG_TRUE) {
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

static void start_span(ulang_character_stream *stream) {
	stream->spanStart = stream->index;
	stream->spanLineStart = stream->line;
}

static ulang_span end_span(ulang_character_stream *stream) {
	ulang_span span;
	span.data.data = stream->data->data + stream->spanStart;
	span.data.length = stream->index - stream->spanStart;
	span.startLine = stream->spanLineStart;
	span.endLine = stream->line;
	return span;
}

static ulang_bool span_matches(ulang_span *span, const char *needle, size_t length) {
	if (span->data.length != length) return ULANG_FALSE;

	const char *sourceData = span->data.data;
	for (uint32_t i = 0; i < length; i++) {
		if (sourceData[i] != needle[i]) return ULANG_FALSE;
	}
	return ULANG_TRUE;
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
}

void ulang_error_free(ulang_error *error) {
	ulang_free(error->message.data);
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

typedef enum ulang_token_type {
	ULANG_TOKEN_INTEGER,
	ULANG_TOKEN_FLOAT,
	ULANG_TOKEN_STRING,
	ULANG_TOKEN_IDENTIFIER,
	ULANG_TOKEN_SPECIAL_CHAR,
	ULANG_TOKEN_EOF
} ulang_token_type;

typedef struct {
	ulang_span span;
	ulang_token_type type;
} ulang_token;

static ulang_bool has_more_tokens(ulang_character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	skip_white_space(stream);
	return has_more(stream);
}

static ulang_bool next_token(ulang_character_stream *stream, ulang_token *token, ulang_error *error) {
	if (!has_more_tokens(stream)) {
		token->span.startLine = 0;
		token->span.endLine = 0;
		token->type = ULANG_TOKEN_EOF;
		return ULANG_TRUE;
	}
	start_span(stream);

	if (match(stream, "-", ULANG_TRUE) || match_digit(stream, ULANG_FALSE)) {
		token->type = ULANG_TOKEN_INTEGER;
		if (match(stream, "0x", ULANG_TRUE)) {
			while (match_hex(stream));
		} else {
			while (match_digit(stream, ULANG_TRUE));
			if (match(stream, ".", ULANG_TRUE)) {
				token->type = ULANG_TOKEN_FLOAT;
				while (match_digit(stream, ULANG_TRUE));
			}
		}
		if (match(stream, "b", ULANG_TRUE)) {
			if (token->type == ULANG_TOKEN_FLOAT) {
				ulang_error_init(error, stream->data, end_span(stream), "Byte literal can not have a decimal point.");
				return ULANG_FALSE;
			}
			token->type = ULANG_TOKEN_INTEGER;
		}
		token->span = end_span(stream);
		return ULANG_TRUE;
	}

	// String literal
	if (match(stream, "\"", ULANG_TRUE)) {
		ulang_bool matchedEndQuote = ULANG_FALSE;
		token->type = ULANG_TOKEN_STRING;
		while (has_more(stream)) {
			// Note: escape sequences like \n are parsed in the AST
			if (match(stream, "\\", ULANG_TRUE)) {
				consume(stream);
			}
			if (match(stream, "\"", ULANG_TRUE)) {
				matchedEndQuote = ULANG_TRUE;
				break;
			}
			if (match(stream, "\n", ULANG_FALSE)) {
				ulang_error_init(error, stream->data, end_span(stream),
								 "String literal is not closed by double quote");
				return ULANG_FALSE;
			}
			consume(stream);
		}
		if (!matchedEndQuote) {
			ulang_error_init(error, stream->data, end_span(stream), "String literal is not closed by double quote");
			return ULANG_FALSE;
		}
		token->span = end_span(stream);
		return ULANG_TRUE;
	}

	// Identifier or keyword
	if (match_identifier_start(stream)) {
		while (match_identifier_part(stream));
		token->type = ULANG_TOKEN_IDENTIFIER;
		token->span = end_span(stream);
		return ULANG_TRUE;
	}

	// Else check for "simple" tokens made up of
	// 1 character literals, like ".", ",", or ";",
	consume(stream);
	token->type = ULANG_TOKEN_SPECIAL_CHAR;
	token->span = end_span(stream);
	return ULANG_TRUE;
}

typedef enum ulang_operand_type {
	UL_NIL = 0,  // No operand
	UL_REG, // Register
	UL_VAL, // Label or value
	UL_OFF, // Offset
} ulang_operand_type;

typedef struct ulang_opcode {
	ulang_string name;
	ulang_operand_type operands[3];
	int numOperands;
	int code;
} ulang_opcode;

ulang_opcode opcodes[] = {
		{ULANG_STR_OBJ("halt")},
		{ULANG_STR_OBJ("add"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("sub"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("mul"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("div"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("div_unsigned"),       {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("remainder"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("remainder_unsigned"), {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("add_float"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("sub_float"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("mul_float"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("div_float"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("cos_float"),          {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("sin_float"),          {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("atan2_float"),        {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("sqrt_float"),         {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("pow_float"),          {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("int_to_float"),       {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("float_to_int"),       {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("cmp"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("cmp_unsigned"),       {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("cmp_float"),          {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("not"),                {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("and"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("or"),                 {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("xor"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("shl"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("shr"),                {UL_REG, UL_REG, UL_REG}},
		{ULANG_STR_OBJ("jump"),               {UL_VAL}},
		{ULANG_STR_OBJ("jump_equal"),         {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("jump_not_equal"),     {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("jump_less"),          {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("jump_greater"),       {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("jump_less_equal"),    {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("jump_greater_equal"), {UL_REG, UL_VAL}},
		{ULANG_STR_OBJ("move"),               {UL_REG, UL_REG}},
		{ULANG_STR_OBJ("move"),               {UL_VAL, UL_REG}},
		{ULANG_STR_OBJ("load"),               {UL_VAL, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("load"),               {UL_REG, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("store"),              {UL_REG, UL_VAL, UL_OFF}},
		{ULANG_STR_OBJ("store"),              {UL_REG, UL_REG, UL_OFF}},
		{ULANG_STR_OBJ("load_byte"),          {UL_VAL, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("load_byte"),          {UL_REG, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("store_byte"),         {UL_REG, UL_VAL, UL_OFF}},
		{ULANG_STR_OBJ("store_byte"),         {UL_REG, UL_REG, UL_OFF}},
		{ULANG_STR_OBJ("load_short"),         {UL_VAL, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("load_short"),         {UL_REG, UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("store_short"),        {UL_REG, UL_VAL, UL_OFF}},
		{ULANG_STR_OBJ("store_short"),        {UL_REG, UL_REG, UL_OFF}},
		{ULANG_STR_OBJ("push"),               {UL_VAL, UL_NIL}},
		{ULANG_STR_OBJ("push"),               {UL_REG, UL_NIL}},
		{ULANG_STR_OBJ("stackalloc"),         {UL_OFF, UL_NIL}},
		{ULANG_STR_OBJ("pop"),                {UL_REG, UL_NIL}},
		{ULANG_STR_OBJ("pop"),                {UL_OFF, UL_NIL}},
		{ULANG_STR_OBJ("call"),               {UL_VAL}},
		{ULANG_STR_OBJ("call"),               {UL_REG}},
		{ULANG_STR_OBJ("return"),             {UL_OFF}},
		{ULANG_STR_OBJ("port_write"),         {UL_REG, UL_OFF}},
		{ULANG_STR_OBJ("port_write"),         {UL_VAL, UL_OFF}},
		{ULANG_STR_OBJ("port_read"),          {UL_OFF, UL_REG}},
		{ULANG_STR_OBJ("port_read"),          {UL_REG, UL_REG}},
};

typedef struct ulang_register {
	ulang_string name;
	int index;
} ulang_register;

ulang_register registers[] = {
		{{ULANG_STR("r1")}},
		{{ULANG_STR("r2")}},
		{{ULANG_STR("r3")}},
		{{ULANG_STR("r4")}},
		{{ULANG_STR("r5")}},
		{{ULANG_STR("r6")}},
		{{ULANG_STR("r7")}},
		{{ULANG_STR("r8")}},
		{{ULANG_STR("r9")}},
		{{ULANG_STR("r10")}},
		{{ULANG_STR("r11")}},
		{{ULANG_STR("r12")}},
		{{ULANG_STR("r13")}},
		{{ULANG_STR("r14")}},
		{{ULANG_STR("pc")}},
		{{ULANG_STR("sp")}}
};

static void init_opcodes_and_registers() {
	for (int i = 0; i < (int) (sizeof(opcodes) / sizeof(ulang_opcode)); i++) {
		ulang_opcode *opcode = &opcodes[i];
		opcode->code = i;
		for (int j = 0; j < 3; j++) {
			if (opcode->operands[j] == UL_NIL) break;
			opcode->numOperands++;
		}
	}

	for (int i = 0; i < (int) (sizeof(registers) / sizeof(ulang_register)); i++) {
		registers[i].index = i;
	}
}

static ulang_register *matches_register(ulang_span *span) {
	for (int i = 0; i < (int) (sizeof(registers) / sizeof(ulang_register)); i++) {
		if (span_matches(span, registers[i].name.data, registers[i].name.length)) {
			return &registers[i];
		}
	}
	return NULL;
}

static ulang_opcode *matches_opcode(ulang_span *span) {
	for (int i = 0; i < (int) (sizeof(opcodes) / sizeof(ulang_opcode)); i++) {
		if (span_matches(span, opcodes[i].name.data, opcodes[i].name.length)) {
			return &opcodes[i];
		}
	}
	return NULL;
}

#define NEXT_TOKEN_CHECK(stream, token, error) if (!next_token(&(stream), &(token), error)) goto _compilation_error;

#define EXPECT_TOKEN_CHECK(stream, str, error) { \
    ulang_token _token; \
    if (!next_token(&(stream), &_token, error)) goto _compilation_error; \
    if (_token.type == ULANG_TOKEN_EOF) { \
        ulang_error_init(error, (stream).data, _token.span, "Unexpected end of file, expected '%.*s'", (str).length, (str).data); \
        goto _compilation_error; \
    } \
    if (!span_matches(&_token.span, (str).data, (str).length)) { \
        ulang_error_init(error, (stream).data, _token.span, "Expected %.*s", (str).length, (str).data); \
        goto _compilation_error; \
    } \
}

typedef struct ulang_patch {
	ulang_span label;
	int patchAddress;
} ulang_patch;
ULANG_ARRAY_IMPLEMENT(ulang_patch_array, ulang_patch)

typedef struct ulang_label {
	ulang_span label;
	int codeAddress;
} ulang_label;
ULANG_ARRAY_IMPLEMENT(ulang_label_array, ulang_label)

ULANG_ARRAY_IMPLEMENT(ulang_byte_array, int32_t)

ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error) {
	ulang_character_stream stream;
	ulang_character_stream_init(&stream, file);
	init_opcodes_and_registers();

	ulang_patch_array *patches = ulang_patch_array_new(16);
	ulang_label_array *labels = ulang_label_array_new(16);
	ulang_byte_array *code = ulang_byte_array_new(16);

	while (has_more_tokens(&stream)) {
		ulang_token token;
		NEXT_TOKEN_CHECK(stream, token, error)

		ulang_opcode *opcode = matches_opcode(&token.span);
		if (!opcode) {
			if (token.type != ULANG_TOKEN_IDENTIFIER) {
				ulang_error_init(error, file, token.span, "Expected a label or data definition");
				goto _compilation_error;
			}

			if (peek(&stream) == ':') {
				EXPECT_TOKEN_CHECK(stream, ULANG_STR_OBJ(":"), error)
				ulang_label label = {token.span, (int) code->size};
				ulang_label_array_add(labels, label);
			}

		} else {
			ulang_token operands[3];
			for (int i = 0; i < opcode->numOperands; i++) {
				ulang_token *operand = &operands[i];
				ulang_operand_type operandType = opcode->operands[i];
				NEXT_TOKEN_CHECK(stream, *operand, error)
				if (operand->type == ULANG_TOKEN_EOF) {
					ulang_error_init(error, file, operand->span, "Expected an operand");
					goto _compilation_error;
				}

				if (operandType == UL_REG && !matches_register(&operand->span)) {
					ulang_error_init(error, file, operand->span, "Expected a register");
					goto _compilation_error;
				}

				if (operandType == UL_VAL &&
					!(operand->type == ULANG_TOKEN_INTEGER || operand->type == ULANG_TOKEN_FLOAT ||
					  operand->type == ULANG_TOKEN_IDENTIFIER)) {
					ulang_error_init(error, file, operand->span, "Expected a number or a label");
					goto _compilation_error;
				}

				if (operandType == UL_OFF && operand->type != ULANG_TOKEN_INTEGER) {
					ulang_error_init(error, file, operand->span, "Expected a number");
					goto _compilation_error;
				}

				if (i < opcode->numOperands - 1) EXPECT_TOKEN_CHECK(stream, ULANG_STR_OBJ(","), error)
			}
		}
	}

	ulang_patch_array_dispose(patches);
	ulang_label_array_dispose(labels);
	ulang_byte_array_dispose(code);
	return ULANG_TRUE;

	_compilation_error:
	ulang_patch_array_dispose(patches);
	ulang_label_array_dispose(labels);
	ulang_byte_array_dispose(code);
	return ULANG_FALSE;
}

void ulang_program_free(ulang_program *program) {
	ulang_free(program->code);
}
