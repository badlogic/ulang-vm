import "./common.css"
import "./page-index.css"

import { Toolbar } from "./components/toolbar"
import { createPlayerFromGist, loadUlang } from "@marioslab/ulang-vm";
import { checkAuthorizationCode } from "./auth";
import { setupLiveEdit } from "./liveedit";

(async () => {
	if (await checkAuthorizationCode()) return;
	await loadUlang();

	new Toolbar(document.querySelector(".toolbar"));
	setupLiveEdit();
	setupUIEvents();

	let player = await createPlayerFromGist(document.querySelector("#demo-canvas"), "a3929e69602cb54d64ffc2c3f638208b");
	player.play();

	(document.querySelector(".main") as HTMLElement).style.display = "flex";
})();

function setupUIEvents () {
	document.getElementById("start-building").addEventListener("click", () => {
		window.location.href = "/editor";
	});
}