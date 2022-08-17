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
                    if (req.headers.host && req.headers.host.split(".")[0] === APPLET_CONTRACT) {
                        console.info(">>>", req.url, req.headers);
                        return req.url;
                    }
                }
            },
        }
    }
});
