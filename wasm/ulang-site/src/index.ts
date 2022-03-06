import "./ui.css"

import { Editor } from "./editor"
import { loadUlang } from "@marioslab/ulang-vm"
import Split from "split.js"

(async function () {
	await loadUlang();

	let resizeOutput = () => {
		let canvas = document.getElementById("debugger-output");
		let container = canvas.parentElement;
		// Force reflow
		canvas.style.width = 1 + "px";
		canvas.style.height = 1 + "px";
		if (container.clientWidth < container.clientHeight) {
			canvas.style.width = container.clientWidth + "px";
			canvas.style.height = container.clientWidth * (240 / 320) + "px";
		} else {
			canvas.style.width = container.clientWidth + "px";
			canvas.style.height = "auto";
			if (canvas.clientHeight > container.clientHeight) {
				canvas.style.height = container.clientHeight + "px";
				canvas.style.width = container.clientHeight * (320 / 240) + "px";
			}
		}
		if (Math.abs(container.clientWidth - canvas.clientWidth) > 2) {
			canvas.style.marginLeft = Math.abs(container.clientWidth - canvas.clientWidth) / 2 + "px";
		}
		if (Math.abs(container.clientHeight - canvas.clientHeight) > 2) {
			canvas.style.marginTop = Math.abs(container.clientHeight - canvas.clientHeight) / 2 + "px";
		}
	}
	window.addEventListener("resize", () => resizeOutput());
	requestAnimationFrame(() => resizeOutput());

	Split(['#left-col', '#right-col'], {
		minSize: 0,
		gutterSize: 5,
		onDrag: resizeOutput
	})

	Split(['#editor-container', '#help-container'], {
		direction: "vertical",
		minSize: 0,
		gutterSize: 5,
	})

	Split(['#debugger-container', '#debug-view-container'], {
		direction: "vertical",
		minSize: 0,
		gutterSize: 5,
		onDrag: resizeOutput
	})

	let editor = new Editor("editor-container");
})();