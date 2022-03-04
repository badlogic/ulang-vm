import { ulang as createUlang } from "./ulang";

let ulang = null;

function compile (source) {
	let result = {
		error: ulang.newError(),
		file: ulang.newFile("source", source),
		program: ulang.newProgram(),
		free: function () {
			this.program.free();
			this.file.free();
			this.error.free();
		}
	}
	ulang.compile(result.file, result.program, result.error);
	return result;
};

export enum VmState {
	Stopped, Running, Paused
}

export class Vm {
	private vm: any;
	private state = VmState.Stopped;
	private compilerResult = null
	private vmStart = 0;
	private executedInstructions = 0;
	private vsyncHit = false;
	private breakpoints: string[] = [];
	private bpPtr = 0;
	private numBps = 0;
	private syscallHandlerPtr = 0;
	private lastStepHitBreakpoint = false;

	constructor (public canvas: HTMLCanvasElement) {
		this.syscallHandlerPtr = ulang.addFunction(this.syscallHandler, "iii");
	}

	private syscallHandler = (syscall, vmPtr) => {
		let vm = ulang.nativeToJsVm(vmPtr);
		switch (syscall) {
			case 0:
				return -1;
			case 1:
				let buffer = vm.popUint();
				ulang.argbToRgba(vm.memoryPtr() + buffer, 320 * 240);
				let frame = new Uint8ClampedArray(ulang.HEAPU8.buffer, vm.memoryPtr() + buffer, 320 * 240 * 4);
				let imageData = new ImageData(frame, 320, 240);
				this.canvas.getContext("2d").putImageData(imageData, 0, 0);
				this.vsyncHit = true;
				return 0;
			case 2: {
				let numArgs = vm.popUint();
				let str = "";
				for (let i = 0; i < numArgs; i++) {
					let argType = vm.popUint(vm);
					switch (argType) {
						case 0:
							str += vm.popInt();
							break;
						case 1:
							str += "0x" + vm.popInt().toString(16);
							break;
						case 2:
							str += vm.popFloat();
							break;
						case 3:
							let strAddr = vm.popUint();
							str += ulang.UTF8ArrayToString(ulang.HEAPU8, vm.memoryPtr() + strAddr);
							break;
						default:
							break;
					}
				}
				console.log(str);
				return -1;
			}
		}
	}

	setBreakpoints = (breakpoints: string[]) => {
		this.breakpoints = breakpoints;
		if (this.bpPtr != 0) {
			ulang.free(this.bpPtr);
			this.bpPtr = 0;
			this.numBps = 0;
		}
		if (this.state != VmState.Stopped) this.calculateBreakpoints();
	}

	private calculateBreakpoints () {
		if (this.bpPtr != 0) return this.bpPtr;
		// Needs to come before the next line, as WASM memory can grow and pointers may get relocated
		let addressToLine = this.vm.program().addressToLine();
		let p = this.bpPtr = ulang.alloc(4 * this.breakpoints.length);
		for (var i = 0; i < this.breakpoints.length; i++) {
			let bpLine = this.breakpoints[i];
			for (var j = 0; j < addressToLine.length; j++) {
				if (addressToLine[j] == bpLine) {
					ulang.setUint32(p, j * 4);
					p += 4;
					this.numBps++;
					break;
				}
			}
		}
		return this.bpPtr;
	}

	run (source) {
		if (this.compilerResult) {
			this.compilerResult.free();
			this.compilerResult = null;
		}
		if (this.vm) {
			this.vm.free();
			this.vm = null;
		}

		this.compilerResult = compile(source);
		if (this.compilerResult.error.isSet()) {
			alert("Can't run program with errors.");
			this.compilerResult.free();
			if (this.stateChangeListener) this.stateChangeListener(this, this.state);
			return;
		}
		this.vm = ulang.newVm(this.compilerResult.program);
		this.vm.setSyscall(0, this.syscallHandlerPtr);
		this.vm.setSyscall(1, this.syscallHandlerPtr);
		this.vm.setSyscall(2, this.syscallHandlerPtr);
		this.vmStart = performance.now();
		this.executedInstructions = 0;
		this.lastStepHitBreakpoint = false;
		this.state = VmState.Running;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		console.log("Starting vm.");
		requestAnimationFrame(() => this.frame());
	}

