#include <ulang.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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

typedef enum operand_type {
	UL_NIL = 0,  // No operand
	UL_REG, // Register
	UL_VAL, // Label or value
	UL_OFF, // Offset
} operand_type;

typedef struct opcode {
	ulang_string name;
	operand_type operands[3];
	int numOperands;
	ulang_bool hasValueOperand;
	int code;
} opcode;

opcode opcodes[] = {
		{STR_OBJ("halt")},
		{STR_OBJ("add"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("sub"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("mul"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("div"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("div_unsigned"),       {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("remainder"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("remainder_unsigned"), {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("add_float"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("sub_float"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("mul_float"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("div_float"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("cos_float"),          {UL_REG, UL_REG}},
		{STR_OBJ("sin_float"),          {UL_REG, UL_REG}},
		{STR_OBJ("atan2_float"),        {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("sqrt_float"),         {UL_REG, UL_REG}},
		{STR_OBJ("pow_float"),          {UL_REG, UL_REG}},
		{STR_OBJ("int_to_float"),       {UL_REG, UL_REG}},
		{STR_OBJ("float_to_int"),       {UL_REG, UL_REG}},
		{STR_OBJ("cmp"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("cmp_unsigned"),       {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("cmp_float"),          {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("not"),                {UL_REG, UL_REG}},
		{STR_OBJ("and"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("or"),                 {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("xor"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("shl"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("shr"),                {UL_REG, UL_REG, UL_REG}},
		{STR_OBJ("jump"),               {UL_VAL}},
		{STR_OBJ("jump_equal"),         {UL_REG, UL_VAL}},
		{STR_OBJ("jump_not_equal"),     {UL_REG, UL_VAL}},
		{STR_OBJ("jump_less"),          {UL_REG, UL_VAL}},
		{STR_OBJ("jump_greater"),       {UL_REG, UL_VAL}},
		{STR_OBJ("jump_less_equal"),    {UL_REG, UL_VAL}},
		{STR_OBJ("jump_greater_equal"), {UL_REG, UL_VAL}},

		{STR_OBJ("move"),               {UL_REG, UL_REG}},
		{STR_OBJ("move"),               {UL_VAL, UL_REG}},

		{STR_OBJ("load"),               {UL_VAL, UL_OFF, UL_REG}},
		{STR_OBJ("load"),               {UL_REG, UL_OFF, UL_REG}},

		{STR_OBJ("store"),              {UL_REG, UL_VAL, UL_OFF}},
		{STR_OBJ("store"),              {UL_REG, UL_REG, UL_OFF}},

		{STR_OBJ("load_byte"),          {UL_VAL, UL_OFF, UL_REG}},
		{STR_OBJ("load_byte"),          {UL_REG, UL_OFF, UL_REG}},

		{STR_OBJ("store_byte"),         {UL_REG, UL_VAL, UL_OFF}},
		{STR_OBJ("store_byte"),         {UL_REG, UL_REG, UL_OFF}},

		{STR_OBJ("load_short"),         {UL_VAL, UL_OFF, UL_REG}},
		{STR_OBJ("load_short"),         {UL_REG, UL_OFF, UL_REG}},

		{STR_OBJ("store_short"),        {UL_REG, UL_VAL, UL_OFF}},
		{STR_OBJ("store_short"),        {UL_REG, UL_REG, UL_OFF}},

		{STR_OBJ("push"),               {UL_VAL}},
		{STR_OBJ("push"),               {UL_REG}},

		{STR_OBJ("stackalloc"),         {UL_OFF}},

		{STR_OBJ("pop"),                {UL_REG}},
		{STR_OBJ("pop"),                {UL_OFF}},

		{STR_OBJ("call"),               {UL_VAL}},
		{STR_OBJ("call"),               {UL_REG}},

		{STR_OBJ("return"),             {UL_OFF}},

		{STR_OBJ("port_write"),         {UL_REG, UL_OFF}},
		{STR_OBJ("port_write"),         {UL_VAL, UL_OFF}},

		{STR_OBJ("port_read"),          {UL_OFF, UL_REG}},
		{STR_OBJ("port_read"),          {UL_REG, UL_REG}},
};

static int opcodeLength = sizeof(opcodes) / sizeof(opcode);

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
	if (a->length != b->length) return ULANG_FALSE;
	char *aData = a->data;
	char *bData = b->data;
	size_t length = a->length;
	for (size_t i = 0; i < length; i++) {
		if (aData[i] != bData[i]) return ULANG_FALSE;
	}
	return ULANG_TRUE;
}

static void init_opcodes_and_registers() {
	for (int i = 0; i < opcodeLength; i++) {
		opcode *opcode = &opcodes[i];
		opcode->code = i;
		for (int j = 0; j < 3; j++) {
			if (opcode->operands[j] == UL_NIL) break;
			if (opcode->operands[j] == UL_VAL) opcode->hasValueOperand = ULANG_TRUE;
			opcode->numOperands++;
		}
	}

	for (size_t i = 0; i < (sizeof(registers) / sizeof(reg)); i++) {
		registers[i].index = (int) i;
	}
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
	error->is_set = ULANG_TRUE;
}

void ulang_error_free(ulang_error *error) {
	ulang_free(error->message.data);
	error->is_set = ULANG_FALSE;
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
		if (j >= stream->end) return ULANG_FALSE;
		uint32_t c = next_utf8_character(sourceData, &j);
		if ((unsigned char) needleData[i] != c) return ULANG_FALSE;
	}
	if (consume) stream->index += needleLength;
	return ULANG_TRUE;
}

static ulang_bool match_digit(character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return ULANG_FALSE;
	char c = stream->data->data[stream->index];
	if (c >= '0' && c <= '9') {
		if (consume) stream->index++;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_hex(character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	char c = stream->data->data[stream->index];
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
		stream->index++;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_identifier_start(character_stream *stream) {
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

static ulang_bool match_identifier_part(character_stream *stream) {
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

static void skip_white_space(character_stream *stream) {
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
	if (span->data.length != length) return ULANG_FALSE;

	const char *sourceData = span->data.data;
	for (uint32_t i = 0; i < length; i++) {
		if (sourceData[i] != needle[i]) return ULANG_FALSE;
	}
	return ULANG_TRUE;
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
	if (!has_more(stream)) return ULANG_FALSE;
	skip_white_space(stream);
	return has_more(stream);
}

static ulang_bool next_token(character_stream *stream, token *token, ulang_error *error) {
	if (!has_more_tokens(stream)) {
		token->span.startLine = 0;
		token->span.endLine = 0;
		token->type = TOKEN_EOF;
		return ULANG_TRUE;
	}
	start_span(stream);

	if (match(stream, "-", ULANG_TRUE) || match_digit(stream, ULANG_FALSE)) {
		token->type = TOKEN_INTEGER;
		if (match(stream, "0x", ULANG_TRUE)) {
			while (match_hex(stream));
		} else {
			while (match_digit(stream, ULANG_TRUE));
			if (match(stream, ".", ULANG_TRUE)) {
				token->type = TOKEN_FLOAT;
				while (match_digit(stream, ULANG_TRUE));
			}
		}
		if (match(stream, "b", ULANG_TRUE)) {
			if (token->type == TOKEN_FLOAT) {
				ulang_error_init(error, stream->data, end_span(stream), "Byte literal can not have a decimal point.");
				return ULANG_FALSE;
			}
			token->type = TOKEN_INTEGER;
		}
		token->span = end_span(stream);
		return ULANG_TRUE;
	}

	// String literal
	if (match(stream, "\"", ULANG_TRUE)) {
		ulang_bool matchedEndQuote = ULANG_FALSE;
		token->type = TOKEN_STRING;
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
		token->type = TOKEN_IDENTIFIER;
		token->span = end_span(stream);
		return ULANG_TRUE;
	}

	// Else check for "simple" tokens made up of
	// 1 character literals, like ".", ",", or ";",
	consume(stream);
	token->type = TOKEN_SPECIAL_CHAR;
	token->span = end_span(stream);
	return ULANG_TRUE;
}

static ulang_bool
next_token_matches(character_stream *stream, const char *needle, size_t size, ulang_bool consume) {
	character_stream stream_copy = *stream;
	ulang_error error;
	token token;
	if (!next_token(&stream_copy, &token, &error)) return ULANG_FALSE;
	if (token.type == TOKEN_EOF) return ULANG_FALSE;
	ulang_bool matches = span_matches(&token.span, needle, size);
	if (matches && consume) next_token(stream, &token, &error);
	return matches;
}

static ulang_bool
next_token_matches_type(character_stream *stream, token *token, token_type type, ulang_bool consume) {
	character_stream stream_copy = *stream;
	ulang_error error;
	if (!next_token(&stream_copy, token, &error)) return ULANG_FALSE;
	if (token->type != type) return ULANG_FALSE;
	if (consume) next_token(stream, token, &error);
	return ULANG_TRUE;
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
	for (int i = 0; i < opcodeLength; i++) {
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
		*((int16_t *) &code->items[code->size]) = (int16_t) val;
		code->size += 2;
	}
}

static void emit_int(byte_array *code, token *value, int repeat) {
	int val = token_to_int(value);
	byte_array_ensure(code, 4 * repeat);
	for (int i = 0; i < repeat; i++) {
		*((int32_t *) &code->items[code->size]) = (int32_t) val;
		code->size += 4;
	}
}

static void emit_float(byte_array *code, token *value, int repeat) {
	float val = token_to_float(value);
	byte_array_ensure(code, 4 * repeat);
	for (int i = 0; i < repeat; i++) {
		*((float *) &code->items[code->size]) = val;
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

#define EMIT_OP(word, op) word |= op
#define EMIT_REG(word, reg, index) word |= (((reg) & 0xf) << (6 + 4 * (index)))
#define EMIT_OFF(word, offset) word |= (((offset) & 0x3fff) << 18)

static ulang_bool
emit_op(ulang_file *file, opcode *op, token operands[3], patch_array *patches, byte_array *code, ulang_error *error) {
	uint32_t word1 = 0;
	uint32_t word2 = 0;

	EMIT_OP(word1, op->code);

	int numEmittedRegs = 0;
	for (int i = 0; i < op->numOperands; i++) {
		token *operandToken = &operands[i];
		operand_type operandType = op->operands[i];
		switch (operandType) {
			case UL_REG:
				EMIT_REG(word1, token_matches_register(operandToken)->index, numEmittedRegs);
				break;
			case UL_OFF:
				EMIT_OFF(word1, token_to_int(operandToken));
				break;
			case UL_VAL:
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
										 "Internal error, unexpected token for value operand.");
						return ULANG_FALSE;
				}
				break;
			default:
				ulang_error_init(error, file, operandToken->span,
								 "Internal error, unexpected token for value operand.");
				return ULANG_FALSE;
		}
	}

	byte_array_ensure(code, 4 + (op->hasValueOperand ? 4 : 0));
	memcpy(&code->items[code->size], &word1, 4);
	code->size += 4;
	if (op->hasValueOperand) {
		memcpy(&code->items[code->size], &word2, 4);
		code->size += 4;
	}

	return ULANG_TRUE;
}

static void parse_repeat(character_stream *stream, int *numRepeat, ulang_error *error) {
	if (!next_token_matches(stream, STR("@"), ULANG_TRUE)) return;
	token token;
	if (!next_token_matches_type(stream, &token, TOKEN_INTEGER, ULANG_TRUE)) {
		ulang_error_init(error, stream->data, token.span, "Expected an integer value after 'x'.");
		return;
	}
	*numRepeat = token_to_int(&token);
	if (*numRepeat < 0) {
		ulang_error_init(error, stream->data, token.span, "Number of repetitions can not be negative.");
		return;
	}
}

ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error) {
	character_stream stream;
	character_stream_init(&stream, file);
	init_opcodes_and_registers();
	error->is_set = ULANG_FALSE;

	patch_array patches;
	patch_array_init_inplace(&patches, 16);
	label_array labels;
	label_array_init_inplace(&labels, 16);
	byte_array code;
	byte_array_init_inplace(&code, 16);

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
					if (value.type == TOKEN_INTEGER) emit_byte(&code, &value, numRepeat);
					else emit_string(&code, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), ULANG_TRUE)) break;
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
					emit_short(&code, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), ULANG_TRUE)) break;
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
					emit_int(&code, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), ULANG_TRUE)) break;
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
					emit_float(&code, &value, numRepeat);
					if (!next_token_matches(&stream, STR(","), ULANG_TRUE)) break;
				}
				continue;
			}

			// Otherwise, we have a label
			EXPECT_TOKEN(stream, STR_OBJ(":"), error)
			ulang_label label = {tok.span, (int) code.size};
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
					if (operandType == UL_REG && !r) {
						if (!error->is_set) ulang_error_init(error, file, operand->span, "Expected a register");
						fittingOp = NULL;
						break;
					}

					if (operandType == UL_VAL &&
						(!(operand->type == TOKEN_INTEGER ||
						   operand->type == TOKEN_FLOAT ||
						   operand->type == TOKEN_IDENTIFIER) || r)) {
						if (!error->is_set) {
							ulang_error_init(error, file, operand->span, "Expected a number or a label");
						}
						fittingOp = NULL;
						break;
					}

					if (operandType == UL_OFF && operand->type != TOKEN_INTEGER) {
						if (!error->is_set) ulang_error_init(error, file, operand->span, "Expected a number");
						fittingOp = NULL;
						break;
					}
				}
				if (fittingOp) break;
				if (op->code + 1 == opcodeLength) break;
				if (!string_equals(&op->name, &opcodes[op->code + 1].name)) break;
				op = &opcodes[op->code + 1];
			}
			if (!fittingOp) goto _compilation_error;
			if (error->is_set) ulang_error_free(error);
			if (!emit_op(file, fittingOp, operands, &patches, &code, error)) goto _compilation_error;
		}
	}

	for (size_t i = 0; i < patches.size; i++) {
		patch *p = &patches.items[i];
		ulang_label *l = NULL;
		for (size_t j = 0; j < labels.size; j++) {
			if (string_equals(&labels.items[j].label.data, &p->label.data)) {
				l = &labels.items[j];
				break;
			}
		}
		if (!l) {
			ulang_error_init(error, file, p->label, "Unknown label.");
			goto _compilation_error;
		}

		uint32_t labelAddress = (uint32_t) l->codeAddress;
		memcpy(&code.items[p->patchAddress], &labelAddress, 4);
	}

	patch_array_free_inplace(&patches);
	program->code = code.items;
	program->codeLength = code.size;
	program->labels = labels.items;
	program->labelsLength = labels.size;
	return ULANG_TRUE;

	_compilation_error:
	patch_array_free_inplace(&patches);
	label_array_free_inplace(&labels);
	byte_array_free_inplace(&code);
	return ULANG_FALSE;
}

void ulang_program_free(ulang_program *program) {
	ulang_free(program->code);
	ulang_free(program->labels);
}
