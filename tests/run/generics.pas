program Generics;
{ Generische Records und Klassen (Stufe 1): TPaar<A, B>, TListe<T>, rekursiv, Vererbung von Instanzen }
type
  TPaar<A, B> = record
    Erstes: A;
    Zweites: B;
    function Text: String; const;
  end;

  TListe<T> = class
    Elemente: array[1..10] of optional T;
    Anzahl: Integer;
    procedure Add(vao x: T);
    function Hole(i: Integer): var optional T;
    function Laenge: Integer; const;
  end;

  TKnoten<T> = class                     { rekursiv }
    Wert: T;
    Next: optional TKnoten<T>;
  end;

  TKunde = class
    Name: String;
    constructor Create(AName: String const);
    destructor Destroy; override;
  end;

  TKundenListe = class(TListe<TKunde>)
    function Suche(N: String const): var optional TKunde;
  end;

constructor TKunde.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TKunde.Destroy; begin WriteLn('  frei ', Name); inherited Destroy; end;

function TPaar<A, B>.Text: String; const;
begin Result := 'Paar'; end;

procedure TListe<T>.Add(vao x: T);
begin
  Inc(Anzahl);
  Elemente[Anzahl] := x;
end;

function TListe<T>.Hole(i: Integer): var optional T;
begin Result := Elemente[i]; end;

function TListe<T>.Laenge: Integer; const;
begin Result := Anzahl; end;

function TKundenListe.Suche(N: String const): var optional TKunde;
begin
  Result := none;
  for val i := 1 to Anzahl do
    if Elemente[i] is some k then
      if k.Name = N then Result := k;
end;

val
  p: TPaar<String, Integer> := (Erstes: 'Anna'; Zweites: 30);
vao
  zahlen := TListe<Integer>.Create;
  kunden := TKundenListe.Create;
  k1 := TKnoten<String>.Create;
begin
  WriteLn(p.Erstes, ' ', p.Zweites, ' ', p.Text);
  zahlen.Add(10);
  zahlen.Add(20);
  if zahlen.Hole(2) is some z then WriteLn('zahlen[2] = ', z, ', Laenge ', zahlen.Laenge);
  kunden.Add(TKunde.Create('Anna'));
  kunden.Add(TKunde.Create('Bert'));
  if kunden.Suche('Bert') is some k then WriteLn('gefunden: ', k.Name);
  WriteLn(kunden.ClassName, ' erbt von TListe<TKunde>, Laenge ', kunden.Laenge);
  k1.Wert := 'eins';
  k1.Next := TKnoten<String>.Create;
  if k1.Next is some n then n.Wert := 'zwei';
  WriteLn(k1.Wert);
end.
