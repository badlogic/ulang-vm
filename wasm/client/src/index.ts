import "./ui.css"
import { Editor } from "./editor"
import { setupLayout } from "./layout";
import { Auth, checkAuthorizationCode } from "./auth";
import { loadUlang, VirtualMachine } from "@marioslab/ulang-vm"
import { Debugger } from "./debugger";
import { setupLiveEdit } from "./liveedit"
import { loadProject, project } from "./project";

(async function () {
	await checkAuthorizationCode();
	await loadUlang();

	let editor = new Editor("editor-container");
	let virtualMachine = new VirtualMachine("debugger-output");
	new Debugger(editor, virtualMachine, "toolbar-run", "toolbar-continue", "toolbar-pause", "toolbar-step", "toolbar-stop", "debug-view-registers", "debug-view-stack", "debug-view-memory");
	new Auth("toolbar-login", "toolbar-logout", "toolbar-avatar");

	setupLiveEdit();
	setupLayout();
	setupUIEvents(editor);

	await loadProject(editor, document.getElementById("toolbar-title") as HTMLInputElement);

	(document.getElementsByClassName("main")[0] as HTMLElement).style.display = "flex";
})();

function setupUIEvents (editor: Editor) {
	let titleInput = document.getElementById("toolbar-title") as HTMLInputElement;
	titleInput.addEventListener("change", () => {
		project.setTitle(titleInput.value);
	})

	let downloadButton = document.getElementById("toolbar-download");
	downloadButton.addEventListener("click", () => project.download());

	let save = document.getElementById("toolbar-save");
	save.addEventListener("click", () => project.save());

	editor.setContentListener((content) => {
		project.setSource(content);
	});

	editor.getDOM().addEventListener("keydown", (ev) => {
		if ((ev.metaKey || ev.ctrlKey)) {
			ev.preventDefault();
			if (ev.key == "s") {
				project.save();
			}
			if (ev.key == "d") {
				ev.preventDefault();
				console.log("downloading");
			}
		}
	}, false);
}