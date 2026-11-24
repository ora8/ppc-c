program ArrayClone;
{ Arrays mit Objekten: Zuweisung verschiebt, a.Clone kopiert tief }
type
  TTier = class
    Name: String;
    constructor Create(AName: String const);
    destructor Destroy; override;
    function Clone: TTier; const; auto;
  end;
  TZoo = array[1..3] of optional TTier;
  TGehege = record                       { Array in einem Record: Clone wird erzeugt }
    Tiere: TZoo;
    function Clone: TGehege; const; auto;
  end;

constructor TTier.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TTier.Destroy; begin WriteLn('  frei ', Name); inherited Destroy; end;

procedure Zeige(Titel: String; z: TZoo);
begin
  Write(Titel, ': ');
  for val i := 1 to 3 do
    if z[i] is some t then Write(t.Name, ' ') else Write('- ');
  WriteLn;
end;

val
  a: TZoo := default;
  b: TZoo := default;
  c: TZoo := default;
  g: TGehege := default;
  h: TGehege := default;
begin
  a[1] := TTier.Create('Leo');
  a[2] := TTier.Create('Mia');

  WriteLn('1. Zuweisung = Move');
  b := a;                                { die Objekte wandern nach b, a ist leer }
  Zeige('a', a);
  Zeige('b', b);

  WriteLn('2. Kopie mit Clone, Element fuer Element');
  for val i := 1 to 3 do
    if b[i] is some t then c[i] := t.Clone;
  if c[1] is some t then t.Name := 'Leo2';     { aendert nur die Kopie }
  Zeige('b', b);
  Zeige('c', c);

  WriteLn('3. Kopie des ganzen Arrays mit a.Clone');
  val d := b.Clone;
  begin
    if d[2] is some t then t.Name := 'Mia2';
    Zeige('b', b);
    Zeige('d', d);
  end;
  WriteLn('4. Kopie des ganzen Records mit Clone');
  g.Tiere[1] := TTier.Create('Rex');
  h := g.Clone;
  Zeige('g', g.Tiere);
  Zeige('h', h.Tiere);
  val zahlen: array[1..3] of Integer := (1, 2, 3);
  begin
    val k := zahlen.Clone;                     { ohne Objekte: einfach eine Kopie }
    begin
      k[1] := 9;
      WriteLn('Zahlen: ', zahlen[1], ' ', k[1]);
    end;
  end;
  WriteLn('Programmende');
end.
