import { showDialog } from "./ui";

const CLIENT_ID = "98d7ee42b2a5f52f8e97";
const redirectUri = `${window.location.protocol}//${window.location.host}/api/auth_redirect`;
let accessToken = localStorage.getItem("access-token");

export async function setupAuth () {
	console.log(`Redirect URI: ${redirectUri}`);
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