program StdLibrary;
{ Standardbibliothek (Generics Stufe 6): uses ppc_std -> TListe, TStapel, TMap, Sortiere, Suche }
uses ppc_std;
type
  TKunde = class
    Name: String;
    constructor Create(AName: String const);
    destructor Destroy; override;
  end;
constructor TKunde.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TKunde.Destroy; begin WriteLn('  frei ', Name); inherited Destroy; end;

val
  zahlen: array of Integer := (5, 3, 9, 1, 7);
  namen: array of String := ('Mia', 'Anna', 'Bert');
vao
  kunden := TListe<TKunde>.Create;
  stapel := TStapel<Integer>.Create;
  alter := TMap<String, Integer>.Create;
  map := TMap<Integer, TKunde>.Create;
begin
  Sortiere(zahlen);
  for val i := 0 to High(zahlen) do Write(zahlen[i], ' ');
  WriteLn;
  Sortiere(namen);
  WriteLn(namen[0], ' ', namen[1], ' ', namen[2]);
  if Suche(zahlen, 7) is some p then WriteLn('7 an Position ', p);
  if Suche(zahlen, 4) = none then WriteLn('4 nicht gefunden');

  kunden.Add(TKunde.Create('Anna'));
  kunden.Add(TKunde.Create('Bert'));
  kunden.Add(TKunde.Create('Carla'));
  if kunden.Hole(1) is some k then WriteLn('Hole(1): ', k.Name);
  if kunden.Nimm(0) is some k then WriteLn('Nimm(0): ', k.Name);
  WriteLn('Count: ', kunden.Count);

  for val i := 1 to 3 do stapel.Push(i * 10);
  if stapel.Top is some t then WriteLn('Top: ', t);
  if stapel.Pop is some t then WriteLn('Pop: ', t);
  WriteLn('Stapel: ', stapel.Count, ' leer: ', stapel.IstLeer);

  alter.Setze('Bert', 28);
  alter.Setze('Anna', 30);
  alter.Setze('Carla', 31);
  alter.Setze('Anna', 31);
  if alter.Hole('Anna') is some a then WriteLn('Anna: ', a);
  WriteLn('enthaelt Dora: ', alter.Enthaelt('Dora'), ', Count ', alter.Count);
  if alter.Entferne('Bert') is some a then WriteLn('Bert entfernt: ', a);
  WriteLn('Schluessel[0..1]: ', alter.Schluessel[0], ' ', alter.Schluessel[1]);

  map.Setze(2, TKunde.Create('Zwei'));
  map.Setze(1, TKunde.Create('Eins'));
  map.Setze(2, TKunde.Create('Zwei neu'));    { alte Zwei wird freigegeben }
  if map.Hole(2) is some k then WriteLn('map[2]: ', k.Name);
  WriteLn('Ende');
end.
