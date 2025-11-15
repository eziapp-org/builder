import * as fs from "fs";
import * as path from "path";
import { execSync } from "child_process";

class windowsPackager {
    private packagerBinPath = path.join(__dirname, "../../bin/eziapp-packager-windows.exe");
    private eziappBinPath = path.join(__dirname, "../../bin/eziapp-release.exe");
    private argv: string[] = [];
    private eziConfig: any;
    private outDir: string;
    private tempDir: string;

    constructor({ eziConfig, outDir, tempDir }: {
        eziConfig: any;
        outDir: string;
        tempDir: string;
    }) {
        this.eziConfig = eziConfig;
        this.outDir = outDir;
        this.tempDir = tempDir;
    }
    public async package() {
        console.log("packaging for Windows...");
        const appName = this.eziConfig?.application?.name || "EziApp";

        // 复制到输出目录
        const outAppPath = path.join(this.outDir, `${appName}_winx64.exe`);
        fs.copyFileSync(this.eziappBinPath, outAppPath);

        // 输入程序参数
        this.argv.push(...['--input', outAppPath]);

        // 打包ezi资源参数
        this.argv.push(...['--ezi-asset', path.join(this.tempDir, 'ezi.assets.binary')]);

        // 打包图标参数
        const iconPath = path.join(this.tempDir, 'eziapp.ico');
        if (fs.existsSync(iconPath)) {
            this.argv.push(...['--icon', iconPath]);
        }

        // 打包版本参数

        this.argv.push(...['--ver-productName', appName]);

        const version = this.eziConfig?.application?.version;
        if (version) {
            this.argv.push(...['--ver-productVersion', version]);
            this.argv.push(...['--ver-fileVersion', version]);
        }
        const companyName = this.eziConfig?.application?.author;
        if (companyName) {
            this.argv.push(...['--ver-companyName', companyName]);
        }
        const description = this.eziConfig?.application?.description;
        if (description) {
            this.argv.push(...['--ver-fileDescription', description]);
        }

        try {
            // 开始打包
            const packagerCmd = `"${this.packagerBinPath}" --update-version true ${this.argv.join(' ')}`;
            execSync(packagerCmd, { stdio: 'inherit' });
            console.log("Packaging completed:", outAppPath);
        } catch (error) {
            console.error(error);
            fs.unlinkSync(outAppPath);
            process.exit(1);
        }
    }
}

export default windowsPackager;