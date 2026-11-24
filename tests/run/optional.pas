program Optionals;
{ optional T wie Option<T> (Rust) / std::optional<T> (C++) }
type
  TKnoten = class
    Wert: Integer;
    Next: optional TKnoten;
    constructor Create(W: Integer);
  end;
  TEintrag = record
    Name: String;
    Alter: optional Integer;           { Wert-Optional als Record-Feld }
  end;

constructor TKnoten.Create(W: Integer); begin inherited Create; Wert := W; end;

function Wurzel(x: Real): optional Real;
begin
  if x < 0 then Result := none else Result := Sqrt(x);
end;

function Parse(s: String const): optional Integer;
val i, v: Integer := default;
begin
  Result := none;
  if Length(s) = 0 then Exit;
  v := 0;
  for i := 1 to Length(s) do begin
    if not (s[i] in ['0'..'9']) then Exit;
    v := v * 10 + Ord(s[i]) - Ord('0');
  end;
  Result := v;                         { Integer -> some(v) }
end;

function Gross(k: optional TKnoten): Boolean;
begin
  Result := False;
  if (k is some x) and (x.Wert > 10) then Result := True;
end;

function Beschreibe(o: optional Integer): String;
begin
  case o of
    some v: Result := 'some(' + IntToStr(v) + ')';
    none: Result := 'none';
  end;
end;

val
  a, b: optional Integer := none;
  r: optional Real := none;
  s: optional String := none;
  e: TEintrag := default;
  tab: array[1..3] of optional Integer := default;
  i, summe: Integer := default;
  var p: optional TKnoten := none;
vao
  liste: optional TKnoten := none;
  k: TKnoten := TKnoten.Create(3);
begin
  a := 7;
  WriteLn('a = ', Beschreibe(a), ', b = ', Beschreibe(b));
  WriteLn('HasValue: ', a.HasValue, ' ', b.HasValue, ', ValueOr: ', a.ValueOr(0), ' ', b.ValueOr(-1));
  if a <> none then WriteLn('a ist gesetzt');
  if b = none then WriteLn('b ist leer');

  r := 3;                              { Integer -> optional Real }
  if r is some x then WriteLn('r = ', x:0:1);
  case Wurzel(16) of some w: WriteLn('Wurzel(16) = ', w:0:1); none: WriteLn('keine Wurzel'); end;
  case Wurzel(-1) of some w: WriteLn('Wurzel(-1) = ', w:0:1); none: WriteLn('Wurzel(-1): keine'); end;

  WriteLn('Parse: ', Beschreibe(Parse('123')), ' ', Beschreibe(Parse('12x')), ' ', Beschreibe(Parse('')));

  s := 'Hallo';
  if (s is some t) and (Length(t) > 3) then WriteLn('s = ', t, ' (', Length(t), ' Zeichen)');
  s := none;
  case s of
    some t: WriteLn('s = ', t);
  else
    WriteLn('s ist leer');
  end;

  e.Name := 'Bert';
  WriteLn(e.Name, ': ', Beschreibe(e.Alter));
  e.Alter := 42;
  WriteLn(e.Name, ': ', Beschreibe(e.Alter));

  tab[2] := 5;
  tab[3] := 8;
  summe := 0;
  for i := 1 to 3 do
    if tab[i] is some v then summe := summe + v;
  WriteLn('Summe der gesetzten Werte: ', summe);

  liste := TKnoten.Create(20);
  
  if liste is some h then h.Next := k;
  WriteLn('Gross(liste) = ', Gross(liste));
  p := liste;
  while p is some n do begin Write(n.Wert, ' '); p := n.Next; end;
  WriteLn;
  if Take(liste) is some erster then begin   { Take liefert den Besitz }
    WriteLn('entnommen: ', erster.Wert, ', liste leer: ', liste = none);
  end;
end.
