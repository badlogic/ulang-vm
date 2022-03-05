import * as ulang from "./wrapper";

export { compile, printMemory } from "./wrapper";

export enum VirtualMachineState {
	Stopped, Running, Paused
}

export enum LogLevel {
	None,
	Info
}

export class VirtualMachine {
	private vm: ulang.UlangVm;
	private state = VirtualMachineState.Stopped;
	private compilerResult: ulang.UlangCompilationResult = null
	private vmStart = 0;
	private executedInstructions = 0;
	private vsyncHit = false;
	private breakpoints: number[] = [];
	private bpPtr = 0;
	private numBps = 0;
	private syscallHandlerPtr = 0;
	private lastStepHitBreakpoint = false;
	private stateChangeListener: (vm: VirtualMachine, state: VirtualMachineState) => void = null;
	private logLevel = LogLevel.Info;

	constructor (public canvas: HTMLCanvasElement) {
		let syscallHandler = (syscall, vmPtr) => {
			let vm = ulang.ptrToUlangVm(vmPtr);
			switch (syscall) {
				case 0:
					return -1;
				case 1:
					let buffer = vm.popUint();
					ulang.argbToRgba(vm.memoryPtr() + buffer, 320 * 240);
					let frame = new Uint8ClampedArray(ulang.HEAPU8().buffer, vm.memoryPtr() + buffer, 320 * 240 * 4);
					let imageData = new ImageData(frame, 320, 240);
					this.canvas.getContext("2d").putImageData(imageData, 0, 0);
					this.vsyncHit = true;
					return 0;
				case 2: {
					let numArgs = vm.popUint();
					let str = "";
					for (let i = 0; i < numArgs; i++) {
						let argType = vm.popUint();
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
								str += ulang.UTF8ArrayToString(ulang.HEAPU8(), vm.memoryPtr() + strAddr);
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
		this.syscallHandlerPtr = ulang.addFunction(syscallHandler, "iii");
	}

	setLogLevel (logLevel: LogLevel) {
		this.logLevel = logLevel;
	}

	setStateChangeListener (listener: (vm: VirtualMachine, state: VirtualMachineState) => void) {
		this.stateChangeListener = listener;
	}

	setBreakpoints (breakpoints: number[]) {
		this.breakpoints = breakpoints;
		if (this.bpPtr != 0) {
			ulang.free(this.bpPtr);
			this.bpPtr = 0;
			this.numBps = 0;
		}
		if (this.state != VirtualMachineState.Stopped) this.calculateBreakpoints();
	}

	private calculateBreakpoints () {
		if (this.bpPtr != 0) return this.bpPtr;
		// Needs to come before the next line, as WASM memory can grow and pointers may get relocated
		let addressToLine = this.vm.program().addressToLine();
		let p = this.bpPtr = ulang.alloc(4 * this.breakpoints.length);
		for (let i = 0; i < this.breakpoints.length; i++) {
			let bpLine = this.breakpoints[i];
			for (let j = 0; j < addressToLine.length; j++) {
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

		this.compilerResult = ulang.compile(source);
		if (this.compilerResult.error.isSet()) {
			alert("Can't run program with errors.");
			this.compilerResult.error.print();
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
		this.state = VirtualMachineState.Running;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		requestAnimationFrame(() => this.frame());
	}

	stop () {
		if (this.state != VirtualMachineState.Running && this.state != VirtualMachineState.Paused) return;
		this.state = VirtualMachineState.Stopped;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.printVmTime();
		this.printVmState();
	}

	pause () {
		if (this.state != VirtualMachineState.Running) return;
		this.state = VirtualMachineState.Paused;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.printVmState();
	}

	continue () {
		if (this.state != VirtualMachineState.Paused) return;
		this.state = VirtualMachineState.Running;
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.printVmState();
		requestAnimationFrame(() => this.frame());
	}

	step () {
		if (this.state != VirtualMachineState.Paused) return;
		if (!this.vm.step()) {
			if (this.vsyncHit) {
				this.vsyncHit = false;
			} else {
				this.state = VirtualMachineState.Stopped;
				if (this.stateChangeListener) this.stateChangeListener(this, this.state);
				return;
			}
		}
		if (this.stateChangeListener) this.stateChangeListener(this, this.state);
		this.printVmState();
	}

	getCurrentLine () {
		if (this.state != VirtualMachineState.Paused) return -1;
		let pc = this.vm.registers()[14].ui() >> 2;
		let addressToLine: number[] = this.vm.program().addressToLine();
		if (pc >= addressToLine.length) return -1;
		return addressToLine[pc];
	}

	printVmTime () {
		if (this.logLevel == LogLevel.None) return;
		let vmTime = (performance.now() - this.vmStart) / 1000;
		console.log("VM took " + vmTime + " secs");
		console.log("Executed " + this.executedInstructions + " instructions, " + ((this.executedInstructions / vmTime) | 0) + " ins/s");
	}

	printVmState () {
		if (this.logLevel == LogLevel.None) return;
		this.vm.print();
	}

	private frame () {
		if (this.state == VirtualMachineState.Running) {
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
					this.state = VirtualMachineState.Stopped;
					if (this.stateChangeListener) this.stateChangeListener(this, this.state);
					this.printVmTime();
					this.printVmState();
					return;
				}
			}

			while (true) {
				this.executedInstructions += instsPerStep;
				let result: boolean | number = 0;
				if (this.breakpoints.length > 0) this.calculateBreakpoints();
				if (this.numBps > 0) {
					result = this.vm.stepNBP(instsPerStep, this.bpPtr, this.numBps);
					// hit a breakpoint, pause execution
					if (result >= 1) {
						this.lastStepHitBreakpoint = true;
						this.state = VirtualMachineState.Paused;
						if (this.stateChangeListener) this.stateChangeListener(this, this.state);
						this.printVmState();
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
					this.state = VirtualMachineState.Stopped;
					if (this.stateChangeListener) this.stateChangeListener(this, this.state);
					this.printVmTime();
					this.printVmState();
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
}

let loaded = false;
export async function loadUlang () {
	if (!loaded) {
		loaded = true;
		await ulang.loadUlang();
	}
}