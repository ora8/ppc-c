program CloneDemo;
{ Clone: ausdrueckliche tiefe Kopie, vom Compiler erzeugt mit '; auto;' }
type
  TAdresse = class
    Ort: String;
    constructor Create(AOrt: String const);
    destructor Destroy; override;
    function Clone: TAdresse; const; auto;
  end;
  TPerson = class
    Name: String;
    Adresse: optional TAdresse;            { besitzt: wird mitgeklont }
    Chef: var optional TPerson;            { Referenz: zeigt weiter auf dasselbe }
    Kinder: array[1..2] of optional TPerson;
    constructor Create(AName: String const);
    destructor Destroy; override;
    function Clone: TPerson; const; virtual; auto;
  end;
  TMitarbeiter = class(TPerson)
    Nr: Integer;
    Buero: optional TAdresse;
  end;
  TPaar = record
    A, B: optional TAdresse;
    Info: String;
    function Clone: TPaar; const; auto;
  end;

constructor TAdresse.Create(AOrt: String const); begin inherited Create; Ort := AOrt; end;
destructor TAdresse.Destroy; begin WriteLn('  frei Adresse ', Ort); inherited Destroy; end;
constructor TPerson.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TPerson.Destroy; begin WriteLn('  frei ', Name); inherited Destroy; end;

vao boss := TPerson.Create('Boss');

procedure Lokal;
vao
  anna := TPerson.Create('Anna');
begin
  anna.Adresse := TAdresse.Create('Bern');
  anna.Chef := boss;
  anna.Kinder[1] := TPerson.Create('Kind');
  vao kopie := anna.Clone;
  begin
    kopie.Name := 'Anna2';
    if kopie.Adresse is some a then a.Ort := 'Basel';
    if anna.Adresse is some a then WriteLn('Original: ', a.Ort);
    if kopie.Chef is some c then WriteLn('Chef der Kopie: ', c.Name);
    if kopie.Kinder[1] is some k then WriteLn('Kind der Kopie: ', k.Name);
  end;
  WriteLn('-- Ende Lokal');
end;

procedure Poly;
vao
  m := TMitarbeiter.Create('Max');
begin
  m.Nr := 7;
  m.Buero := TAdresse.Create('Zug');
  var p: TPerson := m;
  begin
    vao k := p.Clone;                       { ganzes Objekt, auch TMitarbeiter-Felder }
    begin
      WriteLn('Klasse: ', k.ClassName);
      if k is TMitarbeiter then WriteLn('Nr: ', (k as TMitarbeiter).Nr);
    end;
  end;
end;

procedure RecordClone;
val
  p: TPaar := default;
  q: TPaar := default;
begin
  p.A := TAdresse.Create('Chur');
  p.Info := 'x';
  q := p.Clone;
  if q.A is some a then a.Ort := 'Thun';
  if p.A is some a then WriteLn('Record-Original: ', a.Ort);
end;

begin
  Lokal;
  Poly;
  RecordClone;
end.
