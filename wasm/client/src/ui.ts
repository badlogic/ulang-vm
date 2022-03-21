export function showDialog (title: string, content: string, buttons: { label: string, callback: () => void }[], hasCancel: boolean, cancelLabel: string = "Cancel") {
	let buttonsDom = "";
	for (let i = 0; i < buttons.length; i++) {
		let button = buttons[i];
		buttonsDom += `<button id="${"button" + i}" class="dialog-button">${button.label}</button>`;
	}
	if (hasCancel) {
		buttonsDom += `<button id="cancel" class="dialog-button">Cancel</button>`;
	}

	let dialog = document.createElement("div");
	dialog.classList.add("dialog");
	dialog.innerHTML = `
		<div class="dialog-content">
			<div class="dialog-header">${title}</div>
			${content}			
			${buttonsDom}
		</div>
	`;
	for (let i = 0; i < buttons.length; i++) {
		let button = buttons[i];
		dialog.querySelector("#button" + i).addEventListener("click", () => {
			dialog.remove();
			button.callback()
		});
	}
	if (hasCancel) dialog.querySelector("#cancel").addEventListener("click", () => { dialog.remove() });
	document.body.appendChild(dialog);
	return dialog;
}
