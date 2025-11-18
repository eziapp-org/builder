import * as fs from "fs";
import * as path from "path";
import { execSync } from "child_process";
import chalk from "chalk";

type Sizes = {
    name: string;
    size: number;
    color?: string;
}[];

class windowsPackager {
    private packagerBinPath = path.join(__dirname, "../../bin/eziapp-packager-winx64.exe");
    private eziappBinPath = path.join(__dirname, "../../bin/eziapp-npm-release-winx64.exe");
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
        if (!fs.existsSync(this.outDir)) {
            fs.mkdirSync(this.outDir, { recursive: true });
        }
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
            console.log(chalk.green("✓ Packaging completed:\n" + outAppPath));
        } catch (error) {
            console.error(error);
            fs.unlinkSync(outAppPath);
            process.exit(1);
        }

        // Build Report
        const Sizes = [
            {
                name: 'eziapp-core',
                size: fs.statSync(this.eziappBinPath).size
            },
            {
                name: 'fontend-assets',
                size: fs.statSync(path.join(this.tempDir, 'ezi.assets.binary')).size
            },
            {
                name: 'exe-icon',
                size: fs.existsSync(iconPath) ? fs.statSync(iconPath).size : 0
            },
            {
                name: 'extensions',
                size: 0
            }
        ];
        const appRelativePath = path.relative(process.cwd(), outAppPath);
        this.printBuildReport(Sizes, appRelativePath);

    }

    public printBuildReport(Sizes: Sizes, outAppPath?: string) {

        // 由大到小排序
        Sizes.sort((a, b) => b.size - a.size);

        // 随机设置颜色
        const presetColors = [
            '#e63946',
            '#f1a208',
            '#2a9d8f',
            '#118ab2',
            '#9900a7'
        ];


        function assignColors(Sizes: Sizes) {
            const shuffled = presetColors
                .map(c => ({ c, r: Math.random() }))
                .sort((a, b) => a.r - b.r)
                .map(x => x.c);

            Sizes.forEach((item, idx) => {
                item.color = shuffled[idx % shuffled.length];
            });
        }
        assignColors(Sizes);

        const totalSize = Sizes.reduce((sum, m) => sum + m.size, 0);

        const puts = (str: string) => {
            process.stdout.write(str);
        };

        // 打印标题
        puts(chalk.bold('\n═ Build Report ════════════════════\n\n'));

        puts('Platform: Windows x64\n');
        puts('Build Date: ' + new Date().toLocaleString() + '\n');
        puts('Status: ' + chalk.green.bold('0 error(s), 0 warning(s)\n'));
        if (outAppPath) {
            puts(`Output: ` + chalk.bold(outAppPath) + `\n`);
        }

        // 绘制占比进度条
        const barLength = 35;
        puts(chalk.bold('─'.repeat(barLength)));
        puts('\n');
        Sizes.forEach(m => {
            const percent = m.size / totalSize;
            const blocks = Math.round(percent * barLength);
            puts(chalk.hex(m.color || '#ffffff').bold('█'.repeat(blocks)));
        });
        puts('\n');

        // 打印模块表格

        const header =
            '─ Module '.padEnd(19, '─') +
            ' Size '.padEnd(12, '─') +
            ' Pct ';
        puts(chalk.bold(header) + '\n');

        Sizes.forEach(m => {
            const percent = ((m.size / totalSize) * 100).toFixed(0) + '%';
            const sizeKB = (m.size / 1024).toFixed(0) + 'KB';

            const line =
                chalk.hex(m.color || '#ffffff').bold(('■ ' + m.name).padEnd(20)) +
                sizeKB.padEnd(12) +
                percent;

            puts(line + '\n');
        });

        puts(chalk.bold('─'.repeat(barLength)));
        puts('\n');
        puts((`Total size: `) + chalk.bold(`${(totalSize / 1024).toFixed(0)}KB\n`));
        puts(`UPX: ` + chalk.bold('disabled') + ` (↓ 0%)\n`);
        puts(chalk.bold('─'.repeat(barLength)));
        puts('\n');
    }
}

export default windowsPackager;