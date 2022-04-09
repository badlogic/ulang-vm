import "./common.css"
import "./page-user.css"

import { createToolbar } from "./components/toolbar"
import { loadUlang } from "@marioslab/ulang-vm";
import { auth, checkAuthorizationCode } from "./auth";
import { setupLiveEdit } from "./liveedit";
import { showDialog } from "./components/dialog";
import { createUserProjectDom, UserProject } from "./components/user-project"
import axios from "axios";

(async () => {
	await checkAuthorizationCode();
	await loadUlang();

	createToolbar(document.querySelector(".toolbar"));
	setupLiveEdit();
	await loadUser();

	(document.querySelector(".main") as HTMLElement).style.display = "flex";
})();

async function loadUser () {
	let pathElements = location.pathname.split("/");
	pathElements.splice(0, 2);
	if (pathElements.length == 0) {
		showDialog("Sorry", `<p>User doesn't exist.</p>`, [], true, "OK", () => location.href = "/");
		return;
	}

	let userName = pathElements[0];
	let headers = auth.getAccessToken() ? { headers: { "Authorization": `token ${auth.getAccessToken()}` } } : null;
	let user = await axios.get(`https://api.github.com/users/${userName}?ts=${performance.now()}`, headers);
	if (user.status != 200) {
		showDialog("Sorry", `<p>User ${userName} doesn't exist.</p>`, [], true, "OK", () => location.href = "/");
		return;
	}

	let projects = await axios.get(`/api/${userName}/projects`);
	if (projects.status != 200) {
		showDialog("Sorry", `<p>User ${userName} doesn't exist.</p>`, [], true, "OK", () => location.href = "/");
		return;
	};

	let userProjectsDom = document.querySelector(".user-projects") as HTMLDivElement;
	for (let project of projects.data) {
		userProjectsDom.appendChild(createUserProjectDom(project as UserProject));
	}

	let userNameDom = document.querySelector(".user-name") as HTMLAnchorElement;
	userNameDom.textContent = userName;
	userNameDom.href = `https://github.com/${userName}`;

	let userAvatarDom = document.querySelector(".user-avatar") as HTMLImageElement;
	userAvatarDom.src = user.data.avatar_url;
}
