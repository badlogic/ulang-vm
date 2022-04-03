import "./common.css"
import "./page-index.css"

import { createToolbar } from "./components/toolbar"
import { createPlayerFromGist, loadUlang, UlangPlayer } from "@marioslab/ulang-vm";
import { Auth, checkAuthorizationCode } from "./auth";
import { setupLiveEdit } from "./liveedit";

(async () => {
	await checkAuthorizationCode();
	await loadUlang();

	new Auth();
	createToolbar(document.querySelector(".toolbar"));

	setupLiveEdit();
	setupUIEvents();

	let player = await createPlayerFromGist("demo-canvas", "a3929e69602cb54d64ffc2c3f638208b");
	player.play();

	(document.getElementsByClassName("main")[0] as HTMLElement).style.display = "flex";
})();

function setupUIEvents () {
	document.getElementById("start-building").addEventListener("click", () => {
		window.location.href = "/editor";
	});
}