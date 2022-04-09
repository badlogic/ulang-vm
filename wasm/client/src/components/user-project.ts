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
	let projectDom = temp.children[0] as HTMLAnchorElement;

	let title = projectDom.querySelector(".user-project-title");
	title.textContent = decode(project.title);

	projectDom.href = `/editor/${project.gistid}`;
	return projectDom;
}

import userProjectHtml from "./user-project.html"
import "./user-project.css"

