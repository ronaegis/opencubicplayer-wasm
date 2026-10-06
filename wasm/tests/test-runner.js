#!/usr/bin/env node

/**
 * OCP WASM Complete Test Suite - CLI Runner
 * Orchestrates both canvas and audio tests with comprehensive reporting
 */

const { Command } = require('commander');
const chalk = require('chalk');
const path = require('path');
const fs = require('fs');

const CanvasTestRunner = require('./test-canvas');
const AudioTestRunner = require('./test-audio');

class OCPTestSuite {
    constructor(options = {}) {
        this.options = {
            headless: options.headless !== false,
            verbose: options.verbose || false,
            ci: options.ci || false,
            timeout: options.timeout || 30000,
            skipCanvas: options.skipCanvas || false,
            skipAudio: options.skipAudio || false,
            enableAudio: options.enableAudio || false,
            outputFile: options.outputFile || null
        };

        this.results = {
            canvas: null,
            audio: null,
            summary: null
        };
    }

    log(message, level = 'info') {
        if (!this.options.verbose && level === 'debug') return;

        const timestamp = new Date().toISOString();
        const prefix = this.options.ci ? '' : `[${timestamp}] `;

        switch (level) {
            case 'error':
                console.error(chalk.red(`${prefix}ERROR: ${message}`));
                break;
            case 'warn':
                console.warn(chalk.yellow(`${prefix}WARN: ${message}`));
                break;
            case 'success':
                console.log(chalk.green(`${prefix}SUCCESS: ${message}`));
                break;
            case 'info':
                console.log(chalk.blue(`${prefix}INFO: ${message}`));
                break;
            case 'debug':
                console.log(chalk.gray(`${prefix}DEBUG: ${message}`));
                break;
            default:
                console.log(`${prefix}${message}`);
        }
    }

    async runCanvasTests() {
        if (this.options.skipCanvas) {
            this.log('Skipping canvas tests as requested');
            return null;
        }

        this.log('Running canvas rendering tests...');

        const canvasRunner = new CanvasTestRunner({
            headless: this.options.headless,
            verbose: this.options.verbose,
            ci: false, // We'll handle CI output ourselves
            timeout: this.options.timeout
        });

        try {
            const setupOk = await canvasRunner.setup();
            if (!setupOk) {
                throw new Error('Failed to setup canvas test runner');
            }

            const report = await canvasRunner.runAllTests();
            await canvasRunner.cleanup();

            this.log(`Canvas tests completed: ${report.passed}/${report.total} passed (${report.successRate}%)`, 'success');
            return report;

        } catch (error) {
            this.log(`Canvas tests failed: ${error.message}`, 'error');
            await canvasRunner.cleanup();
            throw error;
        }
    }

    async runAudioTests() {
        if (this.options.skipAudio) {
            this.log('Skipping audio tests as requested');
            return null;
        }

        this.log('Running audio output tests...');

        const audioRunner = new AudioTestRunner({
            headless: this.options.headless,
            verbose: this.options.verbose,
            ci: false, // We'll handle CI output ourselves
            timeout: this.options.timeout,
            skipAudioPlayback: !this.options.enableAudio
        });

        try {
            const setupOk = await audioRunner.setup();
            if (!setupOk) {
                throw new Error('Failed to setup audio test runner');
            }

            const report = await audioRunner.runAllTests();
            await audioRunner.cleanup();

            this.log(`Audio tests completed: ${report.passed}/${report.total} passed (${report.successRate}%)`, 'success');
            return report;

        } catch (error) {
            this.log(`Audio tests failed: ${error.message}`, 'error');
            await audioRunner.cleanup();
            throw error;
        }
    }

    async runAllTests() {
        this.log('Starting OCP WASM complete test suite...', 'info');

        const startTime = Date.now();

        try {
            // Run tests in parallel if both are enabled
            if (!this.options.skipCanvas && !this.options.skipAudio) {
                this.log('Running canvas and audio tests in parallel...');
                const [canvasReport, audioReport] = await Promise.all([
                    this.runCanvasTests(),
                    this.runAudioTests()
                ]);

                this.results.canvas = canvasReport;
                this.results.audio = audioReport;
            } else {
                // Run tests sequentially
                this.results.canvas = await this.runCanvasTests();
                this.results.audio = await this.runAudioTests();
            }

            const endTime = Date.now();
            const duration = ((endTime - startTime) / 1000).toFixed(2);

            this.results.summary = this.generateSummary(duration);
            this.generateReport();

            return this.results;

        } catch (error) {
            this.log(`Test suite execution failed: ${error.message}`, 'error');
            throw error;
        }
    }

    generateSummary(duration) {
        const canvasResults = this.results.canvas || { passed: 0, total: 0, failed: 0 };
        const audioResults = this.results.audio || { passed: 0, total: 0, failed: 0 };

        const totalPassed = canvasResults.passed + audioResults.passed;
        const totalTests = canvasResults.total + audioResults.total;
        const totalFailed = canvasResults.failed + audioResults.failed;
        const overallSuccessRate = totalTests > 0 ? ((totalPassed / totalTests) * 100).toFixed(1) : 0;

        return {
            duration,
            totalTests,
            totalPassed,
            totalFailed,
            overallSuccessRate: parseFloat(overallSuccessRate),
            canvasSummary: canvasResults,
            audioSummary: audioResults
        };
    }

