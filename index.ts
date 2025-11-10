import {
    getArg,
    getCurrentPlatformNam
} from "./utils/common";

type BuildMode = 'debug' | 'release'
type Platform = 'windows' | 'linux' | 'macos'

const packagers = {
    windows: "./packagers/windows/main"
}

export class Builder {
    private viteConfigPath: string;
    private eziConfigPath: string;
    private outDir: string;
    private viteConfig: any;
    private eziConfig: any;
    private mode: BuildMode;
    private platform: Platform;

    constructor(config: {
        viteConfigPath?: string;
        eziConfigPath?: string;
        outDir?: string;
        mode?: BuildMode;
        platform?: Platform;
    }) {
        this.mode = config.mode || (getArg("--mode") as BuildMode) || 'debug';
        this.platform = config.platform || (getArg("--platform") as Platform) || getCurrentPlatformNam();
        this.viteConfigPath = config.viteConfigPath ?? "vite.config.js";
        this.eziConfigPath = config.eziConfigPath ?? "ezi.config.ts";
        this.outDir = config.outDir ?? "gen";
        this.LoadConfig();
    }
    public LoadConfig() {

    }

    public genAssets() {

    }

    public build() {

    }

    public dev() {

    }

    public main() {
        switch (this.mode) {
            case 'debug':
                this.dev();
                break;
            case 'release':
                this.build();
                break;
            default:
                throw new Error(`Unsupported build mode: ${this.mode}`);
        }
    }
}


if (require.main === module) {
    const builder = new Builder({});
    builder.main();
}