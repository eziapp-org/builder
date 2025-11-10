#!/usr/bin/env node
import { Builder } from './builder';

if (require.main === module) {
    const builder = new Builder({});
    builder.main();
}