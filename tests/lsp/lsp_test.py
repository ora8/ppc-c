#!/usr/bin/env python3
"""Drives 'ppc --lsp' like an editor: open a file with errors, fix it, close; checks the diagnostics."""
import json, os, subprocess, sys

ppc = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'ppc')
here = os.path.abspath(os.path.dirname(__file__))
path = os.path.join(here, 'doc with space.pas')
uri = 'file://' + path.replace(' ', '%20')

proc = subprocess.Popen([ppc, '--lsp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def send(msg):
    body = json.dumps(msg).encode()
    proc.stdin.write(b'Content-Length: %d\r\n\r\n' % len(body) + body)
    proc.stdin.flush()

def recv():
    length = None
    while True:
        line = proc.stdout.readline()
        if not line:
            raise SystemExit('server closed the connection')
        if line.lower().startswith(b'content-length:'):
            length = int(line.split(b':')[1])
        if line in (b'\r\n', b'\n'):
            break
    return json.loads(proc.stdout.read(length))

failed = []
def check(cond, what):
    print(('ok    ' if cond else 'FAIL  ') + what)
    if not cond:
        failed.append(what)

send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'rootUri': None, 'capabilities': {}}})
r = recv()
check(r.get('id') == 1 and r['result']['capabilities']['textDocumentSync']['change'] == 1, 'initialize: full text sync')
send({'jsonrpc': '2.0', 'method': 'initialized', 'params': {}})

bad = "program T;\nval x: Integer := 1;\nbegin\n  x := 'ü' + 'a';\n  WriteLn(y);\nend.\n"
send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
      'params': {'textDocument': {'uri': uri, 'languageId': 'pascal', 'version': 1, 'text': bad}}})
d = recv()
diags = d['params']['diagnostics']
check(d['method'] == 'textDocument/publishDiagnostics' and d['params']['uri'] == uri, 'diagnostics for the opened file')
check(len(diags) == 2, 'two errors reported (%d)' % len(diags))
check(diags and diags[0]['range']['start'] == {'line': 3, 'character': 11}, 'first error at the + (UTF-16 column after the ü)')
check(any('"y"' in x['message'] for x in diags), 'unknown identifier y reported')
check(all(x['severity'] == 1 for x in diags), 'errors have severity 1')

borrow = ("program T;\ntype TK = class end;\nprocedure P;\nvao k := TK.Create;\nbegin\n  k.Free;\n  k.Free;\nend;\nbegin end.\n")
send({'jsonrpc': '2.0', 'method': 'textDocument/didChange',
      'params': {'textDocument': {'uri': uri, 'version': 2}, 'contentChanges': [{'text': borrow}]}})
diags = recv()['params']['diagnostics']
check(len(diags) == 1 and diags[0]['message'].startswith('Borrow error:') and diags[0]['range']['start']['line'] == 6,
      'borrow error on line 7')

good = "program T;\nbegin\n  WriteLn('fine');\nend.\n"
send({'jsonrpc': '2.0', 'method': 'textDocument/didChange',
      'params': {'textDocument': {'uri': uri, 'version': 3}, 'contentChanges': [{'text': good}]}})
check(recv()['params']['diagnostics'] == [], 'no diagnostics after the fix')

modpath = os.path.join(here, 'lspmod.pas')
moduri = 'file://' + os.path.join(here, 'lspuse.pas')
send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
      'params': {'textDocument': {'uri': moduri, 'languageId': 'pascal', 'version': 1,
                                  'text': "program U;\nimport lspmod;\nbegin\n  WriteLn(Twice(2));\nend.\n"}}})
diags = recv()['params']['diagnostics']
check(len(diags) == 1 and 'lspmod.pas' in diags[0]['message'] and diags[0]['range']['start']['line'] == 0,
      'an error in an imported module is shown at the top of the importing file')
send({'jsonrpc': '2.0', 'id': 7, 'method': 'textDocument/hover', 'params': {}})
r = recv()
check(r.get('id') == 7 and r['error']['code'] == -32601, 'unsupported request answered with an error')

send({'jsonrpc': '2.0', 'method': 'textDocument/didClose', 'params': {'textDocument': {'uri': uri}}})
check(recv()['params']['diagnostics'] == [], 'diagnostics cleared on close')

send({'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown'})
check(recv().get('id') == 2, 'shutdown answered')
send({'jsonrpc': '2.0', 'method': 'exit'})
check(proc.wait(timeout=5) == 0, 'exit code 0')
print('--- lsp: %d failed' % len(failed))
sys.exit(1 if failed else 0)
