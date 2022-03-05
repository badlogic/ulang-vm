import { createVm, Vm, VmState } from "@marioslab/ulang-vm"

(async function () {
	let vm = await createVm(document.getElementById("output") as HTMLCanvasElement);
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

	let vm2 = await createVm(document.getElementById("output2") as HTMLCanvasElement);
	vm2.setStateChangeListener((vm, state) => {
		if (state == VmState.Stopped) {
			console.log("Finished");
		}
	});
	vm2.run(`
		buffer: reserve int x 320 * 240
		mov (160 + 120 * 320) * 4, r1		
		add r1, buffer, r1
		mov 0xffffffff, r2
		sto r2, r1, 0
		push buffer
		syscall 1
		halt
	`);
})();