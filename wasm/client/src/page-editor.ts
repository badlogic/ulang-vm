import "./common.css"
import "./page-editor.css"
import { Editor } from "./components/editor"
import { setupLayout } from "./layout";
import { auth, checkAuthorizationCode } from "./auth";
import { Breakpoint, loadUlang, VirtualMachine } from "@marioslab/ulang-vm"
import { Debugger } from "./components/debugger";
import { setupLiveEdit } from "./liveedit"
import { loadProject, project } from "./project";
import { showDialog } from "./components/dialog";
import { Toolbar } from "./components/toolbar";
import { decode } from "html-entities";
import { Explorer } from "./components/explorer";

(async function () {
	await checkAuthorizationCode();
	await loadUlang();
	await loadProject();

	let toolbar = new Toolbar(document.querySelector(".toolbar"), true); // 	
	let editor = new Editor(document.querySelector("#editor-container"));
	let virtualMachine = new VirtualMachine("debugger-output");
	new Debugger(editor, virtualMachine, toolbar, document.querySelector("#debug-views-container"));
	let explorer = new Explorer(document.querySelector("#explorer"), project);

	setupLiveEdit();
	setupLayout();
	setupUIEvents(explorer, editor, toolbar, virtualMachine);

	(document.querySelector(".main") as HTMLElement).style.display = "flex";
})();

function setupUIEvents (explorer: Explorer, editor: Editor, toolbar: Toolbar, virtualMachine: VirtualMachine) {
	// Setup toolbar
	let titleInput = toolbar.getTitleInput()
	titleInput.value = decode(project.getTitle());
	titleInput.addEventListener("input", () => {
		if (titleInput.value.trim().length == 0) {
			showDialog("Sorry", "<p>Title can't be empty.</p>", [], true, "OK");
			titleInput.value = "Untitled";
		}
		project.setTitle(titleInput.value);
	})
	if (project.getId() && project.getOwner() != auth.getUsername()) toolbar.setAuthor(project.getOwner());

	if (project.getForkedFrom()) toolbar.setForkedFrom(project.getForkedFrom());

	let unsavedLabel = toolbar.getUnsavedLabel();
	project.setUnsavedListener((isUnsaved) => {
		unsavedLabel.textContent = isUnsaved ? "(unsaved)" : "";
	})
	unsavedLabel.textContent = project.isUnsaved() ? "(unsaved)" : "";

	let newButton = toolbar.getNewButton();
	newButton.addEventListener("click", () => window.location.href = "/editor");

	let saveButton = toolbar.getSaveButton();
	saveButton.addEventListener("click", () => project.save());

	let screenshotButton = toolbar.getScreenshotButton();
	screenshotButton.addEventListener("click", () => project.setScreenshot(virtualMachine.takeScreenshot()));

	let downloadButton = toolbar.getDownloadButton();
	downloadButton.addEventListener("click", () => project.download());

	// Setup explorer & editor
	editor.setContentListener((filename, content) => {
		project.setFileContent(filename, content);
	});

	explorer.setSelectedCallback((filename) => {
		if (filename.endsWith(".ul")) {
			editor.openFile(filename, project.getFileContent(filename));
		}
	})

	// Load all files into the editor, so we can toggle breakpoints
	for (let filename of project.getFilenames()) {
		explorer.selectFile(filename);
	}

	if (project.getId()) {
		let bpsJson = localStorage.getItem("bps-" + project.getId());
		if (bpsJson) {
			let bps = JSON.parse(bpsJson);
			for (let bp of bps) {
				try {
					let breakpoint = bp as Breakpoint;
					editor.toggleBreakpoint(breakpoint.filename, breakpoint.lineNumber);
				} catch (e) {
					// no, breakpoint/source mismatch
				}
			}
		}
	}
	explorer.selectFile("program.ul");

	editor.setBreakpointListener((bps: Breakpoint[]) => {
		if (project.getId()) localStorage.setItem("bps-" + project.getId(), JSON.stringify(bps));
	});

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

	if (!location.href.includes("localhost")) {
		window.addEventListener("beforeunload", (ev) => {
			if (!project.isUnsaved() || localStorage.getItem("authorize-project")) {
				delete ev['returnValue'];
			} else {
				ev.returnValue = true;
			}
		});
	}
}