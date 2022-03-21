import { auth } from "./auth";
import { saveAs } from "file-saver";
import { Editor } from "./editor";

export let project: Project;

export async function loadProject (editor: Editor, titleInput: HTMLInputElement) {
	project = new Project("Untitled", null, null, "");
	let lastSource = localStorage.getItem("source");
	if (lastSource != null) { editor.setContent(lastSource); }

	titleInput.value = project.getTitle();
}

export class Project {
	private title: string;
	private owner: string;
	private id: string;
	private source: string;

	constructor (title: string, owner: string, id: string, source: string) {
		this.title = title || "Untitled";
		this.owner = owner;
		this.id = id;
		this.source = source;
	}

	download (): any {
		var file = new File([this.source], "source.ul", { type: "text/plain;charset=utf-8" });
		saveAs(file);
	}

	save () {
		if (!auth.isAuthenticated()) {
			auth.login();
			return;
		}
		throw new Error("Not implemented");
	}

	setTitle (title: string) {
		this.title = title.trim();
	}

	getTitle () {
		return this.title;
	}

	setSource (source: any) {
		this.source = source;
	}

	getSource () {
		return this.source
	}
}