import { Express, Response } from "express";
import * as fs from "fs";
import * as path from "path";
import chokidar from "chokidar"

var lastChangeTimestamp = 0;

export function setupLiveEdit (app: Express, assetPath: string) {
	let p = path.join(__dirname, assetPath);
	chokidar.watch(p).on('all', () => {
		lastChangeTimestamp = Date.now()
		console.log("Files changed");
	});

	var reloadScript = `
	<script>
	(function(){
	var lastChangeTimestamp = null;
	setInterval(() => {
		fetch("/live-edit")
			.then(response => response.text())
			.then(timestamp => {
				if (lastChangeTimestamp == null) {
					lastChangeTimestamp = timestamp;
				} else if (lastChangeTimestamp != timestamp) {
					location.reload();
				}
			});
	}, 500);
	})();
	</script>
	`;

	let sendFile = (filename: string, res: Response<any, Record<string, any>>) => {
		fs.readFile(path.join(__dirname, assetPath, filename), function (err, data) {
			if (err) {
				res.sendStatus(404);
			} else {
				res.setHeader("Content-Type", "text/html; charset=UTF-8");
				res.send(Buffer.concat([data, Buffer.from(reloadScript)]));
			}
		});
	};

	app.get("/", (req, res, next) => sendFile("index.html", res));
	app.get("/*.html", (req, res, next) => sendFile(req.path, res));
	app.get("/live-edit", (req, res) => res.send(`${lastChangeTimestamp}`));
}