export function showDialog (title: string, content: string, buttons: { label: string, callback: () => void }[], hasCancel: boolean, cancelLabel: string = "Cancel", closeCallback: () => void = () => { }) {
	let buttonsDom = "";

	if (hasCancel) {
		buttonsDom += `<button id="cancel" class="button" style="margin-right: 0; margin-left: 0.4em;">${cancelLabel}</button>`;
	}

	for (let i = 0; i < buttons.length; i++) {
		let button = buttons[i];
		buttonsDom += `<button id="${"button" + i}" class="button" style="margin-right: 0; margin-left: 0.4em;">${button.label}</button>`;
	}

	let dialog = document.createElement("div");
	dialog.classList.add("dialog");
	dialog.innerHTML = `
		<div class="dialog-content">
			<div class="dialog-header">${title}</div>
			${content}
			<div style="display: flex; flex-direction: row;">
				<div class="toolbar-filler"></div>
				${buttonsDom}
			</div>
		</div>
	`;
	for (let i = 0; i < buttons.length; i++) {
		let button = buttons[i];
		dialog.querySelector("#button" + i).addEventListener("click", () => {
			dialog.remove();
			button.callback()
			if (closeCallback) closeCallback();
		});
	}
	if (hasCancel) dialog.querySelector("#cancel").addEventListener("click", () => {
		dialog.remove();
		if (closeCallback) closeCallback();
	});
	document.body.appendChild(dialog);
	return dialog;
}

import "./dialog.css"