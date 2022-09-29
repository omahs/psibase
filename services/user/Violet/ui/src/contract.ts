import { action, AppletId, getJson, operation, Operation, setOperations } from "common/rpc.mjs";

export class Service {
    protected cachedApplet?: string;

    constructor() {
        this.applet();
    }

    protected async applet(): Promise<string> {
        if (this.cachedApplet) return this.cachedApplet;
        const appletName = await getJson<string>("/common/thisservice");
        this.cachedApplet = appletName;
        console.log(appletName, 'is this applet')
        return appletName;
    }

    public async getAppletName(): Promise<string> {
        return this.applet();
    }

    public async getAppletId(): Promise<AppletId> {
        const appletName = await this.getAppletName();
        return new AppletId(appletName);
    }

    public get ops(): Operation[] {
        // @ts-ignore 
        return this.operations || []
    }


}

export function Action(target: any, key: string, descriptor: any) {
    const originalMethod = descriptor.value;
    descriptor.value = function (...args: any[]) {
        const result = originalMethod.apply(this, args);
        return action('violet', key, result)
    }
};


export function Op(name?: string) {
    return function (target: any, key: string, descriptor: any) {
        const id = name ? name : key;
        const op: Operation = {
            exec: descriptor.value.bind(target),
            id
        };
        const isExistingArray = 'operations' in target;
        if (isExistingArray) {
            target['operations'].push(op)
        } else {
            target['operations'] = [op]
        }

        descriptor.value = function (...args: any[]) {
            return operation(new AppletId('violet'), id, ...args)
        }
    };
}
