#!/usr/bin/env node

/**
 * Simple HTTP server for OCP WASM testing
 * Serves WASM files with proper MIME types and CORS headers
 */

const express = require('express');
const path = require('path');
const chalk = require('chalk');

class TestServer {
    constructor(options = {}) {
        this.port = options.port || 0; // 0 = auto-assign port
        this.verbose = options.verbose || false;
        this.app = express();
        this.server = null;
        this.actualPort = null;
    }

    log(message, level = 'info') {
        if (!this.verbose && level === 'debug') return;

        switch (level) {
            case 'error':
                console.error(chalk.red(`ERROR: ${message}`));
                break;
            case 'warn':
                console.warn(chalk.yellow(`WARN: ${message}`));
                break;
            case 'success':
                console.log(chalk.green(`SUCCESS: ${message}`));
                break;
            default:
                console.log(`INFO: ${message}`);
        }
    }

    setupMiddleware() {
        // Enable CORS for all origins and add SharedArrayBuffer headers
        this.app.use((req, res, next) => {
            res.header('Access-Control-Allow-Origin', '*');
            res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
            res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');

            // Required for SharedArrayBuffer (needed for AudioWorklet)
            res.header('Cross-Origin-Opener-Policy', 'same-origin');
            res.header('Cross-Origin-Embedder-Policy', 'require-corp');

            next();
        });

        // Set proper MIME types for WASM files
        this.app.use((req, res, next) => {
            if (req.path.endsWith('.wasm')) {
                res.type('application/wasm');
            } else if (req.path.endsWith('.data')) {
                res.type('application/octet-stream');
            } else if (req.path.endsWith('.js')) {
                res.type('application/javascript');
            } else if (req.path.endsWith('.mod')) {
                res.type('application/octet-stream');
            }
            next();
        });

        // Add cache control headers for development
        this.app.use((req, res, next) => {
            res.header('Cache-Control', 'no-cache, no-store, must-revalidate');
            res.header('Pragma', 'no-cache');
            res.header('Expires', '0');
            next();
        });

        // Log requests in verbose mode
        if (this.verbose) {
            this.app.use((req, res, next) => {
                this.log(`${req.method} ${req.path}`, 'debug');
                next();
            });
        }

        // Serve static files from tests directory
        this.app.use(express.static(__dirname, {
            dotfiles: 'allow',
            index: false
        }));

        // Serve WASM files from parent directory
        this.app.use('/wasm', express.static(path.join(__dirname, '..'), {
            dotfiles: 'allow'
        }));

        // Root redirects to test runner
        this.app.get('/', (req, res) => {
            res.redirect('/test-runner.html');
        });

        // API endpoint for health check
        this.app.get('/api/health', (req, res) => {
            res.json({
                status: 'ok',
                timestamp: new Date().toISOString(),
                port: this.actualPort
            });
        });

        // 404 handler
        this.app.use((req, res) => {
            res.status(404).json({ error: 'File not found', path: req.path });
        });
    }

    async start() {
        return new Promise((resolve, reject) => {
            this.setupMiddleware();

            this.server = this.app.listen(this.port, (err) => {
                if (err) {
                    this.log(`Failed to start server: ${err.message}`, 'error');
                    reject(err);
                    return;
                }

                this.actualPort = this.server.address().port;
                this.log(`Test server started on port ${this.actualPort}`, 'success');

                if (this.verbose) {
                    this.log(`URLs:`, 'info');
                    this.log(`  Test Runner: http://localhost:${this.actualPort}/test-runner.html`);
                    this.log(`  Canvas Tests: http://localhost:${this.actualPort}/test-canvas-rendering.html`);
                    this.log(`  Audio Tests: http://localhost:${this.actualPort}/test-sound-output.html`);
                    this.log(`  WASM Files: http://localhost:${this.actualPort}/wasm/build/ocp.js`);
                    this.log(`  Health Check: http://localhost:${this.actualPort}/api/health`);
                }

                resolve(this.actualPort);
            });
        });
    }

    async stop() {
        return new Promise((resolve) => {
            if (this.server) {
                this.server.close(() => {
                    this.log('Test server stopped', 'success');
                    resolve();
                });
            } else {
                resolve();
            }
        });
    }

    getPort() {
        return this.actualPort;
    }

    getBaseUrl() {
        return `http://localhost:${this.actualPort}`;
    }
}

// CLI execution
async function main() {
    const args = process.argv.slice(2);
    const options = {
        port: parseInt(args.find(arg => arg.startsWith('--port='))?.split('=')[1]) || 0,
        verbose: args.includes('--verbose')
    };

    const server = new TestServer(options);

    try {
        await server.start();

        // Keep server running until interrupted
        process.on('SIGINT', async () => {
            console.log('\nShutting down server...');
            await server.stop();
            process.exit(0);
        });

        process.on('SIGTERM', async () => {
            await server.stop();
            process.exit(0);
        });

        // In CLI mode, keep the server running
        if (require.main === module) {
            console.log('Press Ctrl+C to stop the server');
        }

    } catch (error) {
        console.error(chalk.red(`Failed to start server: ${error.message}`));
        process.exit(1);
    }
}

// Export for use as module
module.exports = TestServer;

// Run if called directly
if (require.main === module) {
    main();
}
