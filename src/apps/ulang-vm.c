#include <stdlib.h>
#include <stdio.h>
#include <ulang.h>
#include <MiniFB.h>
#include <string.h>

static struct mfb_window *window;

static ulang_bool interruptHandler(uint32_t intNum, ulang_vm *vm) {
	if (intNum == 1) {
		uint32_t offset = ulang_vm_pop_uint(vm);
		uint8_t *addr = &vm->memory[offset];
		if (mfb_update_ex(window, addr, 320, 240) < 0) return UL_FALSE;
		// if (!mfb_wait_sync(window)) return UL_FALSE;
	} else if (intNum == 2) {
		printf("%i\n", ulang_vm_pop_uint(vm));
	}
	return UL_TRUE;
}

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

	window = mfb_open("ulang-vm", 640, 480);
	if (!window) {
		printf("Couldn't create window.");
		ulang_error_print(&error);
		ulang_error_free(&error);
		ulang_file_free(&file);
		return -1;
	}

	ulang_vm vm = {0};
	ulang_vm_init(&vm, &program);
	vm.interruptHandlers[1] = interruptHandler;
	vm.interruptHandlers[2] = interruptHandler;
	while (ulang_vm_step(&vm));
	if (vm.error.is_set) ulang_error_print(&vm.error);
	ulang_vm_print(&vm);

	mfb_close(window);

	ulang_vm_free(&vm);
	ulang_program_free(&program);
	ulang_error_free(&error);
	ulang_file_free(&file);
	return 0;
}
