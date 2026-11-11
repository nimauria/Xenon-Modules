import { existsSync, readdirSync } from "node:fs";
import { inside, read, root, checkedFile } from "./repository.mjs";
export function projects(base = root) {
  const folder = inside(base, "development");
  if (!existsSync(folder)) return [];
  const found = [];
  for (const owner of readdirSync(folder, { withFileTypes: true })) {
    if (owner.name === "README.md") continue;
    if (
      !owner.isDirectory() ||
      owner.isSymbolicLink() ||
      !/^[a-z0-9][a-z0-9-]*$/.test(owner.name)
    )
      throw Error("Invalid development contributor folder");
    for (const project of readdirSync(
      inside(base, "development/" + owner.name),
      { withFileTypes: true },
    )) {
      if (
        !project.isDirectory() ||
        project.isSymbolicLink() ||
        !/^[a-z0-9][a-z0-9._-]*$/.test(project.name)
      )
        throw Error("Invalid development project folder");
      const path = "development/" + owner.name + "/" + project.name;
      const definition = read(path + "/project.json", base),
        manifest = read(path + "/xenon-module.json", base);
      if (
        definition.schema !== "xenon.development-project" ||
        definition.version !== 1 ||
        definition.id !== manifest.id ||
        !/^([a-z0-9]+[._-])+[a-z0-9]+$/.test(definition.id)
      )
        throw Error("Development project identity mismatch");
      if (
        !Array.isArray(definition.files) ||
        !definition.files.includes("xenon-module.json") ||
        !Array.isArray(definition.preparationFiles)
      )
        throw Error("Development project export files are missing");
      for (const file of [...definition.files, ...definition.preparationFiles])
        inside(inside(base, path), file);
      if (definition.payloadPath !== null) {
        if (
          definition.payloadPath !==
          "games/" + owner.name + "/" + project.name
        )
          throw Error(
            "Development payload folder must match the contributor and project",
          );
        const entry = read(definition.payloadPath + "/entry.json", base);
        if (entry.id !== definition.id || entry.developmentPath !== path)
          throw Error("Development and catalogue identities differ");
      }
      found.push({ path, ...definition });
    }
  }
  if (new Set(found.map((project) => project.id)).size !== found.length)
    throw Error("Duplicate development module identity");
  return found;
}
