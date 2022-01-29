#include <ulang.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define SOKOL_IMPL

#include "sokol_time.h"

void *ulang_alloc(int numBytes) {
	return malloc(numBytes);
}

void ulang_free(void *ptr) {
	free(ptr);
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

static ulang_bool isTimeSetup = ULANG_FALSE;

double ulang_time_millis() {
	if (!isTimeSetup) {
		stm_setup();
		isTimeSetup = ULANG_TRUE;
	}
	uint64_t time = stm_now();
	return stm_ms(time);
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
	} while (data[*index] && !(((data[*index]) & 0xC0) != 0x80));
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

static ulang_bool match_hex(ulang_character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return ULANG_FALSE;
	char c = stream->data->data[stream->index];
	if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
		if (consume) stream->index++;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_identifier_start(ulang_character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return ULANG_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xc0) {
		if (consume) stream->index = idx;
		return ULANG_TRUE;
	}
	return ULANG_FALSE;
}

static ulang_bool match_identifier_part(ulang_character_stream *stream, ulang_bool consume) {
	if (!has_more(stream)) return ULANG_FALSE;
	uint32_t idx = stream->index;
	const char *sourceData = stream->data->data;
	uint32_t c = next_utf8_character(sourceData, &idx);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9') || c >= 0x80) {
		if (consume) stream->index = idx;
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

void ulang_error_init(ulang_error *error, ulang_file *file, ulang_span span, const char *msg, ...) {
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

ulang_bool has_more_tokens(ulang_character_stream *stream) {
	if (!has_more(stream)) return ULANG_FALSE;
	skip_white_space(stream);
	if (has_more(stream)) return ULANG_TRUE;
}

ulang_bool ulang_compile(ulang_file *file, ulang_program *program, ulang_error *error) {
	(void) program;

	error->message.data = NULL;
	ulang_character_stream stream;
	ulang_character_stream_init(&stream, file);

	error->file = file;
	return ULANG_FALSE;
}

void ulang_program_free(ulang_program *program) {
	ulang_free(program->data);
	ulang_free(program->code);
}
