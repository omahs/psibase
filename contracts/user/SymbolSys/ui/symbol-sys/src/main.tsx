import React from "react";
import ReactDOM from "react-dom/client";
import "./index.css";
import { App } from "./App";

const renderApp = () => {
  psibaseRpc.initializeApplet();
  ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>
  );
};

// Polyfill Psibase RPC
if (typeof psibaseRpc === "undefined") {
  const psibaseUrl = import.meta.env.VITE_PSIBASE_URL;
  console.info("Psibase URL >>>", psibaseUrl);
  import(
    /* @vite-ignore */
    `${psibaseUrl}/common/rpc.mjs`
  ).then((rpc) => {
    globalThis.psibaseRpc = rpc;
    renderApp();
  });
} else {
  renderApp();
}
