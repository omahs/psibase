import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

const APPLET_CONTRACT = "symbol-sys";
const PSIBASE_ADDRESS = "http://psibase.127.0.0.1.sslip.io:8080";

export default defineConfig({
  plugins: [react()],
  server: {
    host: "localhost",
    port: 8081,
    proxy: {
      "/": {
        target: PSIBASE_ADDRESS,
        bypass: (req, _res, _opt) => {
          const host = req.headers.host || "";
          const subdomain = host.split(".")[0];
          if (
            subdomain === APPLET_CONTRACT &&
            req.method !== "POST" &&
            !req.url.startsWith("/common")
          ) {
            return req.url;
          }
        },
      },
    },
  },
});