	stop () {
		if (this.state != VmState.Running && this.state != VmState.Paused) return;
		this.state = VmState.Stopped;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.printVmTime();
		this.vm.print();
	}

	pause () {
		if (this.state != VmState.Running) return;
		this.state = VmState.Paused;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.vm.print();
	}

	continue () {
		if (this.state != VmState.Paused) return;
		this.state = VmState.Running;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.vm.print();
		requestAnimationFrame(() => this.frame());
	}

	step () {
		if (this.state != VmState.Paused) return;
		if (!this.vm.step()) {
			if (this.vsyncHit) {
				this.vsyncHit = false;
			} else {
				this.state = VmState.Stopped;
				if (this.stateChangeListener) this.stateChangeListener(this, this.state);
				return;
			}
		}
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.vm.print();
	}

	getCurrLine () {
		if (this.state != VmState.Paused) return -1;
		let pc = this.vm.registers()[14].ui() >> 2;
		let addressToLine: number[] = this.vm.program().addressToLine();
		if (pc >= addressToLine.length) return -1;
		return addressToLine[pc];
	}

	printVmTime () {
		let vmTime = (performance.now() - this.vmStart) / 1000;
		console.log("VM took " + vmTime + " secs");
		console.log("Executed " + this.executedInstructions + " instructions, " + ((this.executedInstructions / vmTime) | 0) + " ins/s");
	}


	private frame () {
		if (this.state == VmState.Running) {
			let frameStart = performance.now();
			const instsPerStep = 20000;

			// if we had a breakpoint in the last frame, we need
			// to step over the current instruction, so we don't
			// get stuck there.
			if (this.lastStepHitBreakpoint) {
				this.lastStepHitBreakpoint = false;
				if (!this.vm.step()) {
					if (this.vsyncHit) {
						this.vsyncHit = false;
						requestAnimationFrame(() => this.frame());
						return;
					}
					this.state = VmState.Stopped;
					if (this.stateChangeListener) this.stateChangeListener(this, this.state);
					this.printVmTime();
					this.vm.print();
					return;
				}
			}

			while (true) {
				this.executedInstructions += instsPerStep;
				var result = 0;
				if (this.breakpoints.length > 0) this.calculateBreakpoints();
				if (this.numBps > 0) {
					result = this.vm.stepNBP(instsPerStep, this.bpPtr, this.numBps);
					// hit a breakpoint, pause execution
					if (result >= 1) {
						this.lastStepHitBreakpoint = true;
						this.state = VmState.Paused;
						if (this.stateChangeListener) this.stateChangeListener(this, this.state);
						this.vm.print();
						return;
					}
				} else {
					result = this.vm.stepN(instsPerStep);
				}

				if (!result) {
					if (this.vsyncHit) {
						this.vsyncHit = false;
						requestAnimationFrame(() => this.frame());
						return;
					}
					this.state = VmState.Stopped;
					if (this.stateChangeListener) this.stateChangeListener(this, this.state);
					this.printVmTime();
					this.vm.print();
					return;
				}
				let frameTime = performance.now() - frameStart;
				if (frameTime > 16) {
					requestAnimationFrame(() => this.frame());
					return;
				}
			}
		}
	}

	private stateChangeListener: (vm: Vm, state: VmState) => void = null;
	setStateChangeListener = (listener: (vm: Vm, state: VmState) => void) => {
		this.stateChangeListener = listener;
	}
}

export async function createVm (canvas: HTMLCanvasElement) {
	ulang = await createUlang();
	return new Vm(canvas);
}