#include <stdlib.h>
#include <stdio.h>
#include <ulang.h>
#include <MiniFB.h>
#define SOKOL_IMPL
#include <apps/sokol_time.h>

static struct mfb_window *window;
static uint64_t tickStart;

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
			while (true) {
				int argType = ulang_vm_pop_uint(vm);
				if (argType == 6) break;
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
					case 4:
						printf(" ");
						break;
					case 5:
						printf("\n");
						break;
					default:
						break;
				}
			}
			printf("\n");
		}
    case 3: {
        ulang_vm_push_int(vm, mfb_get_mouse_x(window) / 2);
        ulang_vm_push_int(vm, mfb_get_mouse_y(window) / 2);
        const uint8_t *buttons = mfb_get_mouse_button_buffer(window);
        ulang_bool buttonDown = UL_FALSE;
        for (int i = 0; i < 8; i++) {
            if (buttons[i] != 0) {
                buttonDown = UL_TRUE;
                break;
            }
        }
        ulang_vm_push_int(vm, buttonDown);
    }
    case 4: {

    }
    case 5: {
        uint64_t tickSince = stm_since(tickStart);
        float timeSince = (float)stm_sec(tickSince);
        ulang_vm_push_float(vm, timeSince);
    }
	}
	return UL_TRUE;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: ulang <file>");
		return -1;
	}

	ulang_error error = {0};
	ulang_program program = {0};
	if (!ulang_compile(argv[1], ulang_file_read, &program, &error)) {
		ulang_error_print(&error);
		ulang_error_free(&error);
		return -1;
	}

  	stm_setup();
  	tickStart = stm_now();

	window = mfb_open("ulang-vm", 640, 480);
	if (!window) {
		printf("Couldn't create window.");
		ulang_error_print(&error);
		ulang_error_free(&error);
		return -1;
	}

	ulang_vm vm = {0};
	ulang_vm_init(&vm, &program);
  	for (int i = 0; i <= 255; i++) vm.syscalls[i] = syscallHandler;
	while (ulang_vm_step(&vm));
	if (vm.error.is_set) ulang_error_print(&vm.error);
	ulang_vm_print(&vm);

	mfb_close(window);

	ulang_vm_free(&vm);
	ulang_program_free(&program);
	ulang_error_free(&error);
	return 0;
}
