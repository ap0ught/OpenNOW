import { spawnSync } from "node:child_process";
import { readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");

/**
 * Collect `*.node.test.ts` under `src/` for Node's built-in test runner.
 * Keeps Windows/macOS/Linux compatible (no shell glob expansion).
 */
function collectNodeTestFiles(dir, acc = []) {
  for (const name of readdirSync(dir)) {
    const full = join(dir, name);
    const st = statSync(full);
    if (st.isDirectory()) {
      collectNodeTestFiles(full, acc);
    } else if (name.endsWith(".node.test.ts")) {
      acc.push(full);
    }
  }
  return acc;
}

const files = collectNodeTestFiles(join(root, "src")).sort();
if (files.length === 0) {
  console.error("No *.node.test.ts files found under src/");
  process.exit(1);
}

const r = spawnSync(process.execPath, ["--import", "tsx", "--test", ...files], {
  cwd: root,
  stdio: "inherit",
  env: { ...process.env },
});

process.exit(r.status === null ? 1 : r.status);
