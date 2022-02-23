#include <stdlib.h>
#include <stdio.h>
#include <ulang.h>
#include <MiniFB.h>

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

	ulang_error error = {0};
	ulang_program program = {0};
	if (!ulang_compile(&file, &program, &error)) {
		ulang_error_print(&error);
		ulang_error_free(&error);
		ulang_file_free(&file);
		return -1;
	}

	ulang_vm vm = {0};
	ulang_vm_init(&vm, &program);
	while (ulang_vm_step(&vm));
	if (vm.error.is_set) ulang_error_print(&vm.error);

	ulang_vm_free(&vm);
	ulang_program_free(&program);
	ulang_file_free(&file);
	return 0;
}
