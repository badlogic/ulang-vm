import { auth } from "./auth";
import { saveAs } from "file-saver";
import { Editor } from "./editor";
import querystring from "query-string"
import { getGist, Gist, newGist, updateGist } from "./gist";
import { showDialog } from "./ui";

export let project: Project;

export async function loadProject (editor: Editor, _titleLabel: string, _authorLabel: string) {
	let authorizeProject = JSON.parse(localStorage.getItem("authorize-project"));
	let params: any = querystring.parse(location.search);

	if (!params.code && authorizeProject) {
		project = new Project("", authorizeProject["owner"], authorizeProject["id"], authorizeProject["source"]);
		project.setTitle(authorizeProject["title"]);
		localStorage.removeItem("authorize-project");
	} else if (params && params.id) {
		let gist = await getGist(params.id, auth.getAccessToken());
		if (gist && gist.files["source.ul"]) {
			project = Project.fromGist(gist);
		} else {
			showDialog("Sorry", "<p>This Gist could not be loaded. Either it doesn't exist, or you ran into GitHub's rate limit of 60 requests per hour for anonymous users. Login to get increase that limit to 5000 requests per hour.</p>", [], true, "OK", () => {
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

	static fromGist (gist: Gist) {
		return new Project(gist.description, gist.owner.login, gist.id, gist.files["source.ul"].content);
	}

	fromGist (gist: Gist) {
		this.title = gist.description;
		this.owner = gist.owner.login;
		this.id = gist.id;
		this.source = gist.files["source.ul"].content;
	}

	setUnsavedListener (listener: (isUnsaved: boolean) => void) {
		this.unsavedListener = listener;
	}

	download (): any {
		var file = new File([this.source], "source.ul", { type: "text/plain;charset=utf-8" });
		saveAs(file);
	}

	async save () {
		if (this.source.trim().length == 0) {
			showDialog("Sorry", "<p>Can't save an empty program.</p>", [], true, "OK");
			return;
		}

		if (!auth.isAuthenticated()) {
			auth.login();
			return;
		}

		let dialog = null;
		try {
			if (this.id == null) {
				dialog = showDialog("", "Creating new Gist", [], false);
				let gist = await newGist(this.title, this.source, auth.getAccessToken());
				this.fromGist(gist);
				history.replaceState(null, "", `/?id=${this.id}`);
			} else if (this.id != null && this.owner != auth.getUsername()) {
				console.log("Forking gist");
			} else {
				dialog = showDialog("", "Saving Gist", [], false);
				await updateGist(this.id, this.title, this.source, auth.getAccessToken());
			}
		} catch (e) {
			if (dialog) dialog.remove();
			showDialog("Sorry", `<p>Couldn't create or save your Gist.</p><p>${e.toString()}</p>`, [], true, "OK");
			return;
		} finally {
			if (dialog) dialog.remove();
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