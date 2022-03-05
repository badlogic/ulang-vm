import { Editor } from "./editor"
import { loadUlang } from "@marioslab/ulang-vm"

(async function () {
	await loadUlang();
	let editor = new Editor("container");
})();