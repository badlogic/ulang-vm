import * as ulang from "./wrapper";

export { compile, printMemory } from "./wrapper";

export enum VirtualMachineState {
	Stopped, Running, Paused
}

export interface Breakpoint {
	filename: string,
	lineNumber: number
}

export enum LogLevel {
	None,
	Info
}

interface EventListener {
	type: keyof HTMLElementEventMap,
	listener: (this: HTMLCanvasElement, ev: HTMLElementEventMap[keyof HTMLElementEventMap]) => any;
}

export class VirtualMachine {
	private canvas: HTMLCanvasElement;
	private vm: ulang.UlangVm;
	private state = VirtualMachineState.Stopped;
	private compilerResult: ulang.UlangCompilationResult = null
	private vmStart = 0;
	private executedInstructions = 0;
	private vsyncHit = false;
	private debugSyscallHit = false;
	private breakpoints: Breakpoint[] = [];
	private bpPtr = 0;
	private numBps = 0;
	private syscallHandlerPtr = 0;
	private lastStepHitBreakpoint = false;
	private stateChangeListener: (vm: VirtualMachine, state: VirtualMachineState) => void = null;
	private logLevel = LogLevel.Info;
	private mouseX = 0;
	private mouseY = 0;
	private mouseButtonDown = false;
	private keys: Map<String, number> = new Map();
	private listeners: EventListener[] = [];
	private rgbaFramePtr: number;

