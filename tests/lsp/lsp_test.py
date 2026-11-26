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
caps = r['result']['capabilities']
check(r.get('id') == 1 and caps['textDocumentSync']['change'] == 1 and caps['hoverProvider'] and caps['definitionProvider'], 'initialize: full text sync, hover, definition')
check(caps.get('referencesProvider') and caps.get('renameProvider', {}).get('prepareProvider'), 'initialize: references, rename with prepareRename')
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
hovertext = ("program H;\ntype TBox = class Size: Integer; function Area(f: Real): Real; const; end;\n"
             "function TBox.Area(f: Real): Real; const; begin Result := Size * f; end;\n"
             "val u := 1;\nbegin\n  vao b := TBox.Create;\n  begin\n    WriteLn('ü', u, b.Area(2.0), b.Size);\n  end;\nend.\n")
send({'jsonrpc': '2.0', 'method': 'textDocument/didChange',
      'params': {'textDocument': {'uri': uri, 'version': 4}, 'contentChanges': [{'text': hovertext}]}})
check(recv()['params']['diagnostics'] == [], 'hover program has no errors')
def hover(line, ch):
    send({'jsonrpc': '2.0', 'id': 9, 'method': 'textDocument/hover',
          'params': {'textDocument': {'uri': uri}, 'position': {'line': line, 'character': ch}}})
    r = recv()
    return r['result']['contents']['value'] if r.get('result') else None
check(r'Area(f: Real): Real; const' in (hover(7, 22) or ''), 'hover on a method call shows its signature (after a non-ASCII string)')
check('(field of TBox) Size: Integer' in (hover(7, 35) or ''), 'hover on a field')
check('val u: Integer' in (hover(7, 17) or ''), 'hover on a variable')
check(hover(7, 3) is None, 'no hover on spaces')

def definition(u, line, ch):
    send({'jsonrpc': '2.0', 'id': 11, 'method': 'textDocument/definition',
          'params': {'textDocument': {'uri': u}, 'position': {'line': line, 'character': ch}}})
    return recv().get('result')
loc = definition(uri, 7, 35)
check(loc and loc['uri'] == uri and loc['range']['start'] == {'line': 1, 'character': 18}, 'definition of a field in the same file')
loc = definition(uri, 7, 17)
check(loc and loc['range']['start'] == {'line': 3, 'character': 4}, 'definition of a variable')
check(definition(uri, 7, 5) is None, 'no definition for a built-in (WriteLn)')
useuri = 'file://' + os.path.join(here, 'lspuse2.pas')
send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
      'params': {'textDocument': {'uri': useuri, 'languageId': 'pascal', 'version': 1,
                                  'text': "program U;\nimport lspmod2;\nbegin\n  WriteLn(Half(4));\nend.\n"}}})
check(recv()['params']['diagnostics'] == [], 'program using a module has no errors')
loc = definition(useuri, 3, 11)
check(loc and loc['uri'].endswith('/lspmod2.pas') and loc['range']['start'] == {'line': 2, 'character': 9},
      'definition in an imported module')

def request(method, u, line, ch, **extra):
    params = {'textDocument': {'uri': u}, 'position': {'line': line, 'character': ch}}
    params.update(extra)
    send({'jsonrpc': '2.0', 'id': 13, 'method': method, 'params': params})
    return recv()
refs = request('textDocument/references', useuri, 3, 11, context={'includeDeclaration': True}).get('result') or []
where = sorted((x['uri'].rsplit('/', 1)[1], x['range']['start']['line'], x['range']['start']['character'], x['range']['end']['character']) for x in refs)
check(where == [('lspmod2.pas', 2, 9, 13), ('lspmod2.pas', 4, 9, 13), ('lspuse2.pas', 3, 10, 14)],
      'references of Half: interface, implementation (module) and the call (%s)' % where)
