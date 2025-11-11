import { spawn } from "child_process";
import { blue, bold, green, red, yellow } from "./colorf";
import {
    getArg,
    getCurrentPlatformName,
    getCurrentTimeString,
    getAllFiles
} from "./utils";
import * as fs from "fs";
import * as path from "path";
import { build, createServer } from "vite";
import { compress } from '@mongodb-js/zstd'

type BuildMode = 'debug' | 'release'
type Platform = 'windows' | 'linux' | 'macos'

const packagers = {
    windows: "../packagers/windows/main"
}

export class Builder {
    private viteConfigPath: string;
    private eziConfigPath: string;
    private genTempFilePath: string;
    private outDir: string;
    private viteConfig: any;
    private eziConfig: any;
    private mode: BuildMode;
    private platform: Platform;

    private eziDevExePath: string = path.join(__dirname, "../bin/eziapp-dev.exe");

    constructor(config: {
        viteConfigPath?: string;
        eziConfigPath?: string;
        outDir?: string;
        mode?: BuildMode;
        platform?: Platform;
        genTempFilePath?: string;
    }) {
        this.mode = config.mode || (getArg("--mode") as BuildMode) || 'debug';
        this.platform = config.platform || (getArg("--platform") as Platform) || getCurrentPlatformName();
        this.viteConfigPath = config.viteConfigPath ?? "vite.config.js";
        this.eziConfigPath = config.eziConfigPath ?? "ezi.config.ts";
        this.outDir = config.outDir ?? path.join(process.cwd(), "dist");
        this.genTempFilePath = config.genTempFilePath || path.join(process.cwd(), "temp");
        this.LoadConfig();
    }
    public async LoadConfig() {
        // vite config
        let viteConfig = {} as any;
        try {
            viteConfig = await import(path.join(process.cwd(), this.viteConfigPath));
            if (viteConfig.default) {
                viteConfig = viteConfig.default;
            }
        } catch (e) {
            console.log(yellow("! no vite.config.ts found, using default config."));
        }
        this.viteConfig = viteConfig;

        // ezi config
        let eziConfig = {} as any;
        try {
            eziConfig = await import(path.join(process.cwd(), this.eziConfigPath));
            if (eziConfig.default) {
                eziConfig = eziConfig.default;
            }
        } catch (e) {
            console.log(yellow("! no ezi.config.ts found, using default config."));
        }
        this.eziConfig = eziConfig;
    }

    public async genAssets() {
        const assetsMatas: {
            [key: string]: {
                offset: number;
                size: number;
            };
        } = {};
        const assetsBinarys: Buffer[] = [];
        let offset = 0;

        // 打包ezi配置文件
        const configBuffer = await compress(Buffer.from(JSON.stringify(this.eziConfig), "utf-8"));
        const configSize = configBuffer.length;
        assetsMatas["ezi.config.manifest"] = { offset, size: configSize };
        assetsBinarys.push(configBuffer);
        offset += configSize;

        // 打包资源文件
        const assetsDir = path.join(process.cwd(), this.viteConfig?.build?.outDir || "dist");
        const files = getAllFiles(assetsDir);

        for (const file of files) {
            const filePath = path.join(assetsDir, file);
            const stat = fs.statSync(filePath);
            if (stat.isFile()) {
                const packageName = this.eziConfig?.application?.package || "com.ezi.app";
                const fileBuffer = await compress(fs.readFileSync(filePath));
                const assetsId = `https://${packageName}/${file.split(path.sep).join("/")}`;
                const size = fileBuffer.length;
                assetsMatas[assetsId] = { offset, size };
                assetsBinarys.push(fileBuffer);
                offset += size;
            }
        }

        // 打包清单文件
        const manifestBuffer = await compress(Buffer.from(JSON.stringify(assetsMatas), "utf-8"));
        const manifestSize = manifestBuffer.length;
        if (manifestSize > 4294967295) {
            throw new Error("Manifest size exceeds 4GB limit.");
        }
        assetsBinarys.push(manifestBuffer);

        // 写入清单头
        const manifestSizeFlag = 4;
        const headerBuffer = Buffer.alloc(manifestSizeFlag);
        headerBuffer.writeUInt32LE(manifestSize, 0);
        assetsBinarys.push(headerBuffer);

        fs.writeFileSync(path.join(process.cwd(), 'temp/ezi.assets.binary'), Buffer.concat(assetsBinarys));

        console.log(green("✓ assets generated."));
    }

