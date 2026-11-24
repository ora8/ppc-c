program OhneZeiger;
{ Typische Zeiger-Aufgaben aus klassischem Pascal – in ppc ohne ^, @, New, Dispose
  und ohne nil:
    1. verkettete Liste             -> Heapobjekte mit 'optional' Nachfolger
    2. binaerer Suchbaum            -> optionale Teilbaeume, Auspacken mit 'is some'
    3. Werte "per Zeiger" aendern   -> var-Parameter
    4. Referenz auf ein Objekt     -> 'var' (Referenz ohne Besitz)
    5. Ergebnis, das fehlen kann    -> 'optional Integer' (wie Option<i32>) }
type
  { ---------------------------------------------------- 1. Liste }
  TKnoten = class
    Wert: Integer;
    Next: optional TKnoten;             { besitzt den Nachfolger; none = Listenende }
    constructor Create(W: Integer);
  end;

  TListe = class
    Kopf: optional TKnoten;
    Anzahl: Integer;
    procedure Vorne(W: Integer);
    procedure Umdrehen;
    function Summe: Integer;
    function Position(W: Integer): optional Integer;
    procedure Zeige;
  end;

  { ---------------------------------------------------- 2. Baum }
  TBaum = class
    Wert: Integer;
    Links, Rechts: optional TBaum;      { besitzt die Teilbaeume }
    constructor Create(W: Integer);
    procedure Einfuegen(W: Integer);
    function Enthaelt(W: Integer): Boolean;
    procedure InOrder;
    function Hoehe: Integer;
  end;

  { ---------------------------------------------------- 4. Referenz }
  TPunkt = class
    X, Y: Integer;
    constructor Create(AX, AY: Integer);
  end;

{ ======================================================= TKnoten / TListe }
constructor TKnoten.Create(W: Integer); begin inherited Create; Wert := W; end;

procedure TListe.Vorne(W: Integer);
vao
  k: TKnoten := TKnoten.Create(W);
begin
  
  k.Next := Take(Kopf);                 { alte Kette wandert hinter den neuen Knoten }
  Kopf := k;                            { Move: k besitzt danach nichts mehr }
  Inc(Anzahl);
end;

procedure TListe.Umdrehen;              { klassisches Zeiger-Umhaengen, nur mit Moves }
vao
  rest, neu: optional TKnoten := none;
begin
  neu := none;
  rest := Take(Kopf);
  while Take(rest) is some k do begin   { k besitzt den vordersten Knoten }
    rest := Take(k.Next);
    k.Next := neu;                      { vorne an die neue Liste haengen }
    neu := k;
  end;
  Kopf := neu;
end;

function TListe.Summe: Integer;
var p: optional TKnoten := none;       { Ausleihe zum Durchlaufen }
begin
  Result := 0;
  p := Kopf;
  while p is some k do begin            { wie: while let Some(k) = p }
    Result := Result + k.Wert;
    p := k.Next;
  end;
end;

function TListe.Position(W: Integer): optional Integer;
var p: optional TKnoten := none;
    val i: Integer := default;
begin
  Result := none;
  p := Kopf;
  i := 1;
  while p is some k do begin
    if k.Wert = W then Exit(i);         { Integer wird automatisch zu 'some i' }
    p := k.Next;
    Inc(i);
  end;
end;

procedure TListe.Zeige;
var p: optional TKnoten := none;
begin
  p := Kopf;
  while p is some k do begin Write(k.Wert, ' '); p := k.Next; end;
  WriteLn('(', Anzahl, ' Elemente, Summe ', Summe, ')');
end;


{ ======================================================= TBaum }
constructor TBaum.Create(W: Integer); begin inherited Create; Wert := W; end;

procedure TBaum.Einfuegen(W: Integer);
begin
  if W < Wert then begin
    if Links is some l then l.Einfuegen(W) else Links := TBaum.Create(W);
  end else if W > Wert then begin
    if Rechts is some r then r.Einfuegen(W) else Rechts := TBaum.Create(W);
  end;
end;

function TBaum.Enthaelt(W: Integer): Boolean;
begin
  Result := W = Wert;
  if (W < Wert) and (Links is some l) then Result := l.Enthaelt(W)
  else if (W > Wert) and (Rechts is some r) then Result := r.Enthaelt(W);
end;

procedure TBaum.InOrder;
begin
  if Links is some l then l.InOrder;
  Write(Wert, ' ');
  if Rechts is some r then r.InOrder;
end;

function TBaum.Hoehe: Integer;
val hl, hr: Integer := default;
begin
  hl := 0; hr := 0;
  if Links is some l then hl := l.Hoehe;
  if Rechts is some r then hr := r.Hoehe;
  if hl > hr then Result := hl + 1 else Result := hr + 1;
end;

{ ======================================================= 3. var-Parameter }
procedure Tausche(var A, B: Integer);   { statt Tausche(pa, pb: ^Integer) }
val t: Integer := default;
begin t := A; A := B; B := t; end;

{ ======================================================= 4. Referenz }
constructor TPunkt.Create(AX, AY: Integer); begin inherited Create; X := AX; Y := AY; end;

procedure Verschiebe(P: TPunkt; DX: Integer);   { bekommt eine Referenz, kein ^TPunkt }
begin P.X := P.X + DX; end;

vao
  liste := TListe.Create;             { vao: besitzt das Objekt }
  baum := TBaum.Create(50);
  pkt := TPunkt.Create(3, 4);
var
  ref: TPunkt := pkt;                 { Referenz darauf (statt @pkt) }
val
  a, b, i: Integer := default;
begin
  WriteLn('--- 1. verkettete Liste ---');
  
  for i := 1 to 5 do liste.Vorne(i * 10);
  liste.Zeige;
  liste.Umdrehen;
  liste.Zeige;

  WriteLn('--- 5. optional Integer ---');
  case liste.Position(30) of            { wie: match liste.position(30) }
    some pos: WriteLn('30 steht an Position ', pos);
    none: WriteLn('30 nicht gefunden');
  end;
  case liste.Position(99) of
    some pos: WriteLn('99 steht an Position ', pos);
    none: WriteLn('99 nicht gefunden');
  end;
  WriteLn('Position von 50 oder 0: ', liste.Position(50).ValueOr(0));

  WriteLn('--- 2. binaerer Suchbaum ---');
  
  baum.Einfuegen(30); baum.Einfuegen(70); baum.Einfuegen(20);
  baum.Einfuegen(40); baum.Einfuegen(60); baum.Einfuegen(80); baum.Einfuegen(35);
  Write('sortiert: '); baum.InOrder; WriteLn;
  WriteLn('enthaelt 40: ', baum.Enthaelt(40), ', enthaelt 45: ', baum.Enthaelt(45), ', Hoehe: ', baum.Hoehe);

  WriteLn('--- 3. var-Parameter statt Zeiger ---');
  a := 1; b := 2;
  Tausche(a, b);
  WriteLn('a=', a, ' b=', b);

  WriteLn('--- 4. Referenz auf ein Objekt ---');
  Verschiebe(ref, 10);                  { wie @pkt, aber sicher geprueft }
  WriteLn('pkt = (', pkt.X, ',', pkt.Y, ')');
end.
