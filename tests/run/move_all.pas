program MoveAll;
{ Objekte zwischen val (Stack), vao (Heap) und var (Ausleihe) bewegen }
type
  TK = class
    N: Integer;
    Teil: optional TK;
    constructor Create(AN: Integer);
    destructor Destroy; override;
    function Clone: TK; const; auto;
  end;
  TL = class(TK) end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  Destroy ', N); inherited Destroy; end;
function Mach(n: Integer): TK; begin Result := TK.Create(n); end;

procedure Alle;
vao h := TK.Create(1);
val s := TK.Create(2);
begin
  h.Teil := TK.Create(10);
  val a := h;                          { vao -> val: Heapobjekt wandert auf den Stack }
  begin
    if a.Teil is some t then WriteLn('a = ', a.N, ' / ', t.N);
    vao b := a;                         { val -> vao: wieder auf den Heap }
    begin
      WriteLn('b = ', b.N);
    end;
  end;
  val c := Mach(3);                    { Funktionsergebnis direkt auf den Stack }
  val d := c;                          { val -> val }
  var r: TK := d;                      { val -> var: Ausleihe }
  begin
    WriteLn('r = ', r.N, ', s = ', s.N);
    val e := r.Clone;                  { var -> val: nur als Kopie }
    begin
      WriteLn('e = ', e.N);
    end;
  end;
end;

begin
  Alle;
end.
