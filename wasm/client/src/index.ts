import "./ui.css"
import { Editor } from "./editor"
import { setupLayout } from "./layout";
import { setupStorage } from "./storage";
import { Auth, checkAuthorizationCode } from "./auth";
import { loadUlang, VirtualMachine } from "@marioslab/ulang-vm"
import { Debugger } from "./debugger";
import { setupLiveEdit } from "./liveedit"

(async function () {
	await checkAuthorizationCode();
	await loadUlang();
	setupLiveEdit();

	let editor = new Editor("editor-container");
	let virtualMachine = new VirtualMachine("debugger-output");
	new Debugger(editor, virtualMachine, "toolbar-run", "toolbar-continue", "toolbar-pause", "toolbar-step", "toolbar-stop", "debug-view-registers", "debug-view-stack", "debug-view-memory");
	new Auth("toolbar-login", "toolbar-logout", "toolbar-avatar");

	setupLayout();
	setupStorage(editor);
})();