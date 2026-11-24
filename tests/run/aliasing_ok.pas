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

var
  a: TKnoten.Create(1);
val
  l1, l2: borrowed const TKnoten;         { mehrere lesende Ausleihen gleichzeitig }
  m: borrowed TKnoten;                    { eine aenderbare Ausleihe }
  links: borrowed optional TKnoten;
begin
  a.Links := TKnoten.Create(10);
  a.Rechts := TKnoten.Create(20);
  l1 := a;
  l2 := a;
  WriteLn('zwei Leser: ', l1.Info, ' ', l2.Info, ' ', a.Info);   { lesen ist mit Lesern vertraeglich }

  m := a;                                 { l1/l2 werden danach nicht mehr benutzt }
  m.Erhoehe;
  m.Erhoehe;
  WriteLn('nach m: ', a.Info);            { m ist hier zu Ende, a wieder frei benutzbar }

  links := a.Links;                       { Ausleihe eines Felds ... }
  if a.Rechts is some r then r.Erhoehe;   { ... ein anderes Feld darf geaendert werden }
  if links is some x then begin x.Erhoehe; WriteLn('links: ', x.Info); end;
  if a.Rechts is some r then WriteLn('rechts: ', r.Info);
end.
