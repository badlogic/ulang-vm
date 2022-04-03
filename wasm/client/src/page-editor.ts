import "./common.css"
import "./page-editor.css"
import { Editor } from "./components/editor"
import { setupLayout } from "./layout";
import { Auth, checkAuthorizationCode } from "./auth";
import { loadUlang, VirtualMachine } from "@marioslab/ulang-vm"
import { Debugger } from "./debugger";
import { setupLiveEdit } from "./liveedit"
import { loadProject, project } from "./project";
import { showDialog } from "./components/dialog";
import { createToolbar } from "./components/toolbar";

(async function () {
	await checkAuthorizationCode();
	await loadUlang();

	new Auth();
	createToolbar(document.querySelector(".toolbar"), true); // 	
	let editor = new Editor(document.querySelector("#editor-container"));
	let virtualMachine = new VirtualMachine("debugger-output");
	new Debugger(editor, virtualMachine, "toolbar-run", "toolbar-continue", "toolbar-pause", "toolbar-step", "toolbar-stop", "debug-view-registers", "debug-view-stack", "debug-view-memory");

	loadProject(editor, "toolbar-title", "toolbar-author", "toolbar-forked-from", "toolbar-unsaved");
	setupLiveEdit();
	setupLayout();
	setupUIEvents(editor);

	(document.getElementsByClassName("main")[0] as HTMLElement).style.display = "flex";
})();

function setupUIEvents (editor: Editor) {
	let titleInput = document.getElementById("toolbar-title") as HTMLInputElement;
	titleInput.addEventListener("input", () => {
		if (titleInput.value.trim().length == 0) {
			showDialog("Sorry", "<p>Title can't be empty.</p>", [], true, "OK");
			titleInput.value = "Untitled";
		}
		project.setTitle(titleInput.value);
	})

	editor.setContentListener((content) => {
		project.setSource(content);
	});

	let newButton = document.getElementById("toolbar-new");
	newButton.addEventListener("click", () => window.location.href = "/editor");

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