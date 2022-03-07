import "monaco-editor/min/vs/editor/editor.main.css"
import "./editor.css"
import * as monaco from "monaco-editor";
import * as ulang from "@marioslab/ulang-vm"

(globalThis as any).self.MonacoEnvironment = {
	getWorkerUrl: function (moduleId, label) {
		return "./bundle/editor.worker.js";
	},
};

export class Editor {
	private editor: monaco.editor.IStandaloneCodeEditor;
	private decorations: string[] = [];
	private breakpoints: monaco.editor.IModelDeltaDecoration[] = null;
	private currLine: monaco.editor.IModelDeltaDecoration = null;
	private breakpointListener: (bps: number[]) => void = null;

	constructor (containerElement: HTMLElement | string) {
		let container;
		if (typeof (containerElement) === "string") container = document.getElementById(containerElement);
		else container = containerElement;
		defineUlangLanguage();
		this.editor = monaco.editor.create(container, {
			automaticLayout: true,
			theme: "vs-dark",
			language: "ulang",
			glyphMargin: true,
			tabSize: 3
		});

		this.editor.onDidChangeModelContent(() => this.onDidChangeModelContent());
		this.editor.onDidChangeModelDecorations((e) => this.onDidChangeModelDecorations());

		this.editor.onMouseDown((e) => {
			if (e.target.type !== 2) return;
			this.toggleBreakpoint(e.target.range.startLineNumber);
		});
	}

	private onDidChangeModelContent () {
		let result = ulang.compile(this.editor.getValue());
		if (result.error.isSet()) {
			result.error.print();
			let base = result.file.data().data();
			let startLineNumber = result.error.span().startLine();
			let startLine = result.file.lines()[startLineNumber].data().data() - base;
			let endLineNumber = result.error.span().endLine();
			let endLine = result.file.lines()[endLineNumber].data().data() - base;

			let spanStart = result.error.span().data().data() - base;
			let spanEnd = spanStart + result.error.span().data().length();

			let startCol = spanStart - startLine + 1;
			let endCol = spanEnd - endLine + 1;

			let markers = [
				{
					severity: monaco.MarkerSeverity.Error,
					startLineNumber: startLineNumber,
					startColumn: startCol,
					endLineNumber: endLineNumber,
					endColumn: endCol,
					message: result.error.message().toString()
				}
			];
			monaco.editor.setModelMarkers(this.editor.getModel(), "ulang", markers);
		} else {
			monaco.editor.setModelMarkers(this.editor.getModel(), "ulang", []);
		}
		result.free();
		ulang.printMemory();
	}

	private onDidChangeModelDecorations () {
		this.breakpoints = this.getBreakpointDecorations();
		let breakpointLinenumbers = this.breakpoints.map(bp => bp.range.startLineNumber);
		if (this.breakpointListener) this.breakpointListener(breakpointLinenumbers)
	}

	private updateDecorations () {
		let newDecorations: monaco.editor.IModelDeltaDecoration[] = [];
		for (let i = 0; i < this.breakpoints.length; i++) newDecorations.push(this.breakpoints[i]);
		if (this.currLine) newDecorations.push(this.currLine);
		this.decorations = this.editor.deltaDecorations(this.decorations, newDecorations);
	}

	private getBreakpointDecorations () {
		let breakpoints: monaco.editor.IModelDeltaDecoration[] = [];
		let decorations = this.editor.getModel().getAllDecorations();
		for (let i = 0; i < decorations.length; i++) {
			let decoration = decorations[i];
			if (decoration.options.glyphMarginClassName == 'ulang-debug-breakpoint') {
				breakpoints.push({
					range: decoration.range,
					options: decoration.options
				})
			}
		}
		return breakpoints;
	}

	private toggleBreakpoint (lineNumber: number) {
		this.breakpoints = this.getBreakpointDecorations();
		let foundBreakpoint = false;
		this.breakpoints = this.breakpoints.filter(e => {
			if (e.range.startLineNumber == lineNumber) {
				foundBreakpoint = true;
				return false;
			} else {
				return true;
			}
		})
		if (!foundBreakpoint) {
			this.breakpoints.push({
				range: new monaco.Range(lineNumber, 1, lineNumber, 1),
				options: {
					isWholeLine: true,
					glyphMarginClassName: 'ulang-debug-breakpoint'
				}
			});
		}
		this.updateDecorations();
	}

	setCurrLine = (lineNum: number) => {
		if (lineNum == -1) {
			this.currLine = null;
			this.updateDecorations();
			return;
		}
		this.currLine = {
			range: new monaco.Range(lineNum, 1, lineNum, 1000),
			options: {
				isWholeLine: true,
				className: "ulang-debug-curr-line",
			}
		}
		this.updateDecorations();
		this.editor.revealPositionInCenter({ lineNumber: lineNum, column: 1 });
	}

	getContent () {
		return this.editor.getValue();
	}

	setContent (source: string) {
		return this.editor.setValue(source);
	}

	setBreakpointListener (listener) {
		this.breakpointListener = listener;
	}
}

function defineUlangLanguage () {
	monaco.languages.register({ id: 'ulang' });
	monaco.languages.setMonarchTokensProvider('ulang', {
		defaultToken: 'invalid',
		keywords: [
			"halt", "brk", "add", "sub", "mul", "div", "divu", "rem", "remu", "addf", "subf", "mulf", "divf", "cosf", "sinf", "atan2", "sqrtf", "powf", "rand", "i2f", "f2i",
			"not", "and", "or", "xor", "shl", "shr", "shru",
			"cmp", "cmpu", "cmpf",
			"jmp", "je", "jne", "jl", "jg", "jle", "jge",
			"mov",
			"ld", "sto", "ldb", "stob", "lds", "stos",
			"push", "stackalloc", "pop",
			"call", "ret", "retn",
			"syscall",
			"reserve", "byte", "short", "int", "float"
		],
		operators: ["~", "+", "-", "|", "&", "^", "/", "*", "%"],
		registers: ["r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "pc", "sp"],
		escapes: /\\(?:[abfnrtv\\"']|x[0-9A-Fa-f]{1,4}|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8})/,
		tokenizer: {
			root: [
				// identifiers and keywords
				[/[a-z_$][\w$]*/, {
					cases: {
						'@keywords': 'keyword',
						"@registers": "attribute.name",
						'@default': 'annotation'
					}
				}],
				{ include: '@whitespace' },
				[/[=><!~?:&|+\-*\/\^%]+/, {
					cases: {
						'@operators': 'operator',
						'@default': ''
					}
				}],
				[/\d*\.\d+([eE][\-+]?\d+)?/, 'number.float'],
				[/0[xX][0-9a-fA-F]+/, 'number.hex'],
				[/\d+/, 'number'],
				[/[;,.]/, 'delimiter'],
				[/"([^"\\]|\\.)*$/, 'string.invalid'],  // non-teminated string
				[/"/, { token: 'string.quote', bracket: '@open', next: '@string' }],
			],
			string: [
				[/[^\\"]+/, 'string'],
				[/@escapes/, 'string.escape'],
				[/\\./, 'string.escape.invalid'],
				[/"/, { token: 'string.quote', bracket: '@close', next: '@pop' }]
			],
			whitespace: [
				[/[ \t\r\n]+/, 'white'],
				[/#.*$/, 'comment'],
			],
		},
	});
}