import {
  lstatSync,
  readFileSync,
  readdirSync,
  realpathSync,
  existsSync,
  mkdirSync,
  writeFileSync,
} from "node:fs";
import { resolve, sep, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";
export const root = fileURLToPath(new URL("../", import.meta.url));
export const read = (path, base = root) =>
  JSON.parse(readFileSync(checkedFile(base, path), "utf8"));
export const hash = (bytes) => createHash("sha256").update(bytes).digest("hex");
export function inside(base, path) {
  if (
    typeof path !== "string" ||
    path.length > 240 ||
    path.includes("..") ||
    !/^[A-Za-z0-9][A-Za-z0-9._-]*(\/[A-Za-z0-9][A-Za-z0-9._-]*)*$/.test(path)
  )
    throw Error("Unsafe repository path: " + path);
  const full = resolve(base, path),
    anchor = resolve(base);
  if (!full.startsWith(anchor + sep)) throw Error("Path escapes repository");
  return full;
}
export function checkedFile(base, path) {
  const full = inside(base, path),
    stat = lstatSync(full);
  if (
    !stat.isFile() ||
    stat.isSymbolicLink() ||
    !realpathSync(full).startsWith(realpathSync(base) + sep)
  )
    throw Error("Redirected or non-file path: " + path);
  return full;
}
export function collect(base, paths) {
  const files = new Map();
  const visit = (path) => {
    const full = inside(base, path),
      stat = lstatSync(full);
    if (
      stat.isSymbolicLink() ||
      !realpathSync(full).startsWith(realpathSync(base) + sep)
    )
      throw Error("Redirected package path: " + path);
    if (stat.isDirectory())
      for (const name of readdirSync(full).sort()) visit(path + "/" + name);
    else if (stat.isFile()) {
      if (/\.(xex|xexp|iso|xiso|stfs|live|pirs|key|keys)$/i.test(path))
        throw Error("Game payload is not distributable: " + path);
      files.set(path, readFileSync(full));
    } else throw Error("Unsupported package file: " + path);
  };
  if (!Array.isArray(paths) || !paths.length)
    throw Error("Missing export files");
  for (const path of paths) visit(path);
  return files;
}
export const cli = (meta) =>
  process.argv[1] && resolve(process.argv[1]) === fileURLToPath(meta);
export function write(base, path, bytes) {
  const full = inside(base, path);
  if (existsSync(base) && lstatSync(base).isSymbolicLink())
    throw Error("Refusing to write through a redirected root");
  let current = resolve(base);
  for (const part of path.split("/")) {
    current = resolve(current, part);
    if (existsSync(current) && lstatSync(current).isSymbolicLink())
      throw Error("Refusing to write through a symlink: " + path);
  }
  mkdirSync(resolve(full, ".."), { recursive: true });
  writeFileSync(full, bytes);
  return full;
}
