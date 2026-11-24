program StackToHeap;
{ Stackobjekt auf den Heap verschieben: vao h := k, Result := k, vao-Parameter (wie Box::new) }
type
  TK = class
    N: Integer;
    Teil: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TListe = class
    Kopf: optional TK;
    procedure Add(vao k: TK);
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;
procedure TListe.Add(vao k: TK); begin k.Teil := Take(Kopf); Kopf := k; end;

vao global: optional TK := none;

function Baue(n: Integer): TK;
val k := TK.Create(n);                 { auf dem Stack aufbauen ... }
begin
  k.Teil := TK.Create(n + 100);
  Result := k;                          { ... und auf den Heap verschieben }
end;

procedure P(flag: Boolean);
val a := TK.Create(1);
val b := TK.Create(2);
vao l := TListe.Create;
begin
  vao h := a;                           { a wandert auf den Heap }
  begin
    WriteLn('h = ', h.N);
    global := h;
  end;
  if flag then l.Add(b);                { b nur auf einem Pfad verschoben }
  WriteLn('P Ende');
end;

begin
  P(True);
  P(False);
  vao x := Baue(5);
  begin
    if x.Teil is some t then WriteLn('Baue: ', x.N, ' / ', t.N);
  end;
  if global is some g then WriteLn('global: ', g.N);
end.
