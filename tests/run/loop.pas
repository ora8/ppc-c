program Endlos;
{ loop <Anweisung>: Endlosschleife, verlassen mit Break, Exit oder try }
type
  TK = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  frei ', N); inherited Destroy; end;

function ErsteQuadratzahlUeber(g: Integer): Integer;
val i := 1;
begin
  loop
    begin
      if i * i > g then Exit(i * i);
      Inc(i);
    end;
end;

function Pruefe(n: Integer): result Integer;
begin
  if n > 3 then Result := error('zu gross bei ' + IntToStr(n)) else Result := n;
end;

function Summe: result Integer;
val n := 0;
begin
  Result := 0;
  loop
    begin
      Inc(n);
      val x := try Pruefe(n);               { Fehler beendet Schleife und Funktion }
      begin
        Result := ok(x);
      end;
    end;
end;

val
  i := 0;
  raise := 5;                               { 'raise' ist kein Schluesselwort mehr }
begin
  loop
    begin
      Inc(i);
      if i = 2 then Continue;
      if i > 4 then Break;
      Write(i, ' ');
    end;
  WriteLn;
  WriteLn('Quadrat: ', ErsteQuadratzahlUeber(50));
  case Summe of ok v: WriteLn(v); error e: WriteLn('Fehler: ', e); end;
  i := 0;
  loop
    vao k := TK.Create(i);                  { neuer Besitzer in jedem Durchlauf }
    begin
      Inc(i);
      if i = 3 then Break;
    end;
  WriteLn('raise = ', raise);
end.
