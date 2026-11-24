program AliasingOk;
{ Ausleihregeln wie in Rust: was erlaubt ist }
type
  TKnoten = class
    Wert: Integer;
    Links, Rechts: optional TKnoten;
    constructor Create(W: Integer);
    procedure Erhoehe;
    function Info: String; const;
  end;
constructor TKnoten.Create(W: Integer); begin inherited Create; Wert := W; end;
procedure TKnoten.Erhoehe; begin Inc(Wert); end;
function TKnoten.Info: String; const; begin Result := IntToStr(Wert); end;

vao
  a := TKnoten.Create(1);
begin
  a.Links := TKnoten.Create(10);
  a.Rechts := TKnoten.Create(20);
  var
    l1: TKnoten const := a;      { mehrere lesende Ausleihen gleichzeitig }
    l2: TKnoten const := a;
  begin
    WriteLn('zwei Leser: ', l1.Info, ' ', l2.Info, ' ', a.Info);   { lesen ist mit Lesern vertraeglich }
  end;
  var m: TKnoten := a;            { eine aenderbare Ausleihe }
  begin
    m.Erhoehe;
    m.Erhoehe;
  end;
  WriteLn('nach m: ', a.Info);            { m ist zu Ende, a wieder frei benutzbar }
  var links: optional TKnoten := a.Links;   { Ausleihe eines Felds ... }
  begin
    if a.Rechts is some r then r.Erhoehe; { ... ein anderes Feld darf geaendert werden }
    if links is some x then begin x.Erhoehe; WriteLn('links: ', x.Info); end;
    if a.Rechts is some r then WriteLn('rechts: ', r.Info);
  end;
end.
