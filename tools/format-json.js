#!/usr/bin/env node
const fs = require("fs").promises;

const fileList = [
  `${__dirname}/../data/resources/settings.json`, `${__dirname}/../data/resources/data/AudioConfig.json`,
  `${__dirname}/../data/resources/data/TerrainGen.json`, `${__dirname}/../data/resources/data/TileData.json`,
  `${__dirname}/../data/resources/data/UIData.json`, `${__dirname}/../data/resources/data/UILayout.json`
];

async function formatFile(file)
{
  const content = await fs.readFile(file, "utf8");
  await fs.writeFile(file, JSON.stringify(JSON.parse(content), null, 4));
  console.log(`formatting file ${file} done`);
}

Promise.all(fileList.map(formatFile))
  .then(() => console.log("All files formatted successfully"))
  .catch((err) => {
    console.error("Error formatting files:", err);
    process.exit(1);
  });
