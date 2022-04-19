import "monaco-editor/min/vs/editor/editor.main.css"
import * as monaco from "monaco-editor";
import * as ulang from "@marioslab/ulang-vm"
import { explorer } from "./explorer";
import { UlangFile } from "@marioslab/ulang-vm/src/wrapper";
import { Breakpoint } from "@marioslab/ulang-vm";
import { project } from "src/project";

(globalThis as any).self.MonacoEnvironment = {
	getWorkerUrl: function (moduleId, label) {
		return "/bundle/editor.worker.js";
	},
};

export interface SourceFile {
	filename: string;
	model: monaco.editor.IModel;
	decorations: string[];
	breakpoints: Breakpoint[];
	currLine: number;
	viewState: monaco.editor.ICodeEditorViewState;
}

export class Editor {
	private editor: monaco.editor.IStandaloneCodeEditor;
	private sourceFiles: { [filename: string]: SourceFile } = {};
	private currSourceFile: SourceFile = null;
	private breakpointListeners: ((bps: Breakpoint[]) => void)[] = [];
	private contentListener: (filename: string, content: string) => void = null;

	constructor (private container: HTMLElement) {
		defineUlangLanguage();
		this.editor = monaco.editor.create(this.container, {
			automaticLayout: true,
			theme: "vs-dark",
			language: "ulang",
			glyphMargin: true,
			tabSize: 3
		});

		this.editor.onDidChangeModelContent(() => this.onDidChangeModelContent());
		this.editor.onDidChangeModelDecorations((e) => this.onDidChangeModelDecorations(e));

		this.editor.onMouseDown((e) => {
			if (e.target.type != 2 && e.target.type != 3) return;
			this.toggleBreakpoint(this.currSourceFile.filename, e.target.range.startLineNumber);
		});
	}

	private onDidChangeModelContent () {
		if (this.contentListener) this.contentListener(this.currSourceFile.filename, this.editor.getValue());
		let result = ulang.compile(explorer.getSelectedFile(), (filename) => {
			if (!project.fileExists(filename)) return null;
			else return project.getFileContent(filename);
		});
		if (result.error.isSet()) {
			result.error.print();
			let file: UlangFile = result.error.file();
			let startLineNumber = 0;
			let endLineNumber = 0;
			let startCol = 1;
			let endCol = 1;
			if (file) {
				let base = file.data().data();
				startLineNumber = result.error.span().startLine();
				let startLine = file.lines()[startLineNumber].data().data() - base;
				endLineNumber = result.error.span().endLine();
				let endLine = file.lines()[endLineNumber].data().data() - base;

				let spanStart = result.error.span().data().data() - base;
				let spanEnd = spanStart + result.error.span().data().length();

				startCol = spanStart - startLine + 1;
				endCol = spanEnd - endLine + 1;
			}

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

	private onDidChangeModelDecorations (e: monaco.editor.IModelDecorationsChangedEvent) {
		let breakpoints = [];
		let decorations = this.editor.getModel().getAllDecorations();
		for (let i = 0; i < decorations.length; i++) {
			let decoration = decorations[i];
			if (decoration.options.glyphMarginClassName == 'ulang-debug-breakpoint') {
				let breakpoint = JSON.parse((decoration.options as any).description) as Breakpoint;
				breakpoint.lineNumber = decoration.range.startLineNumber;
				breakpoints.push(breakpoint);
			}
		}
		this.currSourceFile.breakpoints = breakpoints;
		this.emitBreakpoints();
	}

	private updateDecorations () {
		let newDecorations: monaco.editor.IModelDeltaDecoration[] = [];
		for (let bp of this.currSourceFile.breakpoints) {
			newDecorations.push({
				range: new monaco.Range(bp.lineNumber, 1, bp.lineNumber, 1),
				options: {
					isWholeLine: true,
					glyphMarginClassName: 'ulang-debug-breakpoint',
					stickiness: monaco.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges,
					description: JSON.stringify(bp)
				},
			} as any);
		}
		if (this.currSourceFile.currLine != -1) {
			newDecorations.push({
				range: new monaco.Range(this.currSourceFile.currLine, 1, this.currSourceFile.currLine, 1000),
				options: {
					isWholeLine: true,
					className: "ulang-debug-curr-line",
				}
			});
		}
		this.currSourceFile.decorations = this.editor.deltaDecorations(this.currSourceFile.decorations, newDecorations);
	}

	toggleBreakpoint (filename: string, lineNumber: number) {
		let foundBreakpoint = false;
		let sourceFile = this.sourceFiles[filename];
		sourceFile.breakpoints = sourceFile.breakpoints.filter(e => {
			if (e.lineNumber == lineNumber) {
				foundBreakpoint = true;
				return false;
			} else {
				return true;
			}
		})
		if (!foundBreakpoint) {
			sourceFile.breakpoints.push({ filename: filename, lineNumber: lineNumber });
		}
		this.emitBreakpoints();
		this.updateDecorations();
	}

	private emitBreakpoints () {
		let allBreakpoints: Breakpoint[] = [];
		for (let file in this.sourceFiles) {
			let sourceFile = this.sourceFiles[file];
			for (let bp of sourceFile.breakpoints)
				allBreakpoints.push(bp);
		}
		for (let l of this.breakpointListeners) l(allBreakpoints);
	}

	setCurrLine (filename: string, lineNumber: number) {
		this.currSourceFile.currLine = -1;
		if (lineNumber == -1) {
			this.currSourceFile.currLine = -1;
			this.updateDecorations();
			return;
		}
		this.openFile(filename);
		this.currSourceFile.currLine = lineNumber;
		this.updateDecorations();
		this.editor.revealPositionInCenter({ lineNumber: lineNumber, column: 1 });
		explorer.selectFile(filename, false)
	}

	getContent () {
		return this.editor.getValue();
	}

	openFile (filename: string, source?: string) {
		if (this.currSourceFile) {
			this.currSourceFile.viewState = this.editor.saveViewState()
		}
		this.currSourceFile = this.sourceFiles[filename];
		if (!this.currSourceFile) {
			let model = monaco.editor.createModel(source ? source : "", "ulang", monaco.Uri.parse("f:/" + filename));
			this.currSourceFile = {
				filename: filename,
				model: model,
				decorations: [],
				breakpoints: [],
				currLine: -1,
				viewState: null
			};
			this.sourceFiles[filename] = this.currSourceFile;
		}
		this.editor.setModel(this.currSourceFile.model);
		if (this.currSourceFile.viewState) {
			this.editor.restoreViewState(this.currSourceFile.viewState);
			this.updateDecorations();
		}
		this.onDidChangeModelContent();
	}

	setBreakpointListener (listener: (bps: Breakpoint[]) => void) {
		this.breakpointListeners.push(listener);
	}

	setContentListener (listener: (filename: string, content: string) => void) {
		this.contentListener = listener;
	}

	getDOM () {
		return this.container;
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
			"reserve", "byte", "short", "int", "float",
			"const",
			"include"
		],
		operators: ["~", "+", "-", "|", "&", "^", "/", "*", "%"],
		registers: ["r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "pc", "sp"],
		escapes: /\\(?:[abfnrtv\\"']|x[0-9A-Fa-f]{1,4}|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8})/,
		tokenizer: {
			root: [
				// identifiers and keywords
				[/[a-zA-Z_$][\w$_]*/, {
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