import { auth } from "../auth";

export function createToolbar (toolbar: HTMLElement, includeEditorButtons: boolean = false) {
	toolbar.innerHTML = toolbarHtml;
	if (includeEditorButtons) toolbar.querySelector(".toolbar-editor").classList.remove("hide");
	setupUIEvents(toolbar);
	return toolbar;
}

function setupUIEvents (toolbar: HTMLElement) {
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

import toolbarHtml from "./toolbar.html"
import "./toolbar.css"