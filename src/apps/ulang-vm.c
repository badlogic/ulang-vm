#include <stdlib.h>
#include <stdio.h>
#include <ulang.h>
#include <MiniFB.h>

static struct mfb_window *window;

static ulang_bool syscallHandler(uint32_t intNum, ulang_vm *vm) {
	switch (intNum) {
		case 0:
			return ulang_vm_debug(vm);
		case 1: {
			uint32_t offset = ulang_vm_pop_uint(vm);
			uint8_t *addr = &vm->memory[offset];
			if (mfb_update_ex(window, addr, 320, 240) < 0) return UL_FALSE;
			if (!mfb_wait_sync(window)) return UL_FALSE;
			return UL_TRUE;
		}
		case 2: {
			int numArgs = ulang_vm_pop_uint(vm);
			for (int i = 0; i < numArgs; i++) {
				int argType = ulang_vm_pop_uint(vm);
				switch (argType) {
					case 0:
						printf("%i", ulang_vm_pop_int(vm));
						break;
					case 1:
						printf("0x%x", ulang_vm_pop_int(vm));
						break;
					case 2:
						printf("%f", ulang_vm_pop_float(vm));
						break;
					case 3:
						printf("%s", &vm->memory[ulang_vm_pop_uint(vm)]);
						break;
					default:
						break;
				}
			}
			printf("\n");
		}
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
	vm.syscalls[0] = syscallHandler;
	vm.syscalls[1] = syscallHandler;
	vm.syscalls[2] = syscallHandler;
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
