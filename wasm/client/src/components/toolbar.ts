import { auth } from "../auth";

export function createToolbar (toolbar: HTMLElement) {
	toolbar.innerHTML = toolbarHtml;
	setupUIEvents(toolbar);
}

function setupUIEvents (toolbar: HTMLElement) {
	let loginButton = toolbar.querySelector("#toolbar-login");
	let logoutButton = toolbar.querySelector("#toolbar-logout");
	let avatarImage = toolbar.querySelector("#toolbar-avatar") as HTMLImageElement;

	loginButton.addEventListener("click", () => {
		auth.login();
		updateUI();
	});
	logoutButton.addEventListener("click", () => {
		auth.logout();
		updateUI();
	});
	avatarImage.addEventListener("click", () => {
		window.location.href = auth.getAvatar();
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