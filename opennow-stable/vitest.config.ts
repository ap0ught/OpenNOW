import { resolve } from "node:path";
import { defineConfig } from "vitest/config";

export default defineConfig({
  resolve: {
    alias: {
      "@shared": resolve(__dirname, "src/shared"),
    },
  },
  test: {
    environment: "node",
    include: ["src/**/*.vitest.test.ts"],
    // Tests must not rely on NVIDIA APIs — keep everything local / mocked.
    pool: "forks",
  },
});
