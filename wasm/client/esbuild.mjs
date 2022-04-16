#!/usr/bin/env node

import TsconfigPathsPlugin from '@esbuild-plugins/tsconfig-paths'
import esbuild from "esbuild"

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

esbuild.build({
	entryPoints: {
		editor: "client/src/page-editor.ts",
		index: "client/src/page-index.ts",
		user: "client/src/page-user.ts",
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
	logLevel: 'info',
	plugins: [TsconfigPathsPlugin.TsconfigPathsPlugin({ tsconfig: "client/tsconfig.json" })],
}).catch(() => process.exit(1))