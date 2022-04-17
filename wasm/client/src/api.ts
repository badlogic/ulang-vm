import axios from "axios";
import { auth } from "./auth";

export async function saveProject (id: string, title: string, screenshot: string) {
	let resp = await axios.post(`/api/${auth.getUsername()}/projects`, {
		user: auth.getUsername(),
		accessToken: auth.getAccessToken(),
		gistId: id,
		title: title,
		screenshot: screenshot
	});
	if (resp.status != 200) throw new Error("Couldn't save project");
}

export async function updateProject (id: string, title: string, screenshot: string) {
	let resp = await axios.patch(`/api/${auth.getUsername()}/projects`, {
		user: auth.getUsername(),
		accessToken: auth.getAccessToken(),
		gistId: id,
		title: title,
		screenshot: screenshot
	});
	if (resp.status != 200) throw new Error("Couldn't update project");
}