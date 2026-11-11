import {
  readFileSync,
  writeFileSync,
  mkdirSync,
  existsSync,
  unlinkSync,
} from "node:fs";
import { dirname, basename } from "node:path";
import {
  inside,
  collect,
  read,
  hash,
  root,
  cli,
  write,
  checkedFile,
} from "./repository.mjs";
import { projects } from "./development.mjs";
import { sync } from "./sync-packages.mjs";
export function exportModule(
  projectPath,
  { base = root, check = false, binary, platform } = {},
) {
  const project = projects(base).find(
    (project) => project.path === projectPath,
  );
  if (!project) throw Error("Unknown development project: " + projectPath);
  if (!project.payloadPath)
    throw Error(
      "This is a development-only project; add a reviewed catalogue entry before exporting it",
    );
  if (Boolean(binary) !== Boolean(platform))
    throw Error("Provide both --binary and --platform");
  const source = inside(base, project.path),
    destination = inside(base, project.payloadPath);
  const files = collect(source, [
    ...project.files,
    ...(binary ? [] : project.preparationFiles),
  ]);
  if (binary) {
    if (
      !/^[a-z0-9]+-[a-z0-9_]+$/.test(platform) ||
      !read(project.payloadPath + "/entry.json", base).platforms.includes(
        platform,
      )
    )
      throw Error("Binary platform is not declared by the catalogue entry");
    if (!/\.(dll|so|dylib)$/i.test(binary))
      throw Error("Expected a module shared library");
    const data = collect(source, [binary]).get(binary);
    const path = "bin/" + platform + "/" + basename(binary);
    files.set(path, data);
    const manifest = JSON.parse(files.get("xenon-module.json"));
    manifest.nativeExtension = { [platform]: path };
    files.set(
      "xenon-module.json",
      Buffer.from(JSON.stringify(manifest, null, 2) + "\n"),
    );
  }
  const current = read("package-files.json", destination);
  const artwork = current.include.filter(
    (path) =>
      path === "artwork.json" ||
      path === "assets" ||
      path.startsWith("assets/"),
  );
  const include = [...files.keys()].sort();
  const declaration =
    JSON.stringify({ include: [...include, ...artwork] }, null, 2) + "\n";
  const changes = [];
  for (const [path, bytes] of files) {
    const full = inside(destination, path);
    if (!existsSync(full) || !readFileSync(full).equals(bytes))
      changes.push(path);
  }
  if (
    readFileSync(inside(destination, "package-files.json"), "utf8") !==
    declaration
  )
    changes.push("package-files.json");
  if (check) {
    if (changes.length) throw Error("Export is stale: " + changes.join(", "));
    return { files: files.size, changed: 0 };
  }
  // Do not silently discard manual edits to old generated payload files.
  const previous = read("package.json", destination);
  for (const old of previous.files) {
    if (
      files.has(old.path) ||
      old.path === "artwork.json" ||
      old.path.startsWith("assets/")
    )
      continue;
    const full = inside(destination, old.path);
    if (
      existsSync(full) &&
      hash(
        readFileSync(checkedFile(base, project.payloadPath + "/" + old.path)),
      ) !== old.sha256
    )
      throw Error("Obsolete payload has local edits: " + old.path);
  }
  for (const [path, bytes] of files)
    write(base, project.payloadPath + "/" + path, bytes);
  for (const old of previous.files) {
    if (
      files.has(old.path) ||
      old.path === "artwork.json" ||
      old.path.startsWith("assets/")
    )
      continue;
    const full = inside(destination, old.path);
    if (existsSync(full)) unlinkSync(full);
  }
  write(base, project.payloadPath + "/package-files.json", declaration);
  sync(false, base);
  return { files: files.size, changed: changes.length };
}
if (cli(import.meta.url)) {
  const args = process.argv.slice(2),
    project = args.shift();
  let check = false,
    binary,
    platform;
  while (args.length) {
    const option = args.shift();
    if (option === "--check") check = true;
    else if (option === "--binary") binary = args.shift();
    else if (option === "--platform") platform = args.shift();
    else throw Error("Unknown option " + option);
  }
  if (!project)
    throw Error(
      "Usage: node scripts/export-module.mjs development/<author>/<game> [--check] [--binary <project-relative-file> --platform <host>]",
    );
  const result = exportModule(project, { check, binary, platform });
  console.log("Verified/exported " + result.files + " runtime files.");
}
