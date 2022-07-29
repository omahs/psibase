import htm from "/common/htm.module.js";
import {
  initializeApplet,
  getJson,
  action,
  addRoute,
  push,
  getLocalResource,
  CommonResources,
} from "/common/rpc.mjs";

const { useEffect, useState, useCallback, useRef } = React;
const { Segment, Header, Form, Table, Input, Button, Message, Tab, Container } =
  semanticUIReact;

const html = htm.bind(React.createElement);

await initializeApplet();

export const thisApplet = await getJson("/common/thiscontract");

const transactionTypes = {
  create: 0,
  buySymbol: 1,
};

function SymbolsRows(symbols) {
  return symbols.map((b) => {
    return html`
      <${Table.Row}>
        <${Table.Cell}>${b.symbol}</${Table.Cell}>
        <${Table.Cell}>${b.quantity}</${Table.Cell}>
      </${Table.Row}>
    `;
  });
}

function SymbolsTable({ loggedInUser }) {
  // const [balances, setBalances] = useState([]);
  // const [user, setUser] = useState("");
  // const [searchTerm, setSearchTerm] = useState("");

  // const onSearchSubmit = (e) => {
  //   e.preventDefault();
  //   setUser(searchTerm);
  // };

  // useEffect(()=>{
  //   setSearchTerm(loggedInUser);
  //   setUser(loggedInUser);
  // }, [loggedInUser]);

  // const getBalances = useCallback(async () => {
  //   if (user)
  //   {
  //     let res = await getJson(siblingUrl(null, thisApplet, "balances/" + user));
  //     setBalances(res);
  //   }
  // }, [user]);

  // const refreshBalances = useCallback((trace) => {
  //   getBalances().catch(console.error);
  // }, [getBalances]);

  // useEffect(()=>{
  //   refreshBalances();
  // }, [refreshBalances])

  // useEffect(()=>{
  //   addRoute("refreshbalances", transactionTypes.credit, refreshBalances);
  // }, [refreshBalances]);

  const symbols = [
    {
      symbol: "PSI",
      quantity: 100_000_000.0,
    },
    {
      symbol: "XPT",
      quantity: 1_000_000.0,
    },
    {
      symbol: "BRL",
      quantity: 1_000_000_000.0,
    },
  ];

  return html`
    <${Table} celled striped>
      <${Table.Header}>
        <${Table.Row}>
          <${Table.HeaderCell}>Symbol</${Table.HeaderCell}>
          <${Table.HeaderCell}>Quantity</${Table.HeaderCell}>
        </${Table.Row}>
      </${Table.Header}>

      <${Table.Body}>
        ${SymbolsRows(symbols)}
      </${Table.Body}>
    </${Table}>
  `;
}

function EmptyPanel() {
  return html`
    <${Segment}>
      Not yet implemented
    </${Segment}>
  `;
}

const EMPTY_MESSAGE = { message: "", type: "positive" };

function GetMessage({ type, message }) {
  if (message) {
    return html` <${Message} ${type} header="" content=${message} /> `;
  } else return html``;
}

function CreatePanel() {
  const [form, setForm] = useState({ newSymbol: "", maxDebit: "" });
  const [message, setMessage] = useState(EMPTY_MESSAGE);

  const onChangeForm = (e) => {
    const field = e.target.name;
    const value = e.target.value;
    setForm({ ...form, [field]: value });
  };

  const onSendSubmit = (e) => {
    e.preventDefault();
    setMessage(EMPTY_MESSAGE);

    const create = async () => {
      const actionData = {
        newSymbol: form.newSymbol,
        maxDebit: { value: parseInt(form.maxDebit) },
      };
      console.info(actionData);

      push(transactionTypes.create, [
        await action(thisApplet, "create", actionData),
      ]);
    };

    // TODO: failing trxs are swallowing the error code
    create().catch((e) => {
      console.error(e);
      setMessage({ type: "negative", message: e.message || e.toString() });
    });
  };

  return html`
    <${Form} onSubmit=${onSendSubmit}>
      <${Form.Field}>
        <${Input} fluid name='newSymbol' label='New Symbol:' placeholder='BTC' defaultValue='${form.newSymbol}' onChange=${onChangeForm}/>
      </${Form.Field}>
      <${Form.Field}>
        <${Input} fluid name='maxDebit' label='Max Debit:' placeholder='1000' defaultValue='${form.maxDebit}' onChange=${onChangeForm}/>
      </${Form.Field}>
      <${Form.Field}>
        <${Button} primary type="submit">Send</${Button}>
      </${Form.Field}>
    </${Form}>
    <${GetMessage} ${message} />
  `;
}

const panes = [
  {
    menuItem: "Create",
    render: () => {
      return html`<${Tab.Pane}><${CreatePanel} /></${Tab.Pane}>`;
    },
  },
  {
    menuItem: "Buy",
    render: () => {
      return html`<${Tab.Pane}><${EmptyPanel} /></${Tab.Pane}>`;
    },
  },
];

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
    <${Container}>
      <${Header} as='h1'>Symbols</${Header}>
      <${SymbolsTable} />
      <${Tab} panes=${panes} />
    </${Container}>
  `;
}

const container = document.getElementById("root");
const root = ReactDOM.createRoot(container);
root.render(html`<${App} />`);
