import { showDialog } from "./ui";
import axios from "axios"
import querystring from "query-string"
import { project } from "./project";

const CLIENT_ID = window.location.host.indexOf("localhost") >= 0 ? "fe384d3b8df3158bc8ec" /*DEV*/ : "98d7ee42b2a5f52f8e97" /*PROD*/;

export let auth: Auth;

export class User {
	constructor (public accessToken: string, public user: { login: string, avatar_url: string, html_url: string }) { }
}

export async function checkAuthorizationCode () {
	let authorizeState = localStorage.getItem("authorize-state");
	let finalUrl = localStorage.getItem("authorize-final-url");
	localStorage.removeItem("authorize-state");
	localStorage.removeItem("authorize-final-url");
	if (!authorizeState) return;
	if (!finalUrl) {
		showDialog("Sorry", "<p>Couldn't log you in. No redirect URL was set.</p>", [], true, "OK");
		return;
	}

	try {
		let params = querystring.parse(location.search);
		if (!params.code || !params.state) {
			showDialog("Sorry", "<p>Couldn't log you in. GitHub didn't return an authorization token.</p>", [], true, "OK");
			return;
		}
		if (!params.state || params.state != authorizeState) throw new Error("State does not match");
		let userResponse = await axios.post("/api/access_token", { code: params.code });
		localStorage.setItem("user", JSON.stringify(userResponse.data));
		window.location.href = finalUrl;
	} catch (e) {
		console.log("Error: " + JSON.stringify(e));
		showDialog("Sorry", "<p>Couldn't log you in. GitHub didn't return an authorization token.</p>", [], true, "OK");
	}
}

export class Auth {
	private loginButton: HTMLElement;
	private logoutButton: HTMLElement;
	private avatarImage: HTMLImageElement;
	private user: User;

	constructor (
		loginButton: HTMLElement | string,
		logoutButton: HTMLElement | string,
		avatarImage: HTMLImageElement | string) {
		auth = this;
		this.loginButton = typeof (loginButton) === "string" ? document.getElementById(loginButton) : loginButton;
		this.logoutButton = typeof (logoutButton) === "string" ? document.getElementById(logoutButton) : logoutButton;
		this.avatarImage = typeof (avatarImage) === "string" ? document.getElementById(avatarImage) as HTMLImageElement : avatarImage;

		const userJson = localStorage.getItem("user");
		if (userJson) {
			this.user = JSON.parse(userJson);
			this.avatarImage.src = this.user.user.avatar_url;
		}

		this.loginButton.addEventListener("click", () => {
			this.login();
		});
		this.logoutButton.addEventListener("click", () => {
			this.logout();
		});
		this.avatarImage.addEventListener("click", () => {
			window.location.href = this.user.user.html_url;
		});

		this.updateUI();
	}

	private updateUI () {
		if (this.user) {
			this.loginButton.classList.add("hide");
			this.logoutButton.classList.remove("hide");
			this.avatarImage.classList.remove("hide");
		} else {
			this.loginButton.classList.remove("hide");
			this.logoutButton.classList.add("hide");
			this.avatarImage.classList.add("hide");
		}
	}

	isAuthenticated () {
		return this.user && this.user.accessToken;
	}

	getUsername () {
		return this.user?.user?.login;
	}

	getAvatar () {
		return this.user?.user?.avatar_url;
	}

	getAccessToken () {
		return this.user?.accessToken;
	}

	login () {
		if (this.isAuthenticated()) return;

		showDialog("Login",
			`<p>
				To save and share your programs on the web, you must login to
					<a href="https://github.com/">GitHub</a>. Your programs 
					will be stored as public Gists in your GitHub account.
			  </p>
			  <p>
			  	Alternatively, you can save the sources of your programs to local files using the
				  <span class="icon icon-download"></span> button.
			  </p>`,
			[{ label: "Login to GitHub", callback: () => { this.startLoginFlow() } }], true);
	}

	logout () {
		this.user = null;
		localStorage.removeItem("user");
		this.updateUI();
		location.reload();
	}

	private startLoginFlow () {
		const authorizeState = cryptoRandomString();
		localStorage.setItem("authorize-state", authorizeState);
		localStorage.setItem("authorize-final-url", window.location.href);
		localStorage.setItem("authorize-project", JSON.stringify(project));
		const redirectUri = window.location.href;
		window.location.href = `https://github.com/login/oauth/authorize?client_id=${CLIENT_ID}&redirect_uri=${encodeURIComponent(redirectUri)}&scope=gist&state=${encodeURIComponent(authorizeState)}&allow_signup=true}`;
	}
}

const cryptoRandomString = () => {
	const entropy = new Uint32Array(10)
	window.crypto.getRandomValues(entropy)
	return window.btoa([...entropy].join(','))
}