    public async build() {
        await build(this.viteConfig);
        await this.genAssets();
    }

    public async dev() {
        const startTime = Date.now();
        const server = await createServer();
        await server.listen();

        const devEziConfig = structuredClone(this.eziConfig);
        devEziConfig.application.package += ".debug";

        const eziConfigJsonPath = path.join(this.genTempFilePath, "ezi.config.json");
        fs.writeFileSync(eziConfigJsonPath, JSON.stringify(devEziConfig, null, 4), { encoding: "utf-8" });

        const appName = devEziConfig?.application?.name || "EziApplication";
        const child = spawn(this.eziDevExePath, [
            '--configpath',
            eziConfigJsonPath,
            '--cwd',
            process.cwd()
        ], {
            stdio: ["ignore", "pipe", "pipe"],
            windowsHide: false
        });

        child.stdout?.on("data", (data) => {
            process.stdout.write(getCurrentTimeString() + bold(blue(` [${appName}] `)) + data);
        });

        const srcDir = path.join(process.cwd(), this.viteConfig?.root || "");
        child.stderr?.on("data", (data) => {
            const errorMsg = data.toString().replace("LOCATION_ORIGIN", srcDir.replaceAll("\\", "/"));
            process.stderr.write(red(errorMsg));
        });

        child.on("exit", (code) => {
            console.log(getCurrentTimeString() + bold(blue(` [${appName}] `)) + yellow(`process [pid:${child.pid}] exited with code ${code ?? 0}.`));
            process.exit(0);
        });

        function printBoxedMessage(lines: string[]) {
            const stripAnsi = (str: string) => str.replace(/\u001b\[[0-9;]*m/g, '');
            const width = Math.max(...lines.map(line => stripAnsi(line).length));
            const top = `╔${"═".repeat(width + 2)}╗`;
            const bottom = `╚${"═".repeat(width + 2)}╝`;
            const content = lines.map(line => `║ ${line.padEnd(width + (line.length - stripAnsi(line).length))} ║`);
            console.log(top);
            content.forEach(line => console.log(line));
            console.log(bottom);
        }

        const viteVersion = require("vite/package.json").version;

        printBoxedMessage([
            `${bold(green('VITE')) + green(' v' + viteVersion)} ready in ${bold((Date.now() - startTime).toString())} ms`,
            ``,
            ` ${green('➜')}  ${bold('devEntry')}: ${blue('http://localhost:' + bold(server.config.server.port.toString()))}`,
            ` ${green("➜")}  ${bold("EziApp")}:   ${bold(blue(appName))} ${green("running")} [pid:${bold("" + child.pid)}]`,
            ` ${green("➜")}  ${bold("Package")}:  ${green(devEziConfig?.application?.package ?? "com.ezi.app")}`,
            ` ${green("➜")}  ${bold("Started")}:  ${blue(new Date().toLocaleString())}`,
        ]);

        const cleanup = () => {
            if (!child.killed) {
                child.kill("SIGTERM");
            }
            server.close();
        };

        process.on("SIGINT", cleanup);
        process.on("SIGTERM", cleanup);
        process.on("exit", cleanup);
    }

    public async main() {
        switch (this.mode) {
            case 'debug':
                await this.dev();
                break;
            case 'release':
                await this.build();
                break;
            default:
                console.error(red(`✗ Unsupported build mode: ${this.mode}`));
                process.exit(1);
        }
    }
}
