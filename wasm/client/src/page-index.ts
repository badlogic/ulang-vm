import "./common.css"
import "./page-index.css"

import { loadUlang } from "@marioslab/ulang-vm";
import { Auth, checkAuthorizationCode } from "./auth";
import { setupLiveEdit } from "./liveedit";

(async () => {
	await checkAuthorizationCode();
	await loadUlang();

	new Auth("toolbar-login", "toolbar-logout", "toolbar-avatar");

	setupLiveEdit();
	setupUIEvents();

	(document.getElementsByClassName("main")[0] as HTMLElement).style.display = "flex";
})();

function setupUIEvents () {
	document.getElementById("start-building").addEventListener("click", () => {
		window.location.href = "/editor.html";
	});
}