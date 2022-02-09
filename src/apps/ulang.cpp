#include <cstdio>
#include <ulang.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: ulang <file>");
		return -1;
	}

	ulang_file file;
	if (!ulang_file_read(argv[1], &file)) {
		printf("Couldn't read file %s.\n", argv[1]);
		return -1;
	}

	ulang_error error = {};
	ulang_program program = {};
	if (!ulang_compile(&file, &program, &error)) {
		ulang_error_print(&error);
		ulang_error_free(&error);
		ulang_file_free(&file);
		return -1;
	}

	ulang_file_free(&file);
	ulang_program_free(&program);
	ulang_print_memory();
	return 0;
}