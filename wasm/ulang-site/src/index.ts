import { createVm, Vm, VmState } from "@marioslab/ulang-vm"


createVm(document.getElementById("output") as HTMLCanvasElement).then((vm) => {
	vm.setStateChangeListener((vm, state) => {
		if (state == VmState.Stopped) {
			console.log("Finished");
		}
	});
	vm.run(`
		buffer: reserve int x 320 * 240
		mov (160 + 120 * 320) * 4, r1		
		add r1, buffer, r1
		mov 0xffff00ff, r2
		sto r2, r1, 0
		push buffer
		syscall 1
		halt
	`);
})
