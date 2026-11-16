import solid from "solid-start/vite";
import cesium from 'vite-plugin-cesium';
import {defineConfig} from "vite";
import suidPlugin from "@suid/vite-plugin";

export default defineConfig({
  plugins: [suidPlugin(), solid({ssr: false}), cesium()],
  server: {
    host: true,
    port: 8080
  }
});