	constructor (canvasElement: HTMLCanvasElement | string) {
		if (typeof (canvasElement) === "string") this.canvas = document.getElementById(canvasElement) as HTMLCanvasElement;
		else this.canvas = canvasElement;

		this.rgbaFramePtr = ulang.alloc(320 * 240 * 4);

		this.addEventListener("mousemove", (e) => {
			var rect = this.canvas.getBoundingClientRect();
			this.mouseX = ((e.clientX - rect.left) / this.canvas.clientWidth * 320) | 0;
			this.mouseY = ((e.clientY - rect.top) / this.canvas.clientHeight * 240) | 0;
		});
		this.addEventListener("mousedown", (e) => {
			var rect = this.canvas.getBoundingClientRect();
			this.mouseX = ((e.clientX - rect.left) / this.canvas.clientWidth * 320) | 0;
			this.mouseY = ((e.clientY - rect.top) / this.canvas.clientHeight * 240) | 0;
			this.mouseButtonDown = true;
		});
		this.addEventListener("mouseup", (e) => {
			var rect = this.canvas.getBoundingClientRect();
			this.mouseX = ((e.clientX - rect.left) / this.canvas.clientWidth * 320) | 0;
			this.mouseY = ((e.clientY - rect.top) / this.canvas.clientHeight * 240) | 0;
			this.mouseButtonDown = false;
		});
		this.addEventListener("mouseleave", (e) => {
			var rect = this.canvas.getBoundingClientRect();
			this.mouseX = ((e.clientX - rect.left) / this.canvas.clientWidth * 320) | 0;
			this.mouseY = ((e.clientY - rect.top) / this.canvas.clientHeight * 240) | 0;
			this.mouseButtonDown = false;
		})

		let syscallHandler = (syscall, vmPtr) => {
			let vm = ulang.ptrToUlangVm(vmPtr);
			switch (syscall) {
				case 0:
					this.pause();
					this.debugSyscallHit = true;
					return 0;
				case 1:
					let buffer = vm.popUint();
					ulang.argbToRgba(vm.memoryPtr() + buffer, this.rgbaFramePtr, 320 * 240);
					let frame = new Uint8ClampedArray(ulang.HEAPU8().buffer, this.rgbaFramePtr, 320 * 240 * 4);
					let imageData = new ImageData(frame, 320, 240);
					this.canvas.getContext("2d").putImageData(imageData, 0, 0);
					this.vsyncHit = true;
					return 0;
				case 2: {
					let str = "";
					while (true) {
						let argType = vm.popUint();
						if (argType == 6) break;
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
							case 4:
								str += " ";
								break;
							case 5:
								str += "\n";
								break;
							default:
								break;
						}
					}
					console.log(str);
					return -1;
				}
				case 3: {
					vm.pushInt(this.mouseX);
					vm.pushInt(this.mouseY);
					vm.pushInt(this.mouseButtonDown ? -1 : 0);
					return -1;
				}
				case 4: {

				}
				case 5: {
					vm.pushFloat(performance.now() / 1000);
					return -1;
				}
			}
		}
		this.syscallHandlerPtr = ulang.addFunction(syscallHandler, "iii");
	}

	private addEventListener<K extends keyof HTMLElementEventMap> (type: K, listener: (this: HTMLCanvasElement, ev: HTMLElementEventMap[K]) => any) {
		this.canvas.addEventListener(type, listener);
		this.listeners.push({ type: type, listener: listener });
	}

	dipose () {
		this.stop();
		for (let i = 0; i < this.listeners.length; i++) {
			let listener = this.listeners[i];
			this.canvas.removeEventListener(listener.type, listener.listener);
		}
		ulang.free(this.rgbaFramePtr);
	}

	setLogLevel (logLevel: LogLevel) {
		this.logLevel = logLevel;
	}

	setStateChangeListener (listener: (vm: VirtualMachine, state: VirtualMachineState) => void) {
		this.stateChangeListener = listener;
	}

	setBreakpoints (breakpoints: Breakpoint[]) {
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
		let addressToFile = this.vm.program().addressToFile();
		let p = this.bpPtr = ulang.alloc(4 * this.breakpoints.length);
		for (let i = 0; i < this.breakpoints.length; i++) {
			let bp = this.breakpoints[i];
			for (let j = 0; j < addressToLine.length; j++) {
				if (addressToLine[j] == bp.lineNumber && addressToFile[j].fileName().toString() == bp.filename) {
					ulang.setUint32(p, j * 4);
					p += 4;
					this.numBps++;
					break;
				}
			}
		}
		return this.bpPtr;
	}

	run (filename: string, fileReader: (filename: string) => string) {
		if (this.compilerResult) {
			this.compilerResult.free();
			this.compilerResult = null;
		}
		if (this.vm) {
			this.vm.free();
			this.vm = null;
		}

		this.compilerResult = ulang.compile(filename, fileReader);
		if (this.compilerResult.error.isSet()) {
			alert("Can't run program with errors.");
			this.compilerResult.error.print();
			this.compilerResult.free();
			if (this.stateChangeListener) this.stateChangeListener(this, this.state);
			return;
		}
		this.vm = ulang.newVm(this.compilerResult.program);
		for (let i = 0; i <= 255; i++) {
			this.vm.setSyscall(i, this.syscallHandlerPtr);
		}
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
			if (this.vsyncHit || this.debugSyscallHit) {
				this.vsyncHit = false;
				this.debugSyscallHit = false;
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
		let pc = this.vm.registers()[15].ui() >> 2;
		let addressToLine: number[] = this.vm.program().addressToLine();
		if (pc >= addressToLine.length) return -1;
		return addressToLine[pc];
	}

	getCurrentFile () {
		if (this.state != VirtualMachineState.Paused) return null;
		let pc = this.vm.registers()[15].ui() >> 2;
		let addressToFile = this.vm.program().addressToFile();
		if (pc >= addressToFile.length) return null;
		return addressToFile[pc];
	}

	getState () {
		return this.state;
	}

	getRegisters () {
		return this.vm.registers();
	}

	getProgram () {
		return this.vm.program();
	}

	getInt (addr: any) {
		return ulang.getInt32(this.vm.memoryPtr() + addr);
	}

	getFloat (addr: any) {
		return ulang.getFloat32(this.vm.memoryPtr() + addr);
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

	getCanvas () {
		return this.canvas;
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
					if (this.vsyncHit || this.debugSyscallHit) {
						this.vsyncHit = false;
						this.debugSyscallHit = false;
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
					if (this.vsyncHit || this.debugSyscallHit) {
						this.vsyncHit = false;
						this.debugSyscallHit = false;
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

	takeScreenshot () {
		return this.canvas.toDataURL("image/png");
	}
}

let loaded = false;
export async function loadUlang () {
	if (!loaded) {
		loaded = true;
		await ulang.loadUlang();
	}
}

export async function createPlayerFromGist (canvas: HTMLCanvasElement, gistId: string) {
	await loadUlang();
	let response = await fetch(`https://api.github.com/gists/${gistId}`);
	if (response.status >= 400) throw new Error(`Couldn't fetch gist ${gistId}`);
	let gist = await response.json();
	if (!gist.files || !gist.files["program.ul"] || !gist.files["program.ul"].content) {
		throw new Error("Gist ${gistId} is not a ulang program.");
	}
	return new UlangPlayer(canvas, gist.files);
}

export type ProgramFiles = {
	[filename: string]: {
		content: string
	}
}

export class UlangPlayer {
	vm: VirtualMachine;
	files: ProgramFiles

	constructor (canvas: HTMLCanvasElement, files: ProgramFiles) {
		this.vm = new VirtualMachine(canvas);
		this.files = files;
	}

	play () {
		this.vm.run("program.ul", (filename: string) => { return this.files[filename].content; });
	}

	getVirtualMachine () {
		return this.vm;
	}

	getProgramFiles () {
		return this.files;
	}

	dispose () {
		if (this.vm) this.vm.dipose();
	}
}