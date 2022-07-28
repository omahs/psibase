function App() {
  const [user, setUser] = useState("");

  useEffect(() => {
    // Todo - Timeout is used because sometimes the window.parentIFrame isn't loaded yet when
    //  this runs. Should use a better fix for the race condition than a delay.
    setTimeout(() => {
      getLocalResource(CommonResources.loggedInUser, setUser);
    }, 50);
  }, []);

  return html`
    <div class="ui container">
      <h1>Symbol</h1>
      <p>User ${user}</p>
    </div>
  `;
}

const container = document.getElementById("root");
const root = ReactDOM.createRoot(container);
root.render(html`<${App} />`);
