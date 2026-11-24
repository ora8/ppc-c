program StdShadow;
{ eigene Namen verdecken die der Bibliothek; die Bibliothek benutzt weiter ihre eigenen }
uses ppc_std;
type
  TStapel = record Hoehe: Integer; end;          { verdeckt TStapel<T> aus ppc_std }

function Suche(x: Integer): String;              { verdeckt Suche<T> }
begin Result := 'eigene Suche ' + IntToStr(x); end;

val
  s: TStapel := (Hoehe: 3);
  a: array of Integer := (3, 1, 2);
vao l := TListe<Integer>.Create;
begin
  WriteLn(s.Hoehe, ' ', Suche(5));
  Sortiere(a);
  WriteLn(a[0], a[1], a[2]);
  l.Add(4);
  WriteLn(l.Count);
end.
