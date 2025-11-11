#!/usr/bin/env node
import { Builder } from './src/builder';

if (require.main === module) {
    const builder = new Builder({});
    builder.main();
}