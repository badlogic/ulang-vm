import { auth } from "../auth";
import { Fork } from "../project";

export class Toolbar {
	constructor (private container: HTMLElement, includeEditorButtons: boolean = false) {
		container.innerHTML = toolbarHtml;
		if (includeEditorButtons) container.querySelector(".toolbar-editor").classList.remove("hide");
		this.setupUIEvents();
	}

	private setupUIEvents () {
		let toolbar = this.container;
		let loginButton = toolbar.querySelector(".toolbar-login");
		let logoutButton = toolbar.querySelector(".toolbar-logout");
		let avatarImage = toolbar.querySelector(".toolbar-avatar") as HTMLImageElement;

		loginButton.addEventListener("click", () => {
			auth.login();
			updateUI();
		});
		logoutButton.addEventListener("click", () => {
			auth.logout();
			updateUI();
		});
		avatarImage.addEventListener("click", () => {
			window.location.href = `/user/${auth.getUsername()}`;
		});

		let updateUI = () => {
			if (auth.isAuthenticated()) {
				loginButton.classList.add("hide");
				logoutButton.classList.remove("hide");
				avatarImage.classList.remove("hide");
				avatarImage.src = auth.getAvatar();
			} else {
				loginButton.classList.remove("hide");
				logoutButton.classList.add("hide");
				avatarImage.classList.add("hide");
			}
		}
		updateUI();
	}

	getRunButton () {
		return this.container.querySelector(".toolbar-run") as HTMLElement;
	}

	getContinueButton () {
		return this.container.querySelector(".toolbar-continue") as HTMLElement;
	}

	getPauseButton () {
		return this.container.querySelector(".toolbar-pause") as HTMLElement;
	}

	getStepButton () {
		return this.container.querySelector(".toolbar-step") as HTMLElement;
	}

	getStopButton () {
		return this.container.querySelector(".toolbar-stop") as HTMLElement;
	}

	setButtonStates (state: VirtualMachineState) {
		this.getRunButton().style.display = "";
		this.getContinueButton().style.display = "none";
		this.getContinueButton().classList.add("toolbar-button-disabled");
		this.getPauseButton().classList.add("toolbar-button-disabled");
		this.getStepButton().classList.add("toolbar-button-disabled");
		this.getStopButton().classList.add("toolbar-button-disabled");

		switch (state) {
			case VirtualMachineState.Running:
				this.getRunButton().style.display = "none";
				this.getContinueButton().style.display = "";
				this.getContinueButton().classList.add("toolbar-button-disabled");
				this.getPauseButton().classList.remove("toolbar-button-disabled");
				this.getStepButton().classList.add("toolbar-button-disabled");
				this.getStopButton().classList.remove("toolbar-button-disabled");
				break;
			case VirtualMachineState.Paused:
				this.getRunButton().style.display = "none";
				this.getContinueButton().style.display = "";
				this.getContinueButton().classList.remove("toolbar-button-disabled");
				this.getPauseButton().classList.add("toolbar-button-disabled");
				this.getStepButton().classList.remove("toolbar-button-disabled");
				this.getStopButton().classList.remove("toolbar-button-disabled");
				break;
		}
	}

	getTitleInput () {
		return this.container.querySelector(".toolbar-title") as HTMLInputElement;
	}

	setAuthor (author: string) {
		if (author == null || author.trim().length == 0) (this.container.querySelector(".toolbar-author") as HTMLDivElement).innerHTML = "";
		else (this.container.querySelector(".toolbar-author") as HTMLDivElement).innerHTML = `<span style="margin-right: 0.5em">by</span><a href="https://github.com/${author}">${author}</a>`;
	}

	setForkedFrom (fork: Fork) {
		if (fork == null) (this.container.querySelector(".toolbar-author") as HTMLDivElement).innerHTML = "";
		else (this.container.querySelector(".toolbar-forked-from") as HTMLDivElement).innerHTML = `<span style="margin-right: 0.5em">forked from</span><a href="/editor/${fork.id}">${fork.owner}</a>`;
	}

	getUnsavedLabel () {
		return this.container.querySelector(".toolbar-unsaved") as HTMLDivElement;
	}

	getNewButton () {
		return this.container.querySelector(".toolbar-new");
	}

	getSaveButton () {
		return this.container.querySelector(".toolbar-save");
	}

	getScreenshotButton () {
		return this.container.querySelector(".toolbar-screenshot");
	}

	getDownloadButton () {
		return this.container.querySelector(".toolbar-download");
	}
}

export function createToolbar () {

}

import toolbarHtml from "./toolbar.html"
import "./toolbar.css"
import { VirtualMachineState } from "@marioslab/ulang-vm";