    generateReport() {
        const summary = this.results.summary;

        if (this.options.ci) {
            // CI-friendly JSON output
            const ciReport = {
                timestamp: new Date().toISOString(),
                duration: summary.duration,
                overall: {
                    total: summary.totalTests,
                    passed: summary.totalPassed,
                    failed: summary.totalFailed,
                    successRate: summary.overallSuccessRate
                },
                canvas: this.results.canvas,
                audio: this.results.audio,
                exitCode: summary.totalFailed > 0 ? 1 : 0
            };

            console.log(JSON.stringify(ciReport, null, 2));

            // Write to file if specified
            if (this.options.outputFile) {
                try {
                    fs.writeFileSync(this.options.outputFile, JSON.stringify(ciReport, null, 2));
                    this.log(`Test results written to ${this.options.outputFile}`, 'success');
                } catch (error) {
                    this.log(`Failed to write results to file: ${error.message}`, 'error');
                }
            }

        } else {
            // Human-readable output
            console.log('\n' + '='.repeat(60));
            console.log(chalk.bold.cyan('    OpenCubicPlayer WASM Test Suite Results'));
            console.log('='.repeat(60));
            console.log(`${chalk.gray('Test Duration:')} ${summary.duration}s`);
            console.log(`${chalk.gray('Total Tests:')} ${summary.totalTests}`);
            console.log(`${chalk.green('Passed:')} ${summary.totalPassed}`);
            console.log(`${chalk.red('Failed:')} ${summary.totalFailed}`);
            console.log(`${chalk.yellow('Success Rate:')} ${summary.overallSuccessRate}%`);

            // Canvas results
            if (this.results.canvas) {
                console.log('\n' + chalk.bold('Canvas Rendering Tests:'));
                console.log(`  Tests: ${this.results.canvas.total}`);
                console.log(`  Passed: ${chalk.green(this.results.canvas.passed)}`);
                console.log(`  Failed: ${chalk.red(this.results.canvas.failed)}`);
                console.log(`  Success Rate: ${this.results.canvas.successRate}%`);
            }

            // Audio results
            if (this.results.audio) {
                console.log('\n' + chalk.bold('Audio Output Tests:'));
                console.log(`  Tests: ${this.results.audio.total}`);
                console.log(`  Passed: ${chalk.green(this.results.audio.passed)}`);
                console.log(`  Failed: ${chalk.red(this.results.audio.failed)}`);
                console.log(`  Success Rate: ${this.results.audio.successRate}%`);
            }

            // Show failed tests
            const allFailedTests = [];
            if (this.results.canvas) {
                allFailedTests.push(...this.results.canvas.results.filter(r => !r.passed).map(r => ({ ...r, suite: 'Canvas' })));
            }
            if (this.results.audio) {
                allFailedTests.push(...this.results.audio.results.filter(r => !r.passed).map(r => ({ ...r, suite: 'Audio' })));
            }

            if (allFailedTests.length > 0) {
                console.log('\n' + chalk.red.bold('Failed Tests:'));
                allFailedTests.forEach(test => {
                    console.log(`  ${chalk.red('✗')} ${chalk.gray(`[${test.suite}]`)} ${test.name}`);
                    console.log(`    ${chalk.gray(test.details)}`);
                });
            }

            console.log('\n' + '='.repeat(60));

            if (summary.totalFailed === 0) {
                console.log(chalk.green.bold('🎉 All tests passed! OCP WASM is working correctly.'));
            } else {
                console.log(chalk.red.bold(`❌ ${summary.totalFailed} test(s) failed. Please check the results above.`));
            }
        }
    }

    getExitCode() {
        return this.results.summary && this.results.summary.totalFailed > 0 ? 1 : 0;
    }
}

// CLI setup
const program = new Command();

program
    .name('ocp-wasm-test')
    .description('OpenCubicPlayer WASM test suite for canvas rendering and audio output')
    .version('1.0.0');

program
    .option('--headless', 'run in headless mode (default: true)', true)
    .option('--no-headless', 'run with visible browser')
    .option('--verbose', 'enable verbose logging')
    .option('--ci', 'CI mode with JSON output')
    .option('--timeout <ms>', 'test timeout in milliseconds', 30000)
    .option('--skip-canvas', 'skip canvas rendering tests')
    .option('--skip-audio', 'skip audio output tests')
    .option('--enable-audio', 'enable actual audio playback (not recommended for CI)')
    .option('--output-file <path>', 'write results to JSON file')
    .action(async (options) => {
        const testSuite = new OCPTestSuite(options);

        try {
            await testSuite.runAllTests();
            process.exit(testSuite.getExitCode());
        } catch (error) {
            console.error(chalk.red(`Test suite failed: ${error.message}`));
            process.exit(1);
        }
    });

// Subcommands
program
    .command('canvas')
    .description('Run only canvas rendering tests')
    .option('--headless', 'run in headless mode', true)
    .option('--verbose', 'enable verbose logging')
    .option('--ci', 'CI mode with JSON output')
    .action(async (options) => {
        const runner = new CanvasTestRunner(options);
        try {
            await runner.setup();
            const report = await runner.runAllTests();
            await runner.cleanup();
            process.exit(report.failed > 0 ? 1 : 0);
        } catch (error) {
            console.error(chalk.red(`Canvas tests failed: ${error.message}`));
            process.exit(1);
        }
    });

program
    .command('audio')
    .description('Run only audio output tests')
    .option('--headless', 'run in headless mode', true)
    .option('--verbose', 'enable verbose logging')
    .option('--ci', 'CI mode with JSON output')
    .option('--enable-audio', 'enable actual audio playback')
    .action(async (options) => {
        const runner = new AudioTestRunner(options);
        try {
            await runner.setup();
            const report = await runner.runAllTests();
            await runner.cleanup();
            process.exit(report.failed > 0 ? 1 : 0);
        } catch (error) {
            console.error(chalk.red(`Audio tests failed: ${error.message}`));
            process.exit(1);
        }
    });

// Export for use as module
module.exports = OCPTestSuite;

// Run if called directly
if (require.main === module) {
    program.parse();
}