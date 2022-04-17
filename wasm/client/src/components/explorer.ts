import { Project } from "src/project";
import { Editor } from "./editor";

export class Explorer {
	private selectedFile: string;
	private selectedCallback: (filename) => void;

	constructor (private container: HTMLElement, private project: Project) {
		container.innerHTML = explorerHtml;
		let filesDom = container.querySelector(".explorer-files");
		this.renderFiles();
		this.setupUIEvents();
	}

	private renderFiles () {
		let filesDom = this.container.querySelector(".explorer-files");
		filesDom.innerHTML = "";
		for (let filename of this.project.getFilenames().sort()) {
			let fileDom = this.createFileDom(filename);
			filesDom.appendChild(fileDom);
		}
	}

	private setupUIEvents () {
		let newButton = this.container.querySelector(".icon-add-file");
		newButton.addEventListener("click", () => {

		});

		let renameButton = this.container.querySelector(".icon-rename-file");
		renameButton.addEventListener("click", () => {
			let rawFilename = this.getSelectedFile();
			let filename = rawFilename.substring(0, rawFilename.lastIndexOf("."));
			let extension = rawFilename.substring(rawFilename.lastIndexOf("."));
			let dialog = showDialog("Rename", `<div style="margin: 1em 0em"><input class="name" style="width: 100%; height: 2em;" value="${filename}"></div>`, [{
				label: "Rename",
				callback: () => {
					let nameInput = dialog.querySelector(".name") as HTMLInputElement;
					let newFilename = nameInput.value + extension;
					this.project.renameFile(this.getSelectedFile(), newFilename);
					this.selectedFile = null;
					this.renderFiles();
					this.selectFile(newFilename);
				}
			}], true);
			let nameInput = dialog.querySelector(".name") as HTMLInputElement;
			nameInput.select();
			(dialog.querySelector(".name") as HTMLInputElement).focus();
		});

		let deleteButton = this.container.querySelector(".icon-delete-file");
		deleteButton.addEventListener("click", () => {
			showDialog("Delete?", `<p>Are you sure you want to delete '${this.getSelectedFile()}'?</p>`, [{
				label: "Delete", callback: () => {
					let filename = this.getSelectedFile()
					this.project.deleteFile(filename);
					this.getFileDom(filename).remove();
					this.selectedFile = null;
					this.renderFiles();
					this.selectFile("program.ul");
				}
			}], true);
		});
	}

	private createFileDom (filename: string) {
		let fileDom: HTMLElement = document.createElement("div");
		fileDom.innerHTML = explorerFileHtml;
		fileDom = fileDom.childNodes.item(0) as HTMLElement;
		let name = fileDom.querySelector(".explorer-file-name") as HTMLElement;
		name.innerText = filename;
		(fileDom as any).filename = filename;
		fileDom.addEventListener("click", () => {
			this.selectFile(filename);
		});
		return fileDom;
	}

	private getFileDom (filename: string) {
		let doms = this.container.querySelectorAll(".explorer-file");
		for (let i = 0; i < doms.length; i++) {
			let dom = doms.item(i) as HTMLElement;
			if ((dom as any).filename == filename) return dom;
		}
		throw new Error(`Couldn't find DOM for file ${filename}`);
	}

	selectFile (filename: string) {
		if (!this.project.fileExists(filename)) {
			showDialog("Sorry", `<p>File ${filename} doesn't exist in this project.</p>`, [], true, "OK");
			return;
		}
		if (this.selectedFile) this.getFileDom(this.selectedFile).classList.remove("explorer-file-selected");
		let fileDom = this.getFileDom(filename);
		fileDom.classList.add("explorer-file-selected");
		this.selectedFile = filename;
		if (filename == "program.ul") {
			this.container.querySelector(".icon-rename-file").classList.add("explorer-button-disabled");
			this.container.querySelector(".icon-delete-file").classList.add("explorer-button-disabled");
		} else {
			this.container.querySelector(".icon-rename-file").classList.remove("explorer-button-disabled");
			this.container.querySelector(".icon-delete-file").classList.remove("explorer-button-disabled");
		}
		if (this.selectedCallback) this.selectedCallback(filename);
	}

	getSelectedFile () {
		return this.selectedFile;
	}

	setSelectedCallback (selectedCallback: (filename: string) => void) {
		this.selectedCallback = selectedCallback;
	}
}

import explorerHtml from "./explorer.html"
import explorerFileHtml from "./explorer-file.html"
import "./explorer.css"
import { showDialog } from "./dialog"; import { newFile } from "@marioslab/ulang-vm/src/wrapper";

