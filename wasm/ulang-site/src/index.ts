import { createVirtualMachine, LogLevel, VirtualMachineState } from "@marioslab/ulang-vm"

(async function () {
	let vm = await createVirtualMachine(document.getElementById("output") as HTMLCanvasElement);
	vm.setStateChangeListener((vm, state) => {
		if (state == VirtualMachineState.Stopped) {
			console.log("Finished vm1");
		}
	});
	vm.setLogLevel(LogLevel.None);
	vm.run(`
		buffer: reserve int x 320 * 240
		mov buffer, r1
		mov 320 * 240, r2
		mov 0, r3
	loop:
		add r3, 1, r3
		and r3, 0xff, r3
		or r3, 0xff000000, r4
		sto r4, r1, 0
		add r1, 4, r1
		sub r2, 1, r2
		cmp r2, 0, r5		
		jne r5, loop
		
		push buffer
		syscall 1

		halt
	`);

	let vm2 = await createVirtualMachine(document.getElementById("output2") as HTMLCanvasElement);
	vm2.setStateChangeListener((vm, state) => {
		if (state == VirtualMachineState.Stopped) {
			console.log("Finished vm2");
		}
	});
	vm2.setLogLevel(LogLevel.None);
	vm2.run(`
		buffer: reserve int x 320 * 240
		mov buffer, r1
		mov 320 * 240, r2
		mov 0, r3
	loop:
		add r3, 1, r3
		and r3, 0xff, r3
		shl r3, 16, r4
		or r4, 0xff000000, r4
		sto r4, r1, 0
		add r1, 4, r1
		sub r2, 1, r2

		cmp r2, 0, r5
		jne r5, loop
		
		push buffer
		syscall 1

		halt
	`);
})();