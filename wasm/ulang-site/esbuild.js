#!/usr/bin/env node

let watch = process.argv.length >= 3 && process.argv[2] == "-watch";

require('esbuild').build({
	entryPoints: ["ulang-site/src/index.ts"],
	bundle: true,
	tsconfig: "ulang-site/tsconfig.json",
	sourcemap: true,
	outfile: 'ulang-site/assets/js/index.js',
	watch: watch,
	logLevel: 'info'
}).catch(() => process.exit(1))