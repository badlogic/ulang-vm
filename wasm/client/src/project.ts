import { auth } from "./auth";
import { saveAs } from "file-saver";
import querystring from "query-string"
import { forkGist, getGist, Gist, newGist, updateGist } from "./gist";
import { showDialog } from "./components/dialog";
import axios from "axios";

export let project: Project;
let authorLabel: HTMLDivElement;
let forkedFromLabel: HTMLDivElement;
let gist: Gist = null;

export async function loadProject () {
	let dialog: HTMLElement = null;

	try {
		let authorizeProject = localStorage.getItem("authorize-project") ? JSON.parse(localStorage.getItem("authorize-project")) : null;
		let params: any = querystring.parse(location.search);
		let pathElements = location.pathname.split("/");
		pathElements.splice(0, 2);
		let gistId = null;

		if (params.id) gistId = params.id;
		if (!gistId && pathElements.length > 0) gistId = pathElements[0];

		if (!params.code && authorizeProject) {
			project = new Project(authorizeProject["title"], authorizeProject["owner"], authorizeProject["id"], authorizeProject["source"], authorizeProject["forkedFrom"]);
			project.setUnsaved(authorizeProject.unsaved);
		} else if (gistId) {
			dialog = showDialog("", "Loading Gist", [], false);
			gist = await getGist(gistId, auth.getAccessToken());
			if (gist && gist.files["source.ul"]) {
				project = Project.fromGist(gist);
			} else {
				showDialog("Sorry", "<p>This Gist could not be loaded. Either it doesn't exist, or you ran into GitHub's rate limit of 60 requests per hour for anonymous users. Login to increase that limit to 5000 requests per hour.</p>", [], true, "OK", () => {
					window.location.href = "/editor";
				});
			}
		}
	} catch (err) {
		console.log("Unexpectedly unable to load project.");
	} finally {
		if (dialog) dialog.remove();
		localStorage.removeItem("authorize-project");
	}

	if (!project) project = new Project("Untitled", null, null, "", null);
}

export interface Fork {
	id: string,
	owner: string
}

export class Project {
	private title: string;
	private titleModified: boolean;
	private owner: string;
	private id: string;
	private source: string;
	private screenshot: string;
	private unsaved: boolean;
	private unsavedListener: (isUnsaved: boolean) => void;

	constructor (title: string, owner: string, id: string, source: string, private forkedFrom: Fork) {
		this.title = title || "Untitled";
		this.titleModified = false;
		this.owner = owner;
		this.id = id;
		this.source = source;
		this.unsaved = false;
	}

	static fromGist (gist: Gist) {
		let fork = gist.fork_of ? { id: gist.fork_of.id, owner: gist.fork_of.owner.login } : null;
		return new Project(gist.description, gist.owner.login, gist.id, gist.files["source.ul"].content, fork);
	}