r = request('textDocument/prepareRename', useuri, 3, 12).get('result')
check(r and r['placeholder'] == 'Half' and r['range']['start'] == {'line': 3, 'character': 10}, 'prepareRename gives the name and its range')
edit = request('textDocument/rename', useuri, 3, 12, newName='Halve').get('result') or {}
changes = {u.rsplit('/', 1)[1]: [e['range']['start']['line'] for e in v] for u, v in edit.get('changes', {}).items()}
check(changes == {'lspmod2.pas': [2, 4], 'lspuse2.pas': [3]} and all(e['newText'] == 'Halve' for v in edit['changes'].values() for e in v),
      'rename edits the module and the program (%s)' % changes)
check('keyword' in request('textDocument/rename', useuri, 3, 12, newName='begin').get('error', {}).get('message', ''),
      'rename to a keyword is refused')
send({'jsonrpc': '2.0', 'method': 'textDocument/didChange',
      'params': {'textDocument': {'uri': useuri, 'version': 2},
                 'contentChanges': [{'text': "program U;\nimport std;\nbegin\n  WriteLn(Trim(' x '));\nend.\n"}]}})
recv()
check('standard library' in request('textDocument/prepareRename', useuri, 3, 11).get('error', {}).get('message', ''),
      'names of the standard library cannot be renamed')
check('error' in request('textDocument/prepareRename', useuri, 3, 4), 'a built-in (WriteLn) cannot be renamed')

def complete(text, line, ch):
    send({'jsonrpc': '2.0', 'method': 'textDocument/didChange',
          'params': {'textDocument': {'uri': uri, 'version': 9}, 'contentChanges': [{'text': text}]}})
    recv()  # the diagnostics of the unfinished text
    send({'jsonrpc': '2.0', 'id': 12, 'method': 'textDocument/completion',
          'params': {'textDocument': {'uri': uri}, 'position': {'line': line, 'character': ch}}})
    return {i['label']: i for i in recv()['result']['items']}
prog = ("program C;\ntype TColor = (Red, Green, Blue);\n  TBox = class private FHidden: Integer; public Size: Integer; "
        "function Area: Integer; end;\nfunction TBox.Area: Integer; begin Result := Size; end;\n"
        "val total := 0;\nbegin\n  vao b := TBox.Create;\n  begin\n    WriteLn(b.Si\n  end;\nend.\n")
items = complete(prog, 8, 16)
check('Size' in items and 'Area' not in items and items['Size']['kind'] == 5, 'members after b. with the prefix Si (unfinished line)')
items = complete(prog.replace('WriteLn(b.Si', 'WriteLn(b.'), 8, 14)
check({'Size', 'Area', 'Free'} <= set(items) and 'FHidden' not in items, 'all public members after b. (private ones hidden)')
items = complete(prog.replace('WriteLn(b.Si', 'tot'), 8, 7)
check('total' in items and 'WriteLn' not in items, 'names in scope with a prefix')
items = complete(prog.replace('WriteLn(b.Si', 'Wr'), 8, 6)
check('WriteLn' in items and 'Write' in items and items['WriteLn']['kind'] == 3, 'built-ins are offered')
items = complete(prog.replace('WriteLn(b.Si', 'WriteLn(TColor.'), 8, 19)
check(set(items) == {'Red', 'Green', 'Blue'}, 'the values of an enumeration after TColor.')
items = complete(prog.replace('    WriteLn(b.Si', '    b.Area; s'), 8, 15)
check('Self' not in items and 'Size' not in items, 'no Self members outside methods')

send({'jsonrpc': '2.0', 'method': 'textDocument/didClose', 'params': {'textDocument': {'uri': uri}}})
check(recv()['params']['diagnostics'] == [], 'diagnostics cleared on close')

send({'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown'})
check(recv().get('id') == 2, 'shutdown answered')
send({'jsonrpc': '2.0', 'method': 'exit'})
check(proc.wait(timeout=5) == 0, 'exit code 0')
print('--- lsp: %d failed' % len(failed))
sys.exit(1 if failed else 0)
