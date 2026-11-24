program OwnedValues;
{ Arrays und Records, die Objekte besitzen: werden verschoben statt kopiert und
  am Ende ihres Bereichs mit allen Objekten freigegeben }
type
  TK = class N: Integer; constructor Create(AN: Integer); destructor Destroy; override; end;
  TPaar = record
    Name: String;
    L, R: optional TK;
  end;
  TListe = array[1..3] of TPaar;
  THalter = class
    P: TPaar;
    A: array[1..2] of optional TK;
  end;
constructor TK.Create(AN: Integer); begin inherited Create; N := AN; end;
destructor TK.Destroy; begin WriteLn('  frei ', N); inherited Destroy; end;

function Mach(n: Integer): TPaar;
begin
  Result.Name := 'p' + IntToStr(n);
  Result.L := TK.Create(n);
end;

procedure Zeige(p: TPaar);              { Ausleihe, keine Kopie }
begin
  if p.L is some k then WriteLn('  ', p.Name, ': ', k.N);
end;

procedure Lokal;
val
  a: array[1..2] of optional TK := default;
  q: TPaar := default;
  z: TPaar := default;
vao
  b: array[1..2] of optional TK := default;
  h := THalter.Create;
begin
  a[1] := TK.Create(1);
  a[2] := TK.Create(2);
  b[1] := TK.Create(3);
  q := Mach(4);
  Zeige(q);
  z := q;                               { Move: q ist danach leer }
  WriteLn('  q leer: ', q.L = none);
  q := Mach(5);
  q := Mach(6);                         { altes q (5) wird freigegeben }
  h.P := Mach(7);
  h.A[2] := TK.Create(8);
  Mach(9);                              { unbenutzt: sofort frei }
  WriteLn('Lokal Ende');
end;

val l: TListe := default;
begin
  Lokal;
  l[1] := Mach(10);
  l[2].R := TK.Create(11);
  WriteLn('Programmende');
end.