	fromGist (gist: Gist) {
		this.title = gist.description;
		this.titleModified = false;
		this.owner = gist.owner.login;
		this.id = gist.id;
		this.source = gist.files["source.ul"].content;
		this.unsaved = false;
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

		if (!this.isUnsaved()) return;

		if (!auth.isAuthenticated()) {
			auth.login();
			return;
		}

		let dialog = null;
		try {
			if (this.id == null) {
				dialog = showDialog("", "Creating new Gist", [], false);
				gist = await newGist(this.title, this.source, auth.getAccessToken());
				this.fromGist(gist);
				await axios.post(`/api/${auth.getUsername()}/projects`, {
					user: auth.getUsername(),
					accessToken: auth.getAccessToken(),
					gistId: gist.id,
					title: this.title,
					screenshot: this.screenshot
				});
				history.replaceState(null, "", `/editor/${this.id}`);
			} else if (this.id != null && this.owner != auth.getUsername()) {
				let forkedId: string = null;
				for (let i = 0; i < gist.forks.length; i++) {
					if (gist.forks[i].user.login == auth.getUsername()) {
						forkedId = gist.forks[i].id;
					}
				}

				if (forkedId) {
					showDialog("Update fork?", "<p>You have already forked this Gist. Do you want to update it?</p>", [{
						label: "Yes", callback: async () => {
							dialog = showDialog("", "Updating forked Gist", [], false);
							try {
								await updateGist(forkedId, this.title, this.source, auth.getAccessToken());
								await axios.patch(`/api/${auth.getUsername()}/projects`, {
									user: auth.getUsername(),
									accessToken: auth.getAccessToken(),
									gistId: forkedId,
									title: this.title,
									screenshot: this.screenshot
								});
								this.setUnsaved(false);
								location.href = `/editor/${forkedId}`;
							} catch (e) {
								if (dialog) dialog.remove();
								dialog = showDialog("Sorry", `<p>Couldn't create or save your Gist.</p><p>${e.toString()}</p>`, [], true, "OK");
								return;
							} finally {
								if (dialog) dialog.remove();
							}
						}
					}], true);
				} else {
					showDialog("Fork?", "<p>This Gist belongs to another user. Do you want to fork it?</p>", [{
						label: "OK", callback: async () => {
							dialog = showDialog("", "Forking Gist", [], false);
							try {
								gist = await forkGist(this.id, auth.getAccessToken());
								let source = this.getSource();
								this.fromGist(gist);
								this.setSource(source);
								await updateGist(this.id, this.title, this.source, auth.getAccessToken());
								try {
									await axios.post(`/api/${auth.getUsername()}/projects`, {
										user: auth.getUsername(),
										accessToken: auth.getAccessToken(),
										gistId: gist.id,
										title: this.title,
										screenshot: this.screenshot
									});
								} catch (e) {
									await axios.patch(`/api/${auth.getUsername()}/projects`, {
										user: auth.getUsername(),
										accessToken: auth.getAccessToken(),
										gistId: gist.id,
										title: this.title,
										screenshot: this.screenshot
									});
								}
								authorLabel.innerHTML = "";
								this.setUnsaved(false);
								history.replaceState(null, "", `/editor/${this.id}`);
							} catch (e) {
								if (dialog) dialog.remove();
								dialog = showDialog("Sorry", `<p>Couldn't create or save your Gist.</p><p>${e.toString()}</p>`, [], true, "OK");
								return;
							} finally {
								if (dialog) dialog.remove();
							}
						}
					}], true);
				}
				return;
			} else {
				dialog = showDialog("", "Saving Gist", [], false);
				await updateGist(this.id, this.title, this.source, auth.getAccessToken());
				try {
					await axios.patch(`/api/${auth.getUsername()}/projects`, {
						user: auth.getUsername(),
						accessToken: auth.getAccessToken(),
						gistId: this.id,
						title: this.title,
						screenshot: this.screenshot
					});
				} catch (e) {
					await axios.post(`/api/${auth.getUsername()}/projects`, {
						user: auth.getUsername(),
						accessToken: auth.getAccessToken(),
						gistId: gist.id,
						title: this.title,
						screenshot: this.screenshot
					});
				}
			}
		} catch (e) {
			if (dialog) dialog.remove();
			showDialog("Sorry", `<p>Couldn't create or save your Gist.</p><p>${e.toString()}</p>`, [], true, "OK");
			return;
		} finally {
			if (dialog) dialog.remove();
			this.screenshot = null;
		}

		this.setUnsaved(false);
	}

	setUnsaved (isUnsaved: boolean) {
		this.unsaved = isUnsaved;
		if (!isUnsaved) this.titleModified = false;
		if (this.unsavedListener) this.unsavedListener(isUnsaved);
	}

	setTitle (title: string) {
		if (this.title == title.trim()) return;
		this.titleModified = true;
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

	getForkedFrom () {
		return this.forkedFrom;
	}

	setScreenshot (screenshot: string) {
		this.screenshot = screenshot;
		this.setUnsaved(true);
	}
}