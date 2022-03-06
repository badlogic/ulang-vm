#!/usr/bin/env node

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

require('esbuild').build({
	entryPoints: {
		index: "ulang-site/src/index.ts",
		"editor.worker": "monaco-editor/esm/vs/editor/editor.worker.js",
	},
	bundle: true,
	tsconfig: "ulang-site/tsconfig.json",
	sourcemap: true,
	outdir: 'ulang-site/assets/bundle',
	loader: {
		".ttf": "file",
		".ul": "text"
	},
	watch: watch,
	logLevel: 'info'
}).catch(() => process.exit(1))