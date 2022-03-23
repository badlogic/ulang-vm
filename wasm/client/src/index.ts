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

	await loadProject(editor, "toolbar-title", "toolbar-author");
	setupLiveEdit();
	setupLayout();
	setupUIEvents(editor);

	(document.getElementsByClassName("main")[0] as HTMLElement).style.display = "flex";
})();

function setupUIEvents (editor: Editor) {
	let titleInput = document.getElementById("toolbar-title") as HTMLInputElement;
	titleInput.addEventListener("input", () => {
		project.setTitle(titleInput.value);
	})

	editor.setContentListener((content) => {
		project.setSource(content);
	});

	let unsavedLabel = document.getElementById("toolbar-unsaved") as HTMLDivElement;
	project.setUnsavedListener((isUnsaved) => {
		unsavedLabel.textContent = isUnsaved ? "(unsaved)" : "";
	})
	unsavedLabel.textContent = project.isUnsaved() ? "(unsaved)" : "";

	let newButton = document.getElementById("toolbar-new");
	newButton.addEventListener("click", () => window.location.href = "/");

	let saveButton = document.getElementById("toolbar-save");
	saveButton.addEventListener("click", () => project.save());

	let downloadButton = document.getElementById("toolbar-download");
	downloadButton.addEventListener("click", () => project.download());

	document.body.addEventListener("keydown", (ev) => {
		if ((ev.metaKey || ev.ctrlKey)) {
			if (ev.key == "s") {
				ev.preventDefault();
				project.save();
			}
			if (ev.key == "d") {
				ev.preventDefault();
				console.log("downloading");
			}
		}
	}, false);

	window.addEventListener("beforeunload", (ev) => {
		if (!project.isUnsaved() || localStorage.getItem("authorize-project")) {
			delete ev['returnValue'];
		} else {
			ev.returnValue = true;
		}
	});
}