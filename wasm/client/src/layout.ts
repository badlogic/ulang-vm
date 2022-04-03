import Split from "split.js"

export function setupLayout () {
	let resizeOutput = () => {
		let canvas = document.getElementById("debugger-output");
		let container = canvas.parentElement;
		// Force reflow
		canvas.style.width = 1 + "px";
		canvas.style.height = 1 + "px";
		let padding = 10;
		if (container.clientWidth < container.clientHeight) {
			canvas.style.width = (container.clientWidth - padding) + "px";
			canvas.style.height = (container.clientWidth - padding) * (240 / 320) + "px";
		} else {
			canvas.style.width = (container.clientWidth - padding) + "px";
			canvas.style.height = "auto";
			if (canvas.clientHeight > container.clientHeight) {
				canvas.style.height = (container.clientHeight - padding) + "px";
				canvas.style.width = (container.clientHeight - padding) * (320 / 240) + "px";
			}
		}
		if (container.clientWidth > canvas.clientWidth) {
			canvas.style.marginLeft = Math.abs(container.clientWidth - canvas.clientWidth) / 2 + "px";
		}
		if (container.clientHeight - canvas.clientHeight) {
			canvas.style.marginTop = Math.abs(container.clientHeight - canvas.clientHeight - padding) / 2 + "px";
		}
	}
	window.addEventListener("resize", () => resizeOutput());
	requestAnimationFrame(() => resizeOutput());

	let sizes = {
		outer: [50, 50],
		left: [50, 50],
		right: [50, 50],
		debug: [34, 33, 33]
	};
	let sizesJSON = localStorage.getItem("split-sizes");
	if (sizesJSON) sizes = JSON.parse(sizesJSON);

	let saveSplitSizes = () => {
		sizes.outer = splitOuter.getSizes();
		sizes.left = splitLeft.getSizes();
		sizes.right = splitRight.getSizes();
		sizes.debug = splitDebug.getSizes();
		localStorage.setItem("split-sizes", JSON.stringify(sizes));
	};

	let splitOuter = Split(['#left-col', '#right-col'], {
		minSize: 0,
		gutterSize: 5,
		sizes: sizes.outer,
		onDrag: resizeOutput,
		onDragEnd: saveSplitSizes
	});

	let splitLeft = Split(['#editor-container', '#help-container'], {
		direction: "vertical",
		minSize: 0,
		gutterSize: 5,
		sizes: sizes.left,
		onDragEnd: saveSplitSizes
	});

	let splitRight = Split(['#debugger-container', '#debug-view-container'], {
		direction: "vertical",
		minSize: 0,
		gutterSize: 5,
		sizes: sizes.right,
		onDrag: resizeOutput,
		onDragEnd: saveSplitSizes
	});

	let splitDebug = Split([".debug-view-registers", ".debug-view-stack", ".debug-view-memory"], {
		direction: "horizontal",
		minSize: 0,
		gutterSize: 5,
		sizes: sizes.debug,
		onDragEnd: saveSplitSizes
	});
}
