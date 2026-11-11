import {
  readFileSync,
  writeFileSync,
  readdirSync,
  lstatSync,
  realpathSync,
} from "node:fs";
import { resolve, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";
export const root = fileURLToPath(new URL("../", import.meta.url));
export const read = (path, base = root) =>
  JSON.parse(readFileSync(resolve(base, path), "utf8"));
export function packageIndex(entry, base = root) {
  const folder = resolve(base, entry.sourcePath),
    files = new Map();
  const visit = (path) => {
    if (
      path.length > 240 ||
      !/^[A-Za-z0-9][A-Za-z0-9._-]*(\/[A-Za-z0-9][A-Za-z0-9._-]*)*$/.test(
        path,
      ) ||
      path.includes("..")
    )
      throw Error("Unsafe package path: " + path);
    const full = resolve(folder, path),
      stat = lstatSync(full);
    if (
      !full.startsWith(folder + sep) ||
      stat.isSymbolicLink() ||
      !realpathSync(full).startsWith(realpathSync(folder) + sep)
    )
      throw Error("Package path escapes module folder");
    if (stat.isDirectory()) {
      for (const item of readdirSync(full).sort()) visit(path + "/" + item);
    } else if (stat.isFile()) {
      if (/\.(xex|iso|xexp|stfs)$/i.test(path))
        throw Error("Proprietary game payload: " + path);
      const bytes = readFileSync(full);
      files.set(path, {
        path,
        size: bytes.length,
        sha256: createHash("sha256").update(bytes).digest("hex"),
      });
    } else throw Error("Unsupported package file: " + path);
  };
  for (const path of read(entry.sourcePath + "/package-files.json", base)
    .include)
    visit(path);
  const manifest = read(entry.sourcePath + "/xenon-module.json", base);
  if (manifest.id !== entry.id || !files.has("xenon-module.json"))
    throw Error("Package manifest identity mismatch");
  if (manifest.nativeExtension) {
    const binaries =
      typeof manifest.nativeExtension === "string"
        ? [manifest.nativeExtension]
        : Object.values(manifest.nativeExtension);
    if (binaries.some((path) => typeof path !== "string" || !files.has(path)))
      throw Error("A declared module DLL is missing from the package index");
  }
  const rows = [...files.values()].sort((a, b) =>
    a.path < b.path ? -1 : a.path > b.path ? 1 : 0,
  );
  if (
    !rows.length ||
    rows.length > 20000 ||
    rows.reduce((sum, f) => sum + f.size, 0) > 128 * 1024 * 1024
  )
    throw Error("Package exceeds launcher limits");
  return {
    schema: "xenon.repository-package",
    version: 1,
    moduleId: entry.id,
    moduleVersion: manifest.version,
    files: rows,
  };
}
export function sync(check = false, base = root) {
  for (const path of read("catalog.json", base).modules) {
    const entry = read(path, base),
      output = resolve(base, entry.sourcePath, "package.json");
    const expected = JSON.stringify(packageIndex(entry, base), null, 2) + "\n";
    if (check) {
      if (readFileSync(output, "utf8") !== expected)
        throw Error(
          "Stale package index: " +
            relative(root, output) +
            ". Run node scripts/sync-packages.mjs",
        );
    } else writeFileSync(output, expected);
  }
}
if (
  process.argv[1] &&
  resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  sync(process.argv.includes("--check"));
  console.log("Module package indexes verified.");
}
