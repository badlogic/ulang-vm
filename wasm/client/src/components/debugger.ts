import * as monaco from "monaco-editor";
import { Editor } from "./editor"
import { VirtualMachine, VirtualMachineState } from "@marioslab/ulang-vm"
import { UlangLabelTarget, UlangValue, UL_VM_MEMORY_SIZE } from "@marioslab/ulang-vm/src/wrapper";
import { project } from "src/project";

export class Debugger {
	private registerView: HTMLElement;
	private registerViewConfig: HTMLElement;
	private stackView: HTMLElement;
	private stackViewConfig: HTMLElement;
	private maxStackEntries: HTMLInputElement;
	private memoryView: HTMLElement;
	private memoryViewConfig: HTMLElement;
	private memoryAddress: HTMLInputElement;
	private memoryType: HTMLSelectElement;
	private numMemoryEntries: HTMLInputElement;
	private lastValues: { i: number, f: number }[] = [];

	constructor (private editor: Editor, private vm: VirtualMachine, private toolbar: Toolbar, debugViewContainer: HTMLElement) {
		toolbar.getRunButton().addEventListener("click", (e) => {
			e.stopPropagation();
			vm.run("program.ul", (filename: string) => {
				return project.getFileContent(filename);
			});
		});
		toolbar.getContinueButton().addEventListener("click", (e) => {
			e.stopPropagation();
			vm.continue();
		});
		toolbar.getPauseButton().addEventListener("click", (e) => {
			e.stopPropagation();
			vm.pause();
		});
		toolbar.getStopButton().addEventListener("click", (e) => {
			e.stopPropagation();
			vm.stop();
		});
		toolbar.getStepButton().addEventListener("click", (e) => {
			e.stopPropagation();
			vm.step();
		});

		debugViewContainer.innerHTML = debuggerViews;

		this.registerView = debugViewContainer.querySelector(".debug-view-registers");
		this.registerViewConfig = this.registerView.querySelector(".debug-view-registers-config");

		this.stackView = debugViewContainer.querySelector(".debug-view-stack");
		this.stackViewConfig = this.stackView.querySelector(".debug-view-stack-config");
		this.maxStackEntries = this.stackViewConfig.querySelector(".debug-max-stack-entries") as HTMLInputElement;
		this.maxStackEntries.addEventListener("change", () => this.updateViews());

		this.memoryView = debugViewContainer.querySelector(".debug-view-memory");
		this.memoryViewConfig = this.memoryView.querySelector(".debug-view-memory-config");
		this.memoryAddress = this.memoryViewConfig.querySelector(".debug-memory-address") as HTMLInputElement;
		this.memoryAddress.addEventListener("change", () => this.updateViews());
		this.memoryType = this.memoryViewConfig.querySelector(".debug-memory-type") as HTMLSelectElement;
		this.memoryType.addEventListener("change", () => this.updateViews());
		this.numMemoryEntries = this.memoryViewConfig.querySelector(".debug-num-memory-entries") as HTMLInputElement;
		this.numMemoryEntries.addEventListener("change", () => this.updateViews());

		vm.setStateChangeListener((vm, state) => {
			toolbar.setButtonStates(state);
			switch (state) {
				case VirtualMachineState.Paused:
					let lineNum = vm.getCurrentLine();
					editor.setCurrLine(lineNum);
					this.updateViews();
					break;
				default:
					editor.setCurrLine(-1);
					this.lastValues = [];
					this.updateViews();
					break;
			}
		});

		editor.setBreakpointListener((breakpoints) => {
			vm.setBreakpoints(breakpoints);
		});

		this.setupEditorTokenHover();
		toolbar.setButtonStates(VirtualMachineState.Stopped);
	}

