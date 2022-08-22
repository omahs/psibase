export type Operation = {
  id: string;
  exec: <OperationParams>(params: OperationParams) => Promise<void>;
};

export declare global {
  var psibaseRpc: {
    getCurrentApplet: () => Promise<string>;
    initializeApplet: () => Promise<void>;
    getJson: (url: string) => Promise<any>;
    query: <Params, Response>(
      applet: string,
      subPath: string,
      queryName: string,
      params: Params,
      callback: (res: Response) => void
    ) => void;
    action: <ActionParams>(
      application: string,
      actionName: string,
      params: ActionParams,
      sender?: string
    ) => void;
    siblingUrl: (
      baseUrl?: string,
      contract: string,
      path: string
    ) => Promise<string>;
    setOperations: (operations: Operation[]) => void;
    operation: <Params>(
      applet: string,
      subPath: string,
      name: string,
      params: Params
    ) => void;
  };
}
