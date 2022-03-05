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
	buffer: reserve int x 320 * 240 * 4
	fire: reserve byte x 320 * 240
	palette: int 0xff070707, 0xff1F0707, 0xff2F0F07, 0xff470F07, 0xff571707, 0xff671F07, 0xff771F07, 0xff8F2707, 0xff9F2F07, 0xffAF3F07, 0xffBF4707, 0xffC74707, 0xffDF4F07, 0xffDF5707, 0xffDF5707, 0xffD75F07, 0xffD75F07, 0xffD7670F, 0xffCF6F0F, 0xffCF770F, 0xffCF7F0F, 0xffCF8717, 0xffC78717, 0xffC78F17, 0xffC7971F, 0xffBF9F1F,0xffBF9F1F, 0xffBFA727, 0xffBFA727, 0xffBFAF2F, 0xffB7AF2F, 0xffB7B72F, 0xffB7B737, 0xffCFCF6F, 0xffDFDF9F, 0xffEFEFC7, 0xffFFFFFF
	
	   # clear buffer
	   mov buffer, r1
	   mov 0, r2
	   mov 0xff000000, r3
	clear_buffer_loop:
	   sto r3, r1, r2
	   add r2, 4, r2
	   cmp r2, 320 * 240 * 4, r4
	   jl r4, clear_buffer_loop
	
	   # clear fire
	   mov fire, r1
	   mov 0, r2
	   mov 0, r3
	clear_fire_loop:
	   sto r3, r1, r2
	   add r2, 1, r2
	   cmp r2, 320 * 240, r4
	   jl r4, clear_fire_loop
	
	   # set bottom fire row
	   mov 320 * 239, r2
	   mov 36, r3
	set_bottom_fire_loop:
	   sto r3, r1, r2
	   add r2, 1, r2
	   cmp r2, 320 * 240, r4
	   jl r4, set_bottom_fire_loop
	
	main_loop:
	
	   # update fire
	   mov fire, r1
	   add r1, 320 * 240, r2
	   add r1, 320, r1
	
	   update_fire_loop:
		  rand r6
		  mulf r6, 3, r6
		  f2i r6, r6
		  mov r6, r7
		  and r6, 1, r6
	
		  ldb r1, 0, r4
		  sub r4, r6, r4
		  cmp r4, 0, r5
		  jge r5, set_fire
		  mov 0, r4
	   set_fire:
		  mov r1, r3
		  sub r3, 320, r3 # to
		  sub r3, r7, r3
		  add r3, 1, r3
		  stob r4, r3, 0
	
		  add r1, 1, r1
		  cmp r1, r2, r5
		  jl r5, update_fire_loop
	
	   # draw fire
	   mov fire, r1
	   add r1, 320 * 240, r2
	   mov buffer, r3
	   mov palette, r4
	
	   draw_fire_loop:
		  # load fire color
		  ldb r1, 0, r5
		  mul r5, 4, r5
		  add r5, palette, r5
		  ld r5, 0, r5
	
		  # store in buffer
		  sto r5, r3, 0
	
		  # advance to next pixel
		  add r1, 1, r1
		  add r3, 4, r3
		  cmp r1, r2, r5
		  jl r5, draw_fire_loop
	
	
	   # update buffer
	   push buffer
	   syscall 0x1
	
	   push 1
	   jmp main_loop
	
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