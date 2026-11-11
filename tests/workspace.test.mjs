import { test } from "node:test";
import assert from "node:assert/strict";
import {
  mkdtempSync,
  mkdirSync,
  writeFileSync,
  readFileSync,
  existsSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { resolve, dirname } from "node:path";
import { execFileSync } from "node:child_process";
import { exportModule } from "../scripts/export-module.mjs";
import { importArtwork } from "../scripts/import-artwork.mjs";
import { artworkIssues } from "../scripts/artwork.mjs";
import { packageIndex, sync } from "../scripts/sync-packages.mjs";
import { inside, collect, root } from "../scripts/repository.mjs";
const project = "development/fixture/example",
  game = "games/fixture/example";
const png = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII=",
  "base64",
);
function fixture(t) {
  const base = mkdtempSync(resolve(tmpdir(), "xenon-module-workspace-"));
  t.after(() => rmSync(base, { recursive: true, force: true }));
  const put = (path, value) => {
    const full = resolve(base, path);
    mkdirSync(dirname(full), { recursive: true });
    writeFileSync(
      full,
      typeof value === "object" && !Buffer.isBuffer(value)
        ? JSON.stringify(value, null, 2) + "\n"
        : value,
    );
  };
  put("catalog.json", {
    schema: "xenon.module-catalog",
    version: 2,
    modules: [game + "/entry.json"],
  });
  put(project + "/project.json", {
    schema: "xenon.development-project",
    version: 1,
    id: "fixture.example",
    payloadPath: game,
    files: ["xenon-module.json", "LICENSE"],
    preparationFiles: ["xenon-analysis"],
  });
  put(project + "/xenon-module.json", {
    id: "fixture.example",
    version: "1.0.0",
  });
  put(project + "/LICENSE", "fixture licence");
  put(project + "/xenon-analysis/manifest.json", {});
  put(project + "/tools/dev-only.txt", "must not ship");
  put(
    project + "/build/game.dll",
    Buffer.from("fictional native-module test fixture"),
  );
  put(game + "/entry.json", {
    id: "fixture.example",
    sourcePath: game,
    developmentPath: project,
    platforms: ["windows-x64"],
    launcher: {},
  });
  put(game + "/package-files.json", {
    include: ["xenon-module.json", "LICENSE"],
  });
  put(game + "/xenon-module.json", { id: "fixture.example", version: "1.0.0" });
  put(game + "/LICENSE", "fixture licence");
  sync(false, base);
  put("input.png", png);
  return { base, put };
}
test("explicit export ships runtime files and leaves development tools out", (t) => {
  const { base } = fixture(t);
  exportModule(project, { base });
  exportModule(project, { base, check: true });
  assert(!existsSync(resolve(base, game, "tools")));
  assert(
    packageIndex(
      JSON.parse(readFileSync(resolve(base, game, "entry.json"))),
      base,
    ).files.some((file) => file.path === "xenon-analysis/manifest.json"),
  );
  assert.throws(
    () =>
      exportModule(project, {
        base,
        binary: "../outside.dll",
        platform: "windows-x64",
      }),
    /Unsafe/,
  );
});
test("prebuilt export puts its DLL under bin/platform and removes preparation inputs", (t) => {
  const { base } = fixture(t);
  exportModule(project, { base });
  exportModule(project, {
    base,
    binary: "build/game.dll",
    platform: "windows-x64",
  });
  const manifest = JSON.parse(
    readFileSync(resolve(base, game, "xenon-module.json")),
  );
  assert.equal(
    manifest.nativeExtension["windows-x64"],
    "bin/windows-x64/game.dll",
  );
  assert(!existsSync(resolve(base, game, "xenon-analysis/manifest.json")));
  assert(
    readFileSync(resolve(base, game, "bin/windows-x64/game.dll")).equals(
      readFileSync(resolve(base, project, "build/game.dll")),
    ),
  );
  exportModule(project, {
    base,
    check: true,
    binary: "build/game.dll",
    platform: "windows-x64",
  });
});
test("export refuses to discard manually edited obsolete preparation data", (t) => {
  const { base, put } = fixture(t);
  exportModule(project, { base });
  put(game + "/xenon-analysis/manifest.json", { manual: "edit" });
  assert.throws(
    () =>
      exportModule(project, {
        base,
        binary: "build/game.dll",
        platform: "windows-x64",
      }),
    /local edits/,
  );
});
test("unconfirmed artwork stays local and requires explicit evidence for publication", (t) => {
  const { base } = fixture(t),
    file = resolve(base, "input.png");
  assert.throws(
    () => importArtwork({ base, game, file, role: "cover" }),
    /permission evidence/,
  );
  const local = importArtwork({ base, game, file, role: "cover", local: true });
  assert(readFileSync(local.path).equals(png));
  assert(!existsSync(resolve(base, game, "assets/cover.png")));
});
test("permitted artwork is indexed with attribution and a content-bound permission record", (t) => {
  const { base, put } = fixture(t),
    file = resolve(base, "input.png");
  importArtwork({
    base,
    game,
    file,
    role: "cover",
    creator: "Fixture artist",
    source: "Original test artwork",
    license: "CC0-1.0",
    basis: "original",
    evidence: "Fictional test fixture, not a real rights assertion",
    attribution: "Fixture artist",
  });
  const entry = JSON.parse(readFileSync(resolve(base, game, "entry.json")));
  assert.deepEqual(artworkIssues(entry, base), []);
  assert(
    packageIndex(entry, base).files.some(
      (file) => file.path === "assets/cover.png",
    ),
  );
  put(game + "/assets/cover.png", Buffer.concat([png, Buffer.from("changed")]));
  assert.throws(() => artworkIssues(entry, base), /changed since/);
});
test("unreferenced public images also need permission records", (t) => {
  const { base, put } = fixture(t);
  put(game + "/assets/extra.png", png);
  const entry = JSON.parse(readFileSync(resolve(base, game, "entry.json")));
  assert.equal(artworkIssues(entry, base).length, 1);
  assert.throws(() => inside(base, "games/../outside"), /Unsafe/);
  assert.throws(() => collect(base, ["../outside"]), /Unsafe/);
});
test("merged root ignores exclude development artifacts while release DLLs and art stay visible", () => {
  const ignored = (path) => {
    try {
      execFileSync("git", ["-c", "safe.directory=" + root, "check-ignore", "--no-index", "-q", path], {
        cwd: root,
      });
      return true;
    } catch (error) {
      if (error.status === 1) return false;
      throw error;
    }
  };
  for (const path of [
    "development/nimauria/project-gracemeria/build/module.dll",
    "development/nimauria/project-gears-2/revisions/test/local-validation.json",
    "development/nimauria/project-gracemeria/runtime/test/session/status.json",
    ".local/artwork/cover.png",
  ])
    assert(ignored(path), path);
  for (const path of [
    "games/nimauria/project-gracemeria/bin/windows-x64/module.dll",
    "games/nimauria/project-gracemeria/assets/cover.png",
    "development/nimauria/project-gracemeria/tools/validate_metadata.py",
  ])
    assert(!ignored(path), path);
});
