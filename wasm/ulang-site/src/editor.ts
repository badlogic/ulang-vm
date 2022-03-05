import "monaco-editor/min/vs/editor/editor.main.css"
import "./editor.css"
import * as monaco from "monaco-editor";
import * as ulang from "@marioslab/ulang-vm"

(globalThis as any).self.MonacoEnvironment = {
	getWorkerUrl: function (moduleId, label) {
		if (label === "typescript" || label === "javascript") {
			return "./js/ts.worker.js";
		}
		return "./js/editor.worker.js";
	},
};

export class Editor {
	private editor: monaco.editor.IStandaloneCodeEditor;
	private decorations: string[] = [];
	private breakpoints: monaco.editor.IModelDeltaDecoration[] = [];
	private breakpointListener: (bps: string[]) => void = null;
	private currLine: monaco.editor.IModelDeltaDecoration = null;

	constructor (elementId: string) {
		defineUlangLanguage();
		this.editor = monaco.editor.create(document.getElementById(elementId), {
			automaticLayout: true,
			theme: "vs-dark",
			language: "ulang",
			glyphMargin: true,
			tabSize: 3
		});

		this.editor.onDidChangeModelContent(() => this.onDidChangeModelContent());

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

	private updateDecorations () {
		let newDecorations: monaco.editor.IModelDeltaDecoration[] = [];
		for (var bpLine in this.breakpoints) {
			var bpDecoration = this.breakpoints[bpLine];
			if (bpDecoration) newDecorations.push(bpDecoration);
		}
		if (this.currLine) newDecorations.push(this.currLine);
		this.decorations = this.editor.deltaDecorations(this.decorations, newDecorations);
	}

	private toggleBreakpoint (lineNumber: number) {
		let previousDecoration = this.breakpoints[lineNumber];
		if (previousDecoration) {
			delete this.breakpoints[lineNumber];
		} else {
			this.breakpoints[lineNumber] = {
				range: new monaco.Range(lineNumber, 1, lineNumber, 1),
				options: {
					isWholeLine: true,
					glyphMarginClassName: 'ulang-debug-breakpoint'
				}
			};
		}
		if (this.breakpointListener) this.breakpointListener(this.getBreakpoints());
		this.updateDecorations();
	}

	private getBreakpoints () {
		var bpLines: string[] = [];
		for (var bp in this.breakpoints) {
			bpLines.push(bp);
		}
		return bpLines;
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