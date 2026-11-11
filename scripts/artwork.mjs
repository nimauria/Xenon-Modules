import { existsSync, readFileSync } from "node:fs";
import {
  inside,
  checkedFile,
  collect,
  hash,
  read,
  root,
} from "./repository.mjs";
export function artworkIssues(entry, base = root) {
  const folder = inside(base, entry.sourcePath),
    issues = [];
  const images = existsSync(inside(folder, "assets"))
    ? collect(folder, ["assets"])
    : new Map();
  const records = existsSync(inside(folder, "artwork.json"))
    ? read("artwork.json", folder).assets
    : [];
  if (!Array.isArray(records)) throw Error("Artwork records must be an array");
  const seen = new Set();
  for (const record of records) {
    if (!record || typeof record.path !== "string" || seen.has(record.path))
      throw Error("Invalid or duplicate artwork record");
    inside(folder, record.path);
    seen.add(record.path);
    if (record.redistribution === "permitted") {
      if (
        !["original", "licensed", "permission"].includes(record.basis) ||
        ["creator", "source", "license", "evidence", "sha256"].some(
          (key) => typeof record[key] !== "string" || !record[key].trim(),
        )
      )
        throw Error("Artwork permission record is incomplete: " + record.path);
      const file = checkedFile(folder, record.path);
      if (hash(readFileSync(file)) !== record.sha256)
        throw Error(
          "Artwork changed since its permission record: " + record.path,
        );
    } else if (!["unverified", "local-only"].includes(record.redistribution))
      throw Error("Unknown artwork redistribution status");
  }
  for (const [path] of images) {
    if (!/\.(png|jpe?g|webp|svg|gif|avif)$/i.test(path)) continue;
    const record = records.find((record) => record.path === path);
    if (!record || record.redistribution !== "permitted")
      issues.push(
        "Artwork permission is unconfirmed: " + entry.sourcePath + "/" + path,
      );
  }
  for (const key of ["tileArt", "heroArt"]) {
    const path = entry.launcher?.[key];
    if (path === undefined || path === "") continue;
    if (
      typeof path !== "string" ||
      !path.startsWith(entry.sourcePath + "/assets/")
    )
      throw Error("Artwork must be inside the game assets folder");
    checkedFile(base, path);
    if (!/\.(png|jpe?g|webp)$/i.test(path))
      throw Error("Unsupported launcher artwork format");
  }
  return issues;
}
