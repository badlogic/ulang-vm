#!/usr/bin/env node

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

require('esbuild').build({
	entryPoints: {
		editor: "client/src/page-editor.ts",
		index: "client/src/page-index.ts",
		"editor.worker": "monaco-editor/esm/vs/editor/editor.worker.js",
	},
	bundle: true,
	tsconfig: "client/tsconfig.json",
	sourcemap: true,
	outdir: 'client/assets/bundle',
	loader: {
		".ttf": "dataurl",
		".ul": "text",
		".html": "text"
	},
	watch: watch,
	logLevel: 'info'
}).catch(() => process.exit(1))