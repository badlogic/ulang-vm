import { Project } from "src/project";
import { Editor } from "./editor";

export class Explorer {
	constructor (private container: HTMLElement, private project: Project, private editor: Editor) {
		container.innerHTML = explorerHtml;
	}
}

import explorerHtml from "./explorer.html"
import explorerFileHtml from "./explorer-file.html"
import "./explorer.css"