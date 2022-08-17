import { defineConfig } from "vite";

const APPLET_CONTRACT = "symbol-sys"

export default defineConfig({
    server: {
        host: "localhost",
        port: "8081",
        proxy: {
            '/': {
                target: 'http://psibase.127.0.0.1.sslip.io:8080/',
                bypass: (req, _res, _opt) => {
                    const host = req.headers.host || ""
                    const subdomain = host.split(".")[0]
                    if (subdomain === APPLET_CONTRACT) {
                        return req.url;
                    }
                }
            },
        }
    }
});
