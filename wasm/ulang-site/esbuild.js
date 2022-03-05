#!/usr/bin/env node

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

require('esbuild').build({
	entryPoints: {
		index: "ulang-site/src/index.ts",
		"editor.worker": "monaco-editor/esm/vs/editor/editor.worker.js",
		"ts.worker": "monaco-editor/esm/vs/language/typescript/ts.worker",
	},
	bundle: true,
	tsconfig: "ulang-site/tsconfig.json",
	sourcemap: true,
	outdir: 'ulang-site/assets/js',
	loader: {
		".ttf": "file",
	},
	watch: watch,
	logLevel: 'info'
}).catch(() => process.exit(1))