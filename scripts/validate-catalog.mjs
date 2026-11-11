import { read, sync, root } from "./sync-packages.mjs";
import { existsSync, readdirSync, lstatSync } from "node:fs";
import { resolve } from "node:path";
const catalog = read("catalog.json"),
  ids = new Set(),
  paths = new Set(),
  folders = new Set();
if (
  catalog.schema !== "xenon.module-catalog" ||
  catalog.version !== 2 ||
  !Array.isArray(catalog.modules) ||
  catalog.modules.length > 256
)
  throw Error("Invalid catalogue");
for (const path of catalog.modules) {
  if (
    !/^games\/[a-z0-9][a-z0-9-]*\/[a-z0-9][a-z0-9._-]*\/entry\.json$/.test(
      path,
    ) ||
    path.includes("..") ||
    paths.has(path)
  )
    throw Error("Invalid catalogue path: " + path);
  paths.add(path);
  const entry = read(path);
  if (
    entry.schema !== "xenon.module-entry" ||
    entry.version !== 2 ||
    entry.type !== "game" ||
    ids.has(entry.id) ||
    !/^([a-z0-9]+[._-])+[a-z0-9]+$/.test(entry.id) ||
    entry.id.length > 128
  )
    throw Error("Invalid module identity");
  ids.add(entry.id);
  if (
    path !== entry.sourcePath + "/entry.json" ||
    entry.repository.provider !== "github" ||
    entry.repository.owner !== "nimauria" ||
    entry.repository.name !== "Xenon-Modules"
  )
    throw Error("Module source must be in Xenon-Modules");
  if (
    !entry.platforms.length ||
    entry.platforms.some((p) => !/^[a-z0-9]+-[a-z0-9_]+$/.test(p))
  )
    throw Error("Invalid platforms");
  for (const kind of ["tileArt", "heroArt"]) {
    const art = entry.launcher[kind];
    if (
      !art.startsWith(entry.sourcePath + "/assets/") ||
      art.includes("..") ||
      !existsSync(resolve(root, art))
    )
      throw Error("Missing module artwork");
  }
}
for (const owner of readdirSync(resolve(root, "games"), {
  withFileTypes: true,
})) {
  if (!owner.isDirectory() || owner.isSymbolicLink())
    throw Error("Invalid contributor folder");
  for (const game of readdirSync(resolve(root, "games", owner.name), {
    withFileTypes: true,
  })) {
    if (!game.isDirectory() || game.isSymbolicLink())
      throw Error("Invalid game folder");
    folders.add(`games/${owner.name}/${game.name}/entry.json`);
  }
}
if (folders.size !== paths.size || [...folders].some((p) => !paths.has(p)))
  throw Error("Every game folder must be listed in catalog.json");
sync(true);
console.log(`Validated ${ids.size} modules and their repository files.`);