	private updateViews () {
		let vm = this.vm;
		this.registerView.innerHTML = "";
		this.registerView.appendChild(this.registerViewConfig);
		this.stackView.innerHTML = "";
		this.stackView.appendChild(this.stackViewConfig);
		this.memoryView.innerHTML = "";
		this.memoryView.appendChild(this.memoryViewConfig);

		if (vm.getState() != VirtualMachineState.Paused) return;

		// Registers		
		let registers = vm.getRegisters();
		for (let i = 1; i < 15; i++) {
			let register = registers[i - 1];
			this.registerView.appendChild(this.createItem("r" + i, register.i(), register.f(), this.checkValueChanged("reg" + (i - 1), register)));
		}
		this.registerView.appendChild(this.createItem("pc", registers[14].i(), registers[14].f(), this.checkValueChanged("reg" + 14, registers[14])));
		this.registerView.appendChild(this.createItem("sp", registers[15].i(), registers[15].f(), this.checkValueChanged("reg" + 15, registers[15])));

		// Stack		
		let maxStackEntries = Number.parseInt(this.maxStackEntries.value);
		let stackAddress = registers[15].ui();
		let entries = 0;
		for (let i = 0; i < maxStackEntries; i++) {
			let addr = stackAddress + i * 4;
			if (addr > UL_VM_MEMORY_SIZE - 4) break;
			let intVal = vm.getInt(addr);
			let floatVal = vm.getFloat(addr);
			this.stackView.appendChild(this.createItem("sp - " + i, intVal, floatVal, this.checkValueChanged("sp" + i, { intVal: intVal, floatVal: floatVal })))
			entries++;
		}
		if (entries == 0) {
			for (let key in this.lastValues) {
				if (key.startsWith("sp")) delete this.lastValues[key];
			}
		}

		// Memory
		let address = Number.parseInt(this.memoryAddress.value);

		// Check if its a register
		if (Number.isNaN(address)) {
			let value = this.memoryAddress.value.trim();
			let registers = this.vm.getRegisters();
			for (let i = 1; i < 15; i++) {
				if (value == "r" + i) {
					address = registers[i - 1].ui();
					break;
				}
			}
			if (value == "pc") address = registers[14].ui();
			if (value == "sp") address = registers[15].ui();
		}

		// Check if its a label
		if (Number.isNaN(address)) {
			let value = this.memoryAddress.value.trim();
			let labels = this.vm.getProgram().labels();
			for (let i = 0; i < labels.length; i++) {
				let label = labels[i];
				if (value == label.label().data().toString()) {
					address = label.address();
					switch (label.target()) {
						case UlangLabelTarget.UL_LT_CODE:
							break;
						case UlangLabelTarget.UL_LT_DATA:
							address += this.vm.getProgram().code().byteLength;
							break;
						case UlangLabelTarget.UL_LT_RESERVED_DATA:
							address += this.vm.getProgram().code().byteLength + this.vm.getProgram().data().byteLength;
					}
					break;
				}
			}
		}

		// Else it's invalid
		if (Number.isNaN(address)) {
			let msg = document.createElement("div");
			msg.innerHTML = "Invalid address " + this.memoryAddress.value;
			this.memoryView.appendChild(msg);
			return;
		}

		let numEntries = Number.parseInt(this.numMemoryEntries.value);
		let type = this.memoryType.value;
		for (let i = 0; i < numEntries; i++) {
			if (address >= UL_VM_MEMORY_SIZE) break;
			let intVal = vm.getInt(address);
			let floatVal = vm.getFloat(address);
			this.memoryView.appendChild(this.createMemoryItem("0x" + address.toString(16), intVal, floatVal, type as "Byte" | "Int" | "Float", this.checkValueChanged("a" + address + type, { intVal: intVal, floatVal: floatVal })));
			if (type.indexOf("Byte") == -1) address += 4;
			else address++;
		}
		console.log(address);
	}

	private checkValueChanged (key: string, value: UlangValue | { intVal: number, floatVal: number }) {
		let changed = false;
		let intVal = 0, floatVal = 0;
		if ("intVal" in value) {
			intVal = value.intVal;
			floatVal = value.floatVal;
		} else {
			intVal = value.i();
			floatVal = value.f();
		}
		if (this.lastValues[key]) {
			let lastValue = this.lastValues[key];
			changed = lastValue.i != intVal || lastValue.f != floatVal;
		} else {
			changed = true;
		}
		this.lastValues[key] = { i: intVal, f: floatVal };
		return changed;
	}

	private createItem (name: string, intVal: number, floatVal: number, changed: boolean) {
		let changedClass = changed ? "debug-view-item-changed" : "";
		let dom = document.createElement("div");
		dom.innerHTML = `
			<span class="debug-view-item-name">${name}</span>
			<span class="${changedClass}">${intVal}</span>
			<span class="${changedClass}">0x${(intVal & 0xffffffff).toString(16)}</span>
			<span class="${changedClass}">${floatVal.toFixed(8)}</span>
		`;
		return dom;
	}

	private createMemoryItem (address: string, intVal: number, floatVal: number, type: "Byte" | "Int" | "Float", changed: boolean) {
		let changedClass = changed ? "debug-view-item-changed" : "";
		let dom = document.createElement("div");
		dom.classList.add("debug-view-item");

		switch (type) {
			case "Byte":
				dom.innerHTML = `
					<span class="debug-view-item-name">${address}</span>
					<span class="${changedClass}">${intVal & 0xff}</span>
					<span class="${changedClass}">0x${(intVal & 0xff).toString(16)}</span>
				`;
				break;
			case "Int":
				dom.innerHTML = `
					<span class="debug-view-item-name">${address}</span>
					<span class="${changedClass}">${intVal}</span>
					<span class="${changedClass}">0x${intVal.toString(16)}</span>
				`;
				break;
			case "Float":
				dom.innerHTML = `
					<span class="debug-view-item-name">${address}</span>
					<span class="${changedClass}">${floatVal.toFixed(8)} </span>
				`;
				break;
		}
		return dom;
	}

	private setupEditorTokenHover () {
		monaco.languages.registerHoverProvider('ulang', {
			provideHover: (model, position, token) => {
				if (this.vm.getState() == VirtualMachineState.Paused) {
					let lineNum = position.lineNumber;
					let startCol = position.column - 1;
					let endCol = position.column - 1;

					let line = this.editor.getContent().split("\n")[lineNum - 1];
					for (let i = startCol; i >= 0; i--) {
						let c = line.charAt(i);
						if (c == ' ' || c == '\n' || c == '\t' || c == "," || c == ":") break;
						startCol--;
					}
					startCol++;
					for (let i = endCol; i < line.length; i++) {
						let c = line.charAt(i);
						if (c == ' ' || c == '\n' || c == '\t' || c == "," || c == ":") break;
						endCol++;
					}
					if (startCol > endCol) return null;

					let token = line.substring(startCol, endCol);
					let regs = ["r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "pc", "sp"];
					let regIdx = -1;
					for (let i = 0; i < regs.length; i++) {
						if (token == regs[i]) {
							regIdx = i;
							break;
						}
					}

					let result = {};
					if (regIdx != -1) {
						let i = this.vm.getRegisters()[regIdx].i();
						let u = this.vm.getRegisters()[regIdx].ui();
						let h = "0x" + u.toString(16);
						let f = this.vm.getRegisters()[regIdx].f();
						result = {
							range: new monaco.Range(lineNum, startCol, lineNum, endCol),
							contents: [
								{ value: "**" + token + "**" },
								{ value: "```html\nint: " + i + "\nuint: " + u + "\nhex: " + h + "\nfloat: " + f + "\n" }
							]
						};
					}

					let labels = this.vm.getProgram().labels();
					for (let i = 0; i < labels.length; i++) {
						let label = labels[i];
						if (label.label().data().toString() == token) {
							let labelAddress = label.address();
							if (label.target() == UlangLabelTarget.UL_LT_DATA) labelAddress += this.vm.getProgram().code().byteLength;
							if (label.target() == UlangLabelTarget.UL_LT_RESERVED_DATA) labelAddress += this.vm.getProgram().code().byteLength + this.vm.getProgram().data().byteLength;

							result = {
								range: new monaco.Range(lineNum, startCol, lineNum, endCol),
								contents: [
									{ value: "**" + token + "**" },
									{ value: "```html\naddress: 0x" + labelAddress.toString(16) + ", " + labelAddress.toString() + "\n" }
								]
							};
							break;
						}
					}
					return result as monaco.languages.ProviderResult<monaco.languages.Hover>;
				} else {
					return null;
				}
			}
		});
	}
}

import "./debugger.css";
import debuggerViews from "./debugger.html";
import { Toolbar } from "./toolbar";