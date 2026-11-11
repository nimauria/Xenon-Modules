import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { resolve, basename, extname, dirname } from "node:path";
import { inside, read, hash, root, cli, write } from "./repository.mjs";
import { sync } from "./sync-packages.mjs";
export function importArtwork({
  base = root,
  game,
  file,
  role,
  local = false,
  creator,
  source,
  license,
  basis,
  evidence,
  attribution = "",
}) {
  const entry = read(game + "/entry.json", base);
  if (entry.sourcePath !== game || !["cover", "hero"].includes(role))
    throw Error("Choose a catalogue game and cover/hero role");
  const extension = extname(file).toLowerCase();
  if (![".png", ".jpg", ".jpeg", ".webp"].includes(extension))
    throw Error("Use PNG, JPEG or WebP artwork");
  const bytes = readFileSync(file);
  if (bytes.length === 0 || bytes.length > 8 * 1024 * 1024)
    throw Error("Artwork must be between 1 byte and 8 MiB");
  const png = bytes
    .subarray(0, 8)
    .equals(Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]));
  const jpeg = bytes[0] === 255 && bytes[1] === 216 && bytes[2] === 255;
  const webp =
    bytes.toString("ascii", 0, 4) === "RIFF" &&
    bytes.toString("ascii", 8, 12) === "WEBP";
  if (!(
    (extension === ".png" && png) ||
    ([".jpg", ".jpeg"].includes(extension) && jpeg) ||
    (extension === ".webp" && webp)
  ))
    throw Error("Artwork bytes do not match the declared image format");
  const name = role + extension,
    path = "assets/" + name;
  if (local) {
    const localRoot = resolve(base, ".local");
    const target = write(
      localRoot,
      "artwork/" + game.substring("games/".length) + "/" + name,
      bytes,
    );
    return { local: true, path: target };
  }
  if (
    !["original", "licensed", "permission"].includes(basis) ||
    [creator, source, license, evidence].some(
      (value) => typeof value !== "string" || !value.trim(),
    )
  )
    throw Error(
      "Public artwork needs creator, source, licence, basis and permission evidence",
    );
  const folder = inside(base, game),
    manifestPath = inside(folder, "artwork.json");
  const manifest = existsSync(manifestPath)
    ? read("artwork.json", folder)
    : { schema: "xenon.artwork", version: 1, assets: [] };
  const record = {
    path,
    role,
    creator,
    source,
    license,
    basis,
    evidence,
    attribution,
    redistribution: "permitted",
    sha256: hash(bytes),
  };
  manifest.assets = manifest.assets.filter((item) => item.path !== path);
  manifest.assets.push(record);
  const target = inside(folder, path);
  write(base, game + "/" + path, bytes);
  write(base, game + "/artwork.json", JSON.stringify(manifest, null, 2) + "\n");
  entry.launcher[role === "cover" ? "tileArt" : "heroArt"] = game + "/" + path;
  write(base, game + "/entry.json", JSON.stringify(entry, null, 2) + "\n");
  const config = read("package-files.json", folder);
  for (const include of ["artwork.json", path])
    if (
      !config.include.includes(include) &&
      !(include.startsWith("assets/") && config.include.includes("assets"))
    )
      config.include.push(include);
  write(
    base,
    game + "/package-files.json",
    JSON.stringify(config, null, 2) + "\n",
  );
  sync(false, base);
  return { local: false, path: target };
}
if (cli(import.meta.url)) {
  const args = process.argv.slice(2),
    options = {};
  while (args.length) {
    const option = args.shift();
    if (option === "--local") options.local = true;
    else if (
      [
        "game",
        "file",
        "role",
        "creator",
        "source",
        "license",
        "basis",
        "evidence",
        "attribution",
      ].includes(option.substring(2))
    )
      options[option.substring(2)] = args.shift();
    else throw Error("Unknown option " + option);
  }
  if (!options.game || !options.file || !options.role)
    throw Error(
      "Usage: node scripts/import-artwork.mjs --game games/<author>/<game> --file <image> --role cover|hero [--local | --creator <name> --source <reference> --license <licence> --basis original|licensed|permission --evidence <reference>]",
    );
  console.log(importArtwork(options));
}
