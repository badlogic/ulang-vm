Module["onRuntimeInitialized"] = () => {
    var ulang = Module;
    const ulang_calloc = Module.cwrap("ulang_calloc", "ptr", ["number"]);
    const ulang_free = Module.cwrap("ulang_free", "void", ["ptr"]);
    const ulang_print_memory = Module.cwrap("ulang_print_memory", "void", []);
    const ulang_file_from_memory = Module.cwrap("ulang_file_from_memory", "number", ["ptr", "ptr", "ptr"]);
    const ulang_file_free = Module.cwrap("ulang_file_free", "void", ["ptr"])
    const ulang_error_print = Module.cwrap("ulang_error_print", "void", ["ptr"]);
    const ulang_error_free = Module.cwrap("ulang_error_free", "void", ["ptr"]);
    const ulang_compile = Module.cwrap("ulang_compile", "number", ["ptr", "ptr", "ptr"]);
    const ulang_program_free = Module.cwrap("ulang_program_free", "void", ["ptr"]);
    const ulang_vm_init = Module.cwrap("ulang_vm_init", "void", ["ptr", "ptr"]);
    const ulang_vm_step = Module.cwrap("ulang_vm_step", "number", ["ptr"]);
    const ulang_vm_step_n = Module.cwrap("ulang_vm_step_n", "number", ["ptr", "number"]);
    const ulang_vm_print = Module.cwrap("ulang_vm_print", "void", ["ptr"]);
    const ulang_vm_pop_int = Module.cwrap("ulang_vm_pop_int", "number", ["ptr"]);
    const ulang_vm_pop_uint = Module.cwrap("ulang_vm_pop_uint", "number", ["ptr"]);
    const ulang_vm_pop_float = Module.cwrap("ulang_vm_pop_float", "number", ["ptr"]);
    const ulang_vm_free = Module.cwrap("ulang_vm_free", "void", ["ptr"]);
    const ulang_sizeof = Module.cwrap("ulang_sizeof", "number", ["number"]);
    const ulang_print_offsets = Module.cwrap("ulang_print_offsets", "void", []);
    const ulang_argb_to_rgba = Module.cwrap("ulang_argb_to_rgba", "ptr", ["ptr", "number"]);

    ulang_print_offsets();

    ulang.argbToRgba = (ptr, numPixels) => ulang_argb_to_rgba(ptr, numPixels);

    const UL_TYPE_FILE = 0;
    const UL_TYPE_ERROR = 1;
    const UL_TYPE_PROGRAM = 2;
    const UL_TYPE_VM = 3;

    let getInt8 = (ptr) => new DataView(Module.HEAPU8.buffer).getInt8(ptr);
    let getInt16 = (ptr) => new DataView(Module.HEAPU8.buffer).getInt16(ptr, true);
    let getUint32 = (ptr) => new DataView(Module.HEAPU8.buffer).getUint32(ptr, true);
    let setUint32 = (ptr, val) => new DataView(Module.HEAPU8.buffer).setUint32(ptr, val, true);
    let getInt32 = (ptr) => new DataView(Module.HEAPU8.buffer).getInt32(ptr, true);
    let getFloat32 = (ptr) => new DataView(Module.HEAPU8.buffer).getFloat32(ptr, true);
    let allocType = (type) => ulang_calloc(ulang_sizeof(type));

    let nativeToJsString = (stringPtr) => {
        return {
            ptr: stringPtr,
            data: () => getUint32(stringPtr),
            length: () => getUint32(stringPtr + 4),
            toString: () => Module.UTF8ArrayToString(Module.HEAPU8, getUint32(stringPtr), getUint32(stringPtr + 4))
        };
    }

    let nativeToJsSpan = (spanPtr) => {
        return {
            ptr: spanPtr,
            data: () => nativeToJsString(spanPtr),
            startLine: () => getUint32(spanPtr + 8),
            endLine: () => getUint32(spanPtr + 12)
        }
    }

    let nativeToJsLine = (linePtr) => {
        return {
            ptr: linePtr,
            data: () => nativeToJsString(linePtr),
            lineNumber: () => getUint32(linePtr + 8)
        }
    }

    let nativeToJsFile = (filePtr) => {
        return {
            ptr: filePtr,
            fileName: () => nativeToJsString(filePtr),
            data: () => nativeToJsString(filePtr + 8),
            lines: () => {
                let lines = [];
                let linesPtr = getUint32(filePtr + 16);
                let numLines = getUint32(filePtr + 20);
                for (let i = 0; i < numLines + 1; i++) {
                    lines.push(nativeToJsLine(linesPtr));
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

    let nativeToJsError = (errorPtr) => {
        return {
            ptr: errorPtr,
            file: () => nativeToJsFile(getUint32(errorPtr)),
            span: () => nativeToJsSpan(errorPtr + 4),
            message: () => nativeToJsString(errorPtr + 20),
            isSet: () => getInt32(errorPtr + 28) != 0,
            print: () => ulang_error_print(errorPtr),
            free: () => {
                ulang_error_free(errorPtr);
                ulang_free(errorPtr);
            }
        }
    }

    let nativeToJsLabel = (labelPtr) => {
        return {
            ptr: labelPtr,
            label: () => nativeToJsSpan(labelPtr),
            target: () => getUint32(labelPtr + 16),
            address: () => getUint32(labelPtr + 20)
        }
    }

    let nativeToJsProgram = (progPtr) => {
        return {
            ptr: progPtr,
            code: () => new DataView(Module.HEAPU8.buffer, getUint32(progPtr), getUint32(progPtr + 4)),
            data: () => new DataView(Module.HEAPU8.buffer, getUint32(progPtr + 8), getUint32(progPtr + 12)),
            reservedBytes: () => getUint32(progPtr + 16),
            labels: () => {
                let labels = [];
                let labelsPtr = getUint32(progPtr + 20);
                let labelsLength = getUint32(progPtr + 24);
                for (let i = 0; i < labelsLength; i++) {
                    labels.push(nativeToJsLabel(labelsPtr));
                    labelsPtr += 24;
                }
                return labels;
            },
            file: () => {
                return nativeToJsFile(progPtr + 28);
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

    let nativeToJsValue = (valPtr) => {
        return {
            ptr: valPtr,
            b: () => getInt8(valPtr),
            s: () => getInt16(valPtr),
            i: () => getInt32(valPtr),
            ui: () => getUint32(valPtr),
            f: () => getFloat32(valPtr)
        }
    }

    let nativeToJsVm = ulang.nativeToJsVm = (vmPtr) => {
        return {
            ptr: vmPtr,
            registers: () => {
                let regs = [];
                let regsPtr = vmPtr;
                for (let i = 0; i < 16; i++) {
                    regs.push(nativeToJsValue(regsPtr))
                    regsPtr += 4;
                }
                return regs;
            },
            memory: () => new DataView(Module.HEAPU8, getUint32(vmPtr + 64), getUint32(vmPtr + 68)),
            memoryPtr: () => getUint32(vmPtr + 64),
            setSyscall: (num, call) => {
                if (num < 0 || num > 255) return;
                setUint32(vmPtr + 72 + num * 4, call);
            },
            error: () => nativeToJsError(vmPtr + 1096),
            program: () => nativeToJsProgram(vmPtr + 1128),
            step: () => ulang_vm_step(vmPtr) != 0,
            stepN: (n) => ulang_vm_step_n(vmPtr, n) != 0,
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

    ulang.UL_LT_UNINITIALIZED = 0;
    ulang.UL_LT_CODE = 1;
    ulang.UL_LT_DATA = 2;
    ulang.UL_LT_RESERVED_DATA = 3;

    ulang.alloc = (numBytes) => ulang_calloc(numBytes);

    ulang.free = (ptr) => ulang_free(ptr);

    ulang.printMemory = () => ulang_print_memory();

    ulang.newFile = (sourceName, sourceData) => {
        let name = Module.allocateUTF8(sourceName);
        let data = Module.allocateUTF8(sourceData);
        let filePtr = allocType(UL_TYPE_FILE);
        ulang_file_from_memory(name, data, filePtr);
        Module._free(name);
        Module._free(data);
        return nativeToJsFile(filePtr);
    }

    ulang.newError = () => nativeToJsError(allocType(UL_TYPE_ERROR));

    ulang.newProgram = () => nativeToJsProgram(allocType(UL_TYPE_PROGRAM));

    ulang.newVm = (program) => {
        let vm = nativeToJsVm(allocType(UL_TYPE_VM));
        ulang_vm_init(vm.ptr, program.ptr);
        return vm;
    }

    ulang.compile = (file, program, error) => {
        return ulang_compile(file.ptr, program.ptr, error.ptr) != 0;
    }
};

Module["print"] = (str) => console.log(str);
Module["printErr"] = (str) => console.error(str);
