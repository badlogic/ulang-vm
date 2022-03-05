import loadWasm from "./ulang"

let module = {
	onRuntimeInitialized: createWrappers
} as any;

export async function loadUlang () {
	return await loadWasm(module);
}

let ulang_calloc: (numBytes: number) => number;
let ulang_free: (ptr: number) => void;
let ulang_print_memory: () => void;
let ulang_file_from_memory: (namePtr: number, dataPtr: number, filePtr: number) => number;
let ulang_file_free: (filePtr: number) => void;
let ulang_error_print: (errorPtr: number) => void;
let ulang_error_free: (errorPtr: number) => void;
let ulang_compile: (filePtr: number, programPtr: number, errorPtr: number) => number;
let ulang_program_free: (programPtr: number) => void;
let ulang_vm_init: (vmPtr: number, programPtr: number) => void;
let ulang_vm_step: (vmPtr: number) => number;
let ulang_vm_step_n: (vmPtr: number, n: number) => number;
let ulang_vm_step_n_bp: (vmPtr: number, n: number, bpPtr: number, numBp: number) => number;
let ulang_vm_print: (vmPtr: number) => void;
let ulang_vm_pop_int: (vmPtr: number) => number;
let ulang_vm_pop_uint: (vmPtr: number) => number;
let ulang_vm_pop_float: (vmPtr: number) => number;
let ulang_vm_free: (vmPtr: number) => void;
let ulang_sizeof: (type: number) => number;
let ulang_print_offsets: () => void;
let ulang_argb_to_rgba: (argbPtr: number, numPixels: number) => void;

export let getInt8 = (ptr: number) => new DataView(module.HEAPU8.buffer).getInt8(ptr);
export let getInt16 = (ptr: number) => new DataView(module.HEAPU8.buffer).getInt16(ptr, true);
export let getUint32 = (ptr: number) => new DataView(module.HEAPU8.buffer).getUint32(ptr, true);
export let setUint32 = (ptr, val) => new DataView(module.HEAPU8.buffer).setUint32(ptr, val, true);
export let getInt32 = (ptr: number) => new DataView(module.HEAPU8.buffer).getInt32(ptr, true);
export let getFloat32 = (ptr: number) => new DataView(module.HEAPU8.buffer).getFloat32(ptr, true);
export let argbToRgba: (argb: number, numPixels: number) => void;
export let addFunction: (func: any, descriptor: string) => number;
export let UTF8ArrayToString: (heap: Uint8Array, ptr: number) => string;;
export let HEAPU8: () => Uint8Array;

export function createWrappers () {
	ulang_calloc = module.cwrap("ulang_calloc", "ptr", ["number"]);
	ulang_free = module.cwrap("ulang_free", "void", ["ptr"]);
	ulang_print_memory = module.cwrap("ulang_print_memory", "void", []);
	ulang_file_from_memory = module.cwrap("ulang_file_from_memory", "number", ["ptr", "ptr", "ptr"]);
	ulang_file_free = module.cwrap("ulang_file_free", "void", ["ptr"])
	ulang_error_print = module.cwrap("ulang_error_print", "void", ["ptr"]);
	ulang_error_free = module.cwrap("ulang_error_free", "void", ["ptr"]);
	ulang_compile = module.cwrap("ulang_compile", "number", ["ptr", "ptr", "ptr"]);
	ulang_program_free = module.cwrap("ulang_program_free", "void", ["ptr"]);
	ulang_vm_init = module.cwrap("ulang_vm_init", "void", ["ptr", "ptr"]);
	ulang_vm_step = module.cwrap("ulang_vm_step", "number", ["ptr"]);
	ulang_vm_step_n = module.cwrap("ulang_vm_step_n", "number", ["ptr", "number"]);
	ulang_vm_step_n_bp = module.cwrap("ulang_vm_step_n_bp", "number", ["ptr", "number", "ptr", "number"]);
	ulang_vm_print = module.cwrap("ulang_vm_print", "void", ["ptr"]);
	ulang_vm_pop_int = module.cwrap("ulang_vm_pop_int", "number", ["ptr"]);
	ulang_vm_pop_uint = module.cwrap("ulang_vm_pop_uint", "number", ["ptr"]);
	ulang_vm_pop_float = module.cwrap("ulang_vm_pop_float", "number", ["ptr"]);
	ulang_vm_free = module.cwrap("ulang_vm_free", "void", ["ptr"]);
	ulang_sizeof = module.cwrap("ulang_sizeof", "number", ["number"]);
	ulang_print_offsets = module.cwrap("ulang_print_offsets", "void", []);
	argbToRgba = ulang_argb_to_rgba = module.cwrap("ulang_argb_to_rgba", "ptr", ["ptr", "number"]);
	addFunction = module.addFunction;
	UTF8ArrayToString = module.UTF8ArrayToString;
	HEAPU8 = () => module.HEAP8 as Uint8Array;
}

export interface UlangString {
	ptr: number;
	data (): number;
	length (): number;
	toString (): string;
}

export function ptrToUlangString (stringPtr: number): UlangString {
	return {
		ptr: stringPtr,
		data: () => getUint32(stringPtr),
		length: () => getUint32(stringPtr + 4),
		toString: () => module.UTF8ArrayToString(module.HEAPU8, getUint32(stringPtr), getUint32(stringPtr + 4)) as string
	};
}

export interface UlangSpan {
	ptr: number;
	data (): UlangString;
	startLine (): number;
	endLine (): number;
}

export function ptrToUlangSpan (spanPtr: number): UlangSpan {
	return {
		ptr: spanPtr,
		data: () => ptrToUlangString(spanPtr),
		startLine: () => getUint32(spanPtr + 8),
		endLine: () => getUint32(spanPtr + 12)
	}
}

export interface UlangLine {
	ptr: number;
	data (): UlangString;
	lineNumber (): number;
}

export function ptrToUlangLine (linePtr: number): UlangLine {
	return {
		ptr: linePtr,
		data: () => ptrToUlangString(linePtr),
		lineNumber: () => getUint32(linePtr + 8)
	}
}

export interface UlangFile {
	ptr: number;
	fileName (): UlangString;
	data (): UlangString;
	lines (): UlangLine[];
	free (): void;
}

export function ptrToUlangFile (filePtr: number): UlangFile {
	return {
		ptr: filePtr,
		fileName: () => ptrToUlangString(filePtr),
		data: () => ptrToUlangString(filePtr + 8),
		lines: () => {
			let lines = [];
			let linesPtr = getUint32(filePtr + 16);
			let numLines = getUint32(filePtr + 20);
			for (let i = 0; i < numLines + 1; i++) {
				lines.push(ptrToUlangLine(linesPtr));
				linesPtr += 12;
			}
			return lines;
		},
		free: () => {
			ulang_file_free(filePtr);
			ulang_free(filePtr);
		}
	}
}

export interface UlangError {
	ptr: number;
	file (): UlangFile;
	span (): UlangSpan;
	message (): UlangString;
	isSet (): boolean;
	print (): void;
	free (): void;
}

export function ptrToUlangError (errorPtr: number): UlangError {
	return {
		ptr: errorPtr,
		file: () => ptrToUlangFile(getUint32(errorPtr)),
		span: () => ptrToUlangSpan(errorPtr + 4),
		message: () => ptrToUlangString(errorPtr + 20),
		isSet: () => getInt32(errorPtr + 28) != 0,
		print: () => ulang_error_print(errorPtr),
		free: () => {
			ulang_error_free(errorPtr);
			ulang_free(errorPtr);
		}
	}
}

export enum UlangLabelTarget {
	UL_LT_UNINITIALIZED = 0,
	UL_LT_CODE = 1,
	UL_LT_DATA = 2,
	UL_LT_RESERVED_DATA = 3
}

export interface UlangLabel {
	ptr: number;
	label (): UlangSpan;
	target (): UlangLabelTarget;
	address (): number;
}

export function ptrToUlangLabel (labelPtr: number): UlangLabel {
	return {
		ptr: labelPtr,
		label: () => ptrToUlangSpan(labelPtr),
		target: () => getUint32(labelPtr + 16),
		address: () => getUint32(labelPtr + 20)
	}
}

export interface UlangProgram {
	ptr: number;
	code (): DataView;
	data (): DataView;
	reservedBytes (): number;
	labels (): UlangLabel[];
	file (): UlangFile;
	addressToLine (): number[];
	free (): void;
}

export function ptrToUlangProgram (progPtr: number): UlangProgram {
	return {
		ptr: progPtr,
		code: () => new DataView(module.HEAPU8.buffer, getUint32(progPtr), getUint32(progPtr + 4)),
		data: () => new DataView(module.HEAPU8.buffer, getUint32(progPtr + 8), getUint32(progPtr + 12)),
		reservedBytes: () => getUint32(progPtr + 16),
		labels: () => {
			let labels = [];
			let labelsPtr = getUint32(progPtr + 20);
			let labelsLength = getUint32(progPtr + 24);
			for (let i = 0; i < labelsLength; i++) {
				labels.push(ptrToUlangLabel(labelsPtr));
				labelsPtr += 24;
			}
			return labels;
		},
		file: () => {
			return ptrToUlangFile(progPtr + 28);
		},
		addressToLine: () => {
			let addressToLine = [];
			let addressToLinePtr = getUint32(progPtr + 32);
			let addressToLineLength = getUint32(progPtr + 36);
			for (let i = 0; i < addressToLineLength; i++) {
				addressToLine.push(getUint32(addressToLinePtr));
				addressToLinePtr += 4;
			}
			return addressToLine;
		},
		free: () => {
			ulang_program_free(progPtr);
			ulang_free(progPtr);
		}
	}
}

export interface UlangValue {
	ptr: number;
	b (): number;
	s (): number;
	i (): number;
	ui (): number;
	f (): number;
}

export function ptrToUlangValue (valPtr): UlangValue {
	return {
		ptr: valPtr,
		b: () => getInt8(valPtr),
		s: () => getInt16(valPtr),
		i: () => getInt32(valPtr),
		ui: () => getUint32(valPtr),
		f: () => getFloat32(valPtr)
	}
}

export interface UlangVm {
	ptr: number;
	registers (): UlangValue[];
	memory (): DataView;
	memoryPtr (): number;
	error (): UlangError;
	program (): UlangProgram;
	setSyscall (num: number, callPtr: number): void;
	step (): boolean;
	stepN (n: number): boolean;
	stepNBP (n: number, bpPtr: number, numBp: number): number;
	print (): void;
	popInt (): number;
	popUint (): number;
	popFloat (): number;
	free (): void;
}

export function ptrToUlangVm (vmPtr: number): UlangVm {
	return {
		ptr: vmPtr,
		registers: () => {
			let regs = [];
			let regsPtr = vmPtr;
			for (let i = 0; i < 16; i++) {
				regs.push(ptrToUlangValue(regsPtr))
				regsPtr += 4;
			}
			return regs;
		},
		memory: () => new DataView(module.HEAPU8, getUint32(vmPtr + 64), getUint32(vmPtr + 68)),
		memoryPtr: () => getUint32(vmPtr + 64),
		error: () => ptrToUlangError(getUint32(vmPtr + 1096)),
		program: () => ptrToUlangProgram(getUint32(vmPtr + 1128)),
		setSyscall: (num, call) => {
			if (num < 0 || num > 255) return;
			setUint32(vmPtr + 72 + num * 4, call);
		},
		step: () => ulang_vm_step(vmPtr) != 0,
		stepN: (n) => ulang_vm_step_n(vmPtr, n) != 0,
		stepNBP: (n, bpPtr, numBp) => ulang_vm_step_n_bp(vmPtr, n, bpPtr, numBp),
		print: () => ulang_vm_print(vmPtr),
		popInt: () => ulang_vm_pop_int(vmPtr),
		popUint: () => ulang_vm_pop_uint(vmPtr),
		popFloat: () => ulang_vm_pop_float(vmPtr),
		free: () => {
			ulang_vm_free(vmPtr);
			ulang_free(vmPtr);
		}
	}
}

enum UlangType {
	UL_TYPE_FILE = 0,
	UL_TYPE_ERROR = 1,
	UL_TYPE_PROGRAM = 2,
	UL_TYPE_VM = 3
}

export function alloc (numBytes: number) { return ulang_calloc(numBytes); }

export function allocType (type: UlangType) { return ulang_calloc(ulang_sizeof(type)); };

export function free (ptr: number) { return ulang_free(ptr); }

export function printMemory () { ulang_print_memory(); }

export function newFile (sourceName: string, sourceData: string) {
	let name = module.allocateUTF8(sourceName);
	let data = module.allocateUTF8(sourceData);
	let filePtr = allocType(UlangType.UL_TYPE_FILE);
	ulang_file_from_memory(name, data, filePtr);
	module._free(name);
	module._free(data);
	return ptrToUlangFile(filePtr);
}

export function newError () { return ptrToUlangError(allocType(UlangType.UL_TYPE_ERROR)); }

export function newProgram () { return ptrToUlangProgram(allocType(UlangType.UL_TYPE_PROGRAM)); }

export interface UlangCompilationResult {
	error: UlangError;
	file: UlangFile;
	program: UlangProgram;
	free (): void;
}

export function compile (source): UlangCompilationResult {
	let error = newError();
	let file = newFile("source", source);
	let program = newProgram();
	let result = {
		error: error,
		file: file,
		program: program,
		free: () => {
			program.free();
			file.free();
			error.free();
		},
	}
	ulang_compile(result.file.ptr, result.program.ptr, result.error.ptr);
	return result;
};

export function newVm (program: UlangProgram) {
	let vm = ptrToUlangVm(allocType(UlangType.UL_TYPE_VM));
	ulang_vm_init(vm.ptr, program.ptr);
	return vm;
}