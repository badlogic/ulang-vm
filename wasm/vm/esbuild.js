#!/usr/bin/env node

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

require('esbuild').build({
	entryPoints: ["vm/src/index.ts"],
	bundle: true,
	tsconfig: "vm/tsconfig.json",
	sourcemap: true,
	outfile: 'vm/dist/iife/ulang.js',
	format: "iife",
	globalName: "ulang",
	watch: watch,
	logLevel: 'info'
}).catch(() => process.exit(1))