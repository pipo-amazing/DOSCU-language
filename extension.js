const vscode = require("vscode");
const { spawn } = require("child_process");
const path = require("path");

const activeRuns = new Set();

function activate(context) {
  const output = vscode.window.createOutputChannel("DOSCU");

  const runCommand = vscode.commands.registerCommand("doscu.runFile", async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== "doscu") {
      vscode.window.showErrorMessage("Open a DOSCU (.doscu) file before running it.");
      return;
    }

    const document = editor.document;
    if (!(await document.save())) return;

    const extensionPath = context.extensionPath;
    const runnerPath = path.join(extensionPath, "run-doscu.py");
    const sourcePath = document.uri.fsPath;
    const configuredPython = vscode.workspace
      .getConfiguration("doscu")
      .get("pythonPath", "python3");

    if (typeof configuredPython !== "string" || !configuredPython.trim()) {
      vscode.window.showErrorMessage("Set DOSCU › Python Path to a Python 3 executable.");
      return;
    }

    output.clear();
    output.show(true);
    output.appendLine(`DOSCU: ${sourcePath}`);

    const pythonCandidates = configuredPython && configuredPython !== "python3"
      ? [configuredPython]
      : process.platform === "win32"
        ? ["python3", "python", "py"]
        : ["python3", "python"];
    startRunner(pythonCandidates, [runnerPath, sourcePath], extensionPath, output);
  });

  const stopCommand = vscode.commands.registerCommand("doscu.stopRun", () => {
    if (activeRuns.size === 0) {
      vscode.window.showInformationMessage("No DOSCU run is active.");
      return;
    }
    for (const child of activeRuns) child.kill();
    activeRuns.clear();
    output.appendLine("DOSCU stop requested.");
  });

  context.subscriptions.push(output, runCommand, stopCommand);
}

function startRunner(candidates, args, cwd, output) {
  const [python, ...remaining] = candidates;
  const child = spawn(python, [...remaining, ...args], { cwd, windowsHide: true });
  child.stdout.on("data", chunk => output.append(chunk.toString()));
  child.stderr.on("data", chunk => output.append(chunk.toString()));
  child.on("spawn", () => activeRuns.add(child));
  child.on("error", error => {
    if (error.code === "ENOENT" && candidates.length > 1) {
      startRunner(candidates.slice(1), args, cwd, output);
      return;
    }
    const message = error.code === "ENOENT"
      ? `Python executable not found: ${python}. Set doscu.pythonPath in Settings.`
      : `Could not start DOSCU: ${error.message}`;
    output.appendLine(message);
    vscode.window.showErrorMessage(message);
  });
  child.on("close", (code, signal) => {
    activeRuns.delete(child);
    if (!child.spawned) return;
    if (signal) output.appendLine(`DOSCU stopped (${signal}).`);
    else if (code === 0) output.appendLine("DOSCU finished.");
    else output.appendLine(`DOSCU exited with code ${code}.`);
  });
}

function deactivate() {
  for (const child of activeRuns) child.kill();
  activeRuns.clear();
}

module.exports = { activate, deactivate };
