// Perfect Pascal for VS Code: starts 'ppc --lsp' (errors while typing, hover, go to definition,
// completion) and adds commands to check and run the current file.
const vscode = require('vscode');
const path = require('path');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

let client;
let output;

function ppcPath() {
  return vscode.workspace.getConfiguration('ppc').get('path') || 'ppc';
}

async function startClient() {
  const serverOptions = { command: ppcPath(), args: ['--lsp'], transport: TransportKind.stdio };
  const clientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'perfectpascal' },
      { scheme: 'file', language: 'perfectpascal-braces' }
    ],
    outputChannel: output,
  };
  client = new LanguageClient('ppc', 'Perfect Pascal', serverOptions, clientOptions);
  try {
    await client.start();
  } catch (err) {
    vscode.window.showErrorMessage(
      `Perfect Pascal: could not start '${ppcPath()} --lsp' (${err.message}). Set "ppc.path" to the compiler.`);
  }
}

function currentFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || !['perfectpascal', 'perfectpascal-braces'].includes(editor.document.languageId)) {
    vscode.window.showWarningMessage('Perfect Pascal: open a .pas or .pps file first.');
    return undefined;
  }
  return editor.document;
}

function terminal() {
  const existing = vscode.window.terminals.find((t) => t.name === 'Perfect Pascal');
  return existing || vscode.window.createTerminal('Perfect Pascal');
}

function quote(s) {
  return "'" + s.replace(/'/g, "'\\''") + "'";
}

async function runFile() {
  const doc = currentFile();
  if (!doc) return;
  await doc.save();
  const file = doc.fileName;
  const exe = path.join(path.dirname(file), path.basename(file, path.extname(file)));
  const extra = vscode.workspace.getConfiguration('ppc').get('runArguments') || '';
  const t = terminal();
  t.show();
  t.sendText(`${quote(ppcPath())} ${extra} -o ${quote(exe)} ${quote(file)} && ${quote(exe)}`);
}

async function checkFile() {
  const doc = currentFile();
  if (!doc) return;
  await doc.save();
  const t = terminal();
  t.show();
  t.sendText(`${quote(ppcPath())} --check ${quote(doc.fileName)} && echo 'no errors'`);
}

async function activate(context) {
  output = vscode.window.createOutputChannel('Perfect Pascal');
  context.subscriptions.push(
    output,
    vscode.commands.registerCommand('ppc.runFile', runFile),
    vscode.commands.registerCommand('ppc.checkFile', checkFile),
    vscode.commands.registerCommand('ppc.restartServer', async () => {
      if (client) await client.stop();
      await startClient();
    }),
    vscode.workspace.onDidChangeConfiguration(async (e) => {
      if (e.affectsConfiguration('ppc.path')) {
        if (client) await client.stop();
        await startClient();
      }
    }),
  );
  await startClient();
}

function deactivate() {
  return client ? client.stop() : undefined;
}

module.exports = { activate, deactivate };
