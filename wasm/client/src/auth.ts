import { showDialog } from "./ui";

const isDev = window.location.host == "localhost:8080";
const CLIENT_ID = isDev ? "5d70175768102ff08ee4" : "98d7ee42b2a5f52f8e97";
const redirectUri = isDev ? "http://localhost:8080/ulang-site/assets/index.html" : "https://marioslab.io/ulang";
let accessToken = localStorage.getItem("access-token");

export async function setupAuth () {
	if (isAuthenticated()) return;
}

export function isAuthenticated () {
	return accessToken != null;
}

export function authenticate () {
	if (isAuthenticated()) return;

	showDialog("Login",
		`<p>
			To save and share your programs, you must login to
	 		<a href="https://github.com/">GitHub</a>. Your programs 
	 		will be stored as public Gists in your GitHub account.
	 	</p>`,
		[{ label: "Login to GitHub", callback: () => { startLoginFlow() } }], true);
}

/** Adapted from  https://github.com/Zemnmez/react-oauth2-hook */
const cryptoRandomString = () => {
	const entropy = new Uint32Array(10)
	window.crypto.getRandomValues(entropy)
	return window.btoa([...entropy].join(','))
}

function startLoginFlow () {
}