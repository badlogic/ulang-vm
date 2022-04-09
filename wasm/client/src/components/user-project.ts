import { decode } from "html-entities";

export interface UserProject {
	gistid: string,
	owner: string,
	title: string,
	created: number,
	modified: number
}

export function createUserProjectDom (project: UserProject) {
	let temp = document.createElement("div");
	temp.innerHTML = userProjectHtml;
	let projectDom = temp.children[0];

	let url = `/editor/${project.gistid}`;

	let thumb = projectDom.querySelector(".user-project-thumb") as HTMLImageElement;
	thumb.src = `/images/${project.gistid}.png`;
	let thumbLink = projectDom.querySelector(".user-project-thumb-link") as HTMLAnchorElement;
	thumbLink.href = url;

	let title = projectDom.querySelector(".user-project-title") as HTMLAnchorElement;
	title.textContent = decode(project.title);
	title.href = `/editor/${project.gistid}`;

	let created = projectDom.querySelector(".user-project-created") as HTMLDivElement;
	created.textContent = `Created: ${new Date(project.created).toDateString()} ${new Date(project.created).toLocaleTimeString()}`;
	return projectDom;
}

import userProjectHtml from "./user-project.html"
import "./user-project.css"

