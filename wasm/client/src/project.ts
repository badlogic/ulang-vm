import { auth } from "./auth";
import { saveAs } from "file-saver";
import { Editor } from "./editor";
import querystring from "query-string"
import axios from "axios";
import { getGist } from "./gist";
import { showDialog } from "./ui";

export let project: Project;

export async function loadProject (editor: Editor, _titleLabel: string, _authorLabel: string) {
	let authorizeProject = JSON.parse(localStorage.getItem("authorize-project"));
	let params: any = querystring.parse(location.search);

	if (!params.code && authorizeProject) {
		project = new Project("", null, null, authorizeProject["source"]);
		project.setTitle(authorizeProject["title"]);
		localStorage.removeItem("authorize-project");
	} else if (params && params.id) {
		let gist = await getGist(params.id);
		if (gist && gist.files["source.ul"]) {
			project = new Project(gist.description, gist.owner.login, gist.id, gist.files["source.ul"].content);
		} else {
			showDialog("Sorry", "<p>This Gist could not be loaded.</p>", [], true, "OK", () => {
				window.location.href = "/";
			});
		}
	}
	if (!project) project = new Project("Untitled", null, null, "");

	let titleInput = document.getElementById(_titleLabel) as HTMLInputElement;
	let authorLabel = document.getElementById(_authorLabel) as HTMLDivElement;
	titleInput.value = project.getTitle();
	if (project.getId()) {
		authorLabel.innerHTML = project.getOwner() != auth.getUsername() ? `by <a href="https://github.com/${project.getOwner()}">${project.getOwner()}</a>` : "";
	} else {
		authorLabel.innerHTML = "";
	}
	editor.setContent(project.getSource());
}

export class Project {
	private title: string;
	private owner: string;
	private id: string;
	private source: string;
	private unsaved: boolean;
	private unsavedListener: (isUnsaved: boolean) => void;

	constructor (title: string, owner: string, id: string, source: string) {
		this.title = title || "Untitled";
		this.owner = owner;
		this.id = id;
		this.source = source;
		this.unsaved = false;
	}

	setUnsavedListener (listener: (isUnsaved: boolean) => void) {
		this.unsavedListener = listener;
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

		if (project.id == null) {
			console.log("Creating new gist");
		} else if (project.id != null && project.owner != auth.getUsername()) {
			console.log("Forking gist");
		} else {
			console.log("Updating gist");
		}

		this.setUnsaved(false);
	}

	private setUnsaved (isUnsaved: boolean) {
		this.unsaved = isUnsaved;
		if (this.unsavedListener) this.unsavedListener(isUnsaved);
	}

	setTitle (title: string) {
		if (this.title == title.trim()) return;
		this.title = title.trim();
		this.setUnsaved(true);
	}

	getTitle () {
		return this.title;
	}

	setSource (source: string) {
		if (this.source == source) return;
		this.source = source;
		this.setUnsaved(true);
	}

	getId () {
		return this.id;
	}

	getOwner () {
		return this.owner;
	}

	getSource () {
		return this.source;
	}

	isUnsaved () {
		return this.unsaved;
	}
}