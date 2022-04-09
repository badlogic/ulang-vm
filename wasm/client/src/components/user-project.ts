import { decode } from "html-entities";
import axios from "axios";
import { auth } from "src/auth";

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
	thumb.onerror = () => {
		thumb.style.display = "none";
	}
	thumb.src = `/images/${project.gistid}.png`;
	let thumbLink = projectDom.querySelector(".user-project-thumb-link") as HTMLAnchorElement;
	thumbLink.href = url;

	let title = projectDom.querySelector(".user-project-title") as HTMLAnchorElement;
	title.textContent = decode(project.title);
	title.href = `/editor/${project.gistid}`;

	let created = projectDom.querySelector(".user-project-created") as HTMLDivElement;
	created.textContent = `Created: ${new Date(project.created).toDateString()} ${new Date(project.created).toLocaleTimeString()}`;

	let deleteButton = projectDom.querySelector(".user-project-delete") as HTMLButtonElement;
	deleteButton.addEventListener("click", async () => {
		await axios.delete(`/api/${auth.getUsername()}/projects`, {
			headers: {},
			data: {
				user: auth.getUsername(),
				accessToken: auth.getAccessToken(),
				gistId: project.gistid,
			}
		});
		projectDom.remove();
	});

	return projectDom;
}

import userProjectHtml from "./user-project.html"
import "./user-project.css"

