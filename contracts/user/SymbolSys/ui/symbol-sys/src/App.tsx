import { useEffect, useState } from "react";
import reactLogo from "./assets/react.svg";
import "./App.css";

const rpc = globalThis.psibaseRpc;

rpc.initializeApplet();

console.log("Initialized Applet", await rpc.getCurrentApplet());

function App() {
  const [count, setCount] = useState(0);
  const [tokenTypes, setTokenTypes] = useState<any>({});

  useEffect(() => {
    const x = async () => {
      const testSiblingUrl = await rpc.siblingUrl(
        undefined,
        "token-sys",
        "getTokenTypes"
      );
      console.info("test sibling url >>>", testSiblingUrl);

      const tokenTypes = await rpc.getJson(testSiblingUrl);
      console.info("token types >>>", tokenTypes);
      setTokenTypes(tokenTypes);
    };
    x();
  }, []);

  return (
    <div className="App">
      <div>
        <a href="https://vitejs.dev" target="_blank">
          <img src="/vite.svg" className="logo" alt="Vite logo" />
        </a>
        <a href="https://reactjs.org" target="_blank">
          <img src={reactLogo} className="logo react" alt="React logo" />
        </a>
      </div>
      <h1>Vite + React5</h1>
      <div className="card">
        <button onClick={() => setCount((count) => count + 1)}>
          count is {count}
        </button>
        <p>PsiBase Token Types:</p>
        <pre style={{ width: 600, textAlign: "left" }}>
          {JSON.stringify(tokenTypes, undefined, 4)}
        </pre>
      </div>
      <p className="read-the-docs">
        Click on the Vite and React logos to learn more
      </p>
    </div>
  );
}

export default App;
