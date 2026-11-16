import { NhostClient } from "@nhost/nhost-js";
import { loadEnv } from "vite";

const env = loadEnv("production", process.cwd())
export const nhost = new NhostClient({
  authUrl: env["VITE_NHOST_AUTH"],
  storageUrl: env["VITE_NHOST_STORAGE"],
  graphqlUrl: env["VITE_NHOST_GRAPHQL"],
  functionsUrl: env["VITE_NHOST_FUNCTIONS"],
});