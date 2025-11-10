import * as fs from "fs";
import * as path from "path";


export function getArg(key: string): string | null {
    const argv = process.argv;
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] === key && i + 1 < argv.length) {
            return argv[i + 1];
        }
    }
    return null;
}


export function getCurrentTimeString(): string {
    const now = new Date();
    const pad = (n: number) => n.toString().padStart(2, "0");
    return `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
}


export function getAllFiles(dir: string, baseDir: string = dir): string[] {
    let results: string[] = [];

    const list = fs.readdirSync(dir);
    for (const file of list) {
        const fullPath = path.join(dir, file);
        const stat = fs.statSync(fullPath);

        if (stat.isDirectory()) {
            results = results.concat(getAllFiles(fullPath, baseDir));
        } else {
            const relativePath = path.relative(baseDir, fullPath);
            results.push(relativePath);
        }
    }
    return results;
}

export function getCurrentPlatformNam() {
    switch (process.platform) {
        case "win32":
            return "windows";
        case "darwin":
            return "macos";
        case "linux":
            return "linux";
        default:
            throw new Error(`Unsupported platform: ${process.platform}`);
    }
}