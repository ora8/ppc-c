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

// ppc.modulePaths: directories for 'import' (relative ones belong to the first workspace folder)
function modulePaths() {
  const dirs = vscode.workspace.getConfiguration('ppc').get('modulePaths') || [];
  const root = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
    ? vscode.workspace.workspaceFolders[0].uri.fsPath : process.cwd();
  return dirs.filter((d) => d).map((d) => (path.isAbsolute(d) ? d : path.join(root, d))).join(':');
}
// ppc.sharedObjects: .so files that 'Run File' links (their C functions are declared 'external')
function sharedObjectArgs() {
  const sos = vscode.workspace.getConfiguration('ppc').get('sharedObjects') || [];
  const root = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
    ? vscode.workspace.workspaceFolders[0].uri.fsPath : process.cwd();
  const list = sos.filter((d) => d).map((d) => (path.isAbsolute(d) ? d : path.join(root, d))).join(';');
  return list ? ['--shared-objects=' + list] : [];
}
// ppc.staticLibraries: .a files that 'Run File' links
function staticLibraryArgs() {
  const libs = vscode.workspace.getConfiguration('ppc').get('staticLibraries') || [];
  const root = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0]
    ? vscode.workspace.workspaceFolders[0].uri.fsPath : process.cwd();
  const list = libs.filter((d) => d).map((d) => (path.isAbsolute(d) ? d : path.join(root, d))).join(';');
  return list ? ['--static-libraries=' + list] : [];
}
function modulePathArgs() {
  const mp = modulePaths();
  return mp ? ['--module-paths', mp] : [];
}

async function startClient() {
  const serverOptions = { command: ppcPath(), args: ['--lsp', ...modulePathArgs()], transport: TransportKind.stdio };
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
  const mp = [...modulePathArgs(), ...sharedObjectArgs(), ...staticLibraryArgs()].map(quote).join(' ');
  t.sendText(`${quote(ppcPath())} ${extra} ${mp} -o ${quote(exe)} ${quote(file)} && ${quote(exe)}`);
}

async function checkFile() {
  const doc = currentFile();
  if (!doc) return;
  await doc.save();
  const t = terminal();
  t.show();
  const mp = modulePathArgs().map(quote).join(' ');
  t.sendText(`${quote(ppcPath())} --check ${mp} ${quote(doc.fileName)} && echo 'no errors'`);
}

// ---- Toggle Declaration/Implementation (Ctrl+Shift+Up/Down, as in Delphi) ----
const LANGS = ['perfectpascal', 'perfectpascal-braces'];
function firstLocation(r) {
  const l = Array.isArray(r) ? r[0] : r;
  if (!l) return undefined;
  return l.targetUri ? { uri: l.targetUri, range: l.targetSelectionRange || l.targetRange } : { uri: l.uri, range: l.range };
}
// on a routine header ('function TBox.Area(...)') the position of the routine's name, else the cursor
function namePosition(doc, pos) {
  const text = doc.lineAt(pos.line).text;
  const m = /^(\s*(?:class\s+)?(?:function|procedure|constructor|destructor|operator)\s+)([A-Za-z_][\w.<>, ]*)/.exec(text);
  if (!m || (pos.character >= m[1].length && pos.character < m[1].length + m[2].length && /\w/.test(text[pos.character] || ''))) return pos;
  const qualified = m[2].replace(/<[^>]*>/g, (g) => ' '.repeat(g.length)).trimEnd();
  const last = qualified.lastIndexOf('.') + 1;
  return new vscode.Position(pos.line, m[1].length + last);
}
async function toggleImplementation() {
  const editor = vscode.window.activeTextEditor;
  if (!editor || !LANGS.includes(editor.document.languageId)) return;
  const uri = editor.document.uri;
  const pos = namePosition(editor.document, editor.selection.active);
  const here = (l) => l && l.uri.toString() === uri.toString() && l.range.start.line === pos.line;
  let target = firstLocation(await vscode.commands.executeCommand('vscode.executeImplementationProvider', uri, pos));
  if (!target || here(target)) /* at an implementation: back to the declaration */
    target = firstLocation(await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, pos));
  if (!target || here(target)) {
    vscode.window.setStatusBarMessage('Perfect Pascal: no declaration or implementation of a routine here', 3000);
    return;
  }
  const ed = await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(target.uri));
  ed.selection = new vscode.Selection(target.range.start, target.range.start);
  ed.revealRange(target.range, vscode.TextEditorRevealType.InCenterIfOutsideViewport);
}

async function activate(context) {
  output = vscode.window.createOutputChannel('Perfect Pascal');
  context.subscriptions.push(
    output,
    vscode.commands.registerCommand('ppc.runFile', runFile),
    vscode.commands.registerCommand('ppc.checkFile', checkFile),
    vscode.commands.registerCommand('ppc.toggleImplementation', toggleImplementation),
    vscode.commands.registerCommand('ppc.restartServer', async () => {
      if (client) await client.stop();
      await startClient();
    }),
    vscode.workspace.onDidChangeConfiguration(async (e) => {
      if (e.affectsConfiguration('ppc.path') || e.affectsConfiguration('ppc.modulePaths')) {
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
