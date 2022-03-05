#!/bin/sh
set -e

if [ ! "$#" -eq 2 ]; then
	echo "Usage: ./publish.sh <last-version> <new-version>"
	exit	
else
	lastVersion=${1%/}
	newVersion=${2%/}
	echo "last version: $lastVersion"
	echo "new version: $newVersion"
fi

sed -i '' "s/$lastVersion/$newVersion/" package.json
sed -i '' "s/$lastVersion/$newVersion/" ulang-vm/package.json
sed -i '' "s/$lastVersion/$newVersion/" ulang-site/package.json

npm run build
npm publish --access public --workspaces