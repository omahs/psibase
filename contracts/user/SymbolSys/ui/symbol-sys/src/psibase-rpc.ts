export const test = async () => {
  console.log("Initialized Applet", await psibaseRpc.getCurrentApplet());

  const testSiblingUrl = await psibaseRpc.siblingUrl(
    undefined,
    "token-sys",
    "getTokenTypes"
  );
  console.info("test sibling url >>>", testSiblingUrl);

  const tokenTypes = await psibaseRpc.getJson(testSiblingUrl);
  console.info("token types >>>", tokenTypes);
  return tokenTypes;
};
