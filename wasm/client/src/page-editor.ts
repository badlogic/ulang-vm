import "./common.css"
import "./page-editor.css"
import { Editor } from "./components/editor"
import { setupLayout } from "./layout";
import { auth, checkAuthorizationCode } from "./auth";
import { loadUlang, VirtualMachine } from "@marioslab/ulang-vm"
import { Debugger } from "./components/debugger";
import { setupLiveEdit } from "./liveedit"
import { loadProject, project } from "./project";
import { showDialog } from "./components/dialog";
import { createToolbar } from "./components/toolbar";

(async function () {
	await checkAuthorizationCode();
	await loadUlang();

	let toolbar = createToolbar(document.querySelector(".toolbar"), true); // 	
	let editor = new Editor(document.querySelector("#editor-container"));
	let virtualMachine = new VirtualMachine("debugger-output");
	new Debugger(editor, virtualMachine, toolbar, document.querySelector("#debug-views-container"));

	await loadProject();
	setupLiveEdit();
	setupLayout();
	setupUIEvents(editor, toolbar);

	(document.querySelector(".main") as HTMLElement).style.display = "flex";
})();

function setupUIEvents (editor: Editor, toolbar: HTMLElement) {
	let titleInput = toolbar.querySelector(".toolbar-title") as HTMLInputElement;
	titleInput.value = project.getTitle();
	titleInput.addEventListener("input", () => {
		if (titleInput.value.trim().length == 0) {
			showDialog("Sorry", "<p>Title can't be empty.</p>", [], true, "OK");
			titleInput.value = "Untitled";
		}
		project.setTitle(titleInput.value);
	})

	let authorLabel = toolbar.querySelector(".toolbar-author") as HTMLDivElement;
	if (project.getId()) {
		authorLabel.innerHTML = project.getOwner() != auth.getUsername() ? `<span style="margin-right: 0.5em">by</span><a href="https://github.com/${project.getOwner()}">${project.getOwner()}</a>` : "";
	} else {
		authorLabel.innerHTML = "";
	}

	let forkedFromLabel = toolbar.querySelector(".toolbar-forked-from") as HTMLDivElement;
	if (project.getForkedFrom()) {
		let fork = project.getForkedFrom();
		forkedFromLabel.innerHTML = `<span style="margin-right: 0.5em">forked from</span><a href="/editor/${fork.id}">${fork.owner}</a>`;
	} else {
		forkedFromLabel.innerHTML = "";
	}

	let unsavedLabel = toolbar.querySelector(".toolbar-unsaved") as HTMLDivElement;
	project.setUnsavedListener((isUnsaved) => {
		unsavedLabel.textContent = isUnsaved ? "(unsaved)" : "";
	})
	unsavedLabel.textContent = project.isUnsaved() ? "(unsaved)" : "";

	editor.setContent(project.getSource());

	editor.setContentListener((content) => {
		project.setSource(content);
	});

	if (project.getId()) {
		let bpsJson = localStorage.getItem("bps-" + project.getId());
		if (bpsJson) {
			let bps = JSON.parse(bpsJson);
			for (let bp of bps) {
				try {
					editor.toggleBreakpoint(bp);
				} catch (e) {
					// no, breakpoint/source mismatch
				}
			}
		}
	}

	editor.setBreakpointListener((bps: number[]) => {
		if (project.getId()) localStorage.setItem("bps-" + project.getId(), JSON.stringify(bps));
	});

	let newButton = toolbar.querySelector(".toolbar-new");
	newButton.addEventListener("click", () => window.location.href = "/editor");

	let saveButton = document.querySelector(".toolbar-save");
	saveButton.addEventListener("click", () => project.save());

	let downloadButton = toolbar.querySelector(".toolbar-download");
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