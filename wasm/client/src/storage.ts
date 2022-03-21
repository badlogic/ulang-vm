import { Editor } from "./editor";
import { saveAs } from "file-saver";
import fireExample from "../../../tests/fire.ul";
import { auth } from "./auth"

export function setupStorage (editor: Editor) {
	let download = document.getElementById("toolbar-download");
	download.addEventListener("click", () => {
		var file = new File([editor.getContent()], "source.ul", { type: "text/plain;charset=utf-8" });
		saveAs(file);
	});
	let save = document.getElementById("toolbar-save");
	save.addEventListener("click", () => {
		if (!auth.isAuthenticated()) {
			auth.login();
			return;
		}
	});

	editor.setContentListener((content) => {
		localStorage.setItem("source", content);
	})
	let lastSource = localStorage.getItem("source");
	if (lastSource != null) { editor.setContent(lastSource); }
	else editor.setContent(fireExample);
}