import * as monaco from "monaco-editor";
import { Editor } from "./editor"
import { VirtualMachine, VirtualMachineState } from "@marioslab/ulang-vm"
import { UlangLabelTarget } from "@marioslab/ulang-vm/src/wrapper";

export class Debugger {
	private run: HTMLElement;
	private cont: HTMLElement;
	private pause: HTMLElement;
	private step: HTMLElement;
	private stop: HTMLElement;

	constructor (private editor: Editor, private vm: VirtualMachine,
		runElement: HTMLElement | string,
		continueElement: HTMLElement | string,
		pauseElement: HTMLElement | string,
		stepElement: HTMLElement | string,
		stopElement: HTMLElement | string) {
		this.run = typeof (runElement) === "string" ? document.getElementById(runElement) : runElement;
		this.cont = typeof (continueElement) === "string" ? document.getElementById(continueElement) : continueElement;
		this.pause = typeof (pauseElement) === "string" ? document.getElementById(pauseElement) : pauseElement;
		this.step = typeof (stepElement) === "string" ? document.getElementById(stepElement) : stepElement;
		this.stop = typeof (stopElement) === "string" ? document.getElementById(stopElement) : stopElement;

		this.run.addEventListener("click", (e) => {
			e.stopPropagation();
			vm.run(editor.getContent());
		});
		this.cont.addEventListener("click", (e) => {
			e.stopPropagation();
			vm.continue();
		});
		this.pause.addEventListener("click", (e) => {
			e.stopPropagation();
			vm.pause();
		});
		this.stop.addEventListener("click", (e) => {
			e.stopPropagation();
			vm.stop();
		});
		this.step.addEventListener("click", (e) => {
			e.stopPropagation();
			vm.step();
		});

		vm.setStateChangeListener((vm, state) => {
			this.setButtonStates(state);
			switch (state) {
				case VirtualMachineState.Paused:
					let lineNum = vm.getCurrentLine();
					editor.setCurrLine(lineNum);
					this.updateMemoryView();
					break;
				default:
					editor.setCurrLine(-1);
					break;
			}
		});

		editor.setBreakpointListener((breakpoints) => {
			vm.setBreakpoints(breakpoints);
		});

		this.setupEditorTokenHoven();
		this.setButtonStates(VirtualMachineState.Stopped);
	}

	private updateMemoryView () {

	}

	private setButtonStates (state: VirtualMachineState) {
		this.run.style.display = "";
		this.cont.style.display = "none";
		this.cont.classList.add("toolbar-button-disabled");
		this.pause.classList.add("toolbar-button-disabled");
		this.step.classList.add("toolbar-button-disabled");
		this.stop.classList.add("toolbar-button-disabled");

		switch (state) {
			case VirtualMachineState.Running:
				this.run.style.display = "none";
				this.cont.style.display = "";
				this.cont.classList.add("toolbar-button-disabled");
				this.pause.classList.remove("toolbar-button-disabled");
				this.step.classList.add("toolbar-button-disabled");
				this.stop.classList.remove("toolbar-button-disabled");
				break;
			case VirtualMachineState.Paused:
				this.run.style.display = "none";
				this.cont.style.display = "";
				this.cont.classList.remove("toolbar-button-disabled");
				this.pause.classList.add("toolbar-button-disabled");
				this.step.classList.remove("toolbar-button-disabled");
				this.stop.classList.remove("toolbar-button-disabled");
				break;
		}
	}

	private setupEditorTokenHoven () {
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