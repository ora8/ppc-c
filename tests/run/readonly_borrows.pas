program ReadonlyBorrows;
{ Nur lesbare Objekt-Ausleihen: 'const T' (wie &T in Rust, const T& in C++) }
type
  TPerson = class
    Name: String;
    Alter: Integer;
    Freund: optional TPerson;               { besitzt den Freund }
    constructor Create(AName: String const; AAlter: Integer);
    procedure Geburtstag;                   { aendert das Objekt }
    function Info: String; const;           { liest nur }
    function BesterFreund: var optional TPerson const; const;
  end;

constructor TPerson.Create(AName: String const; AAlter: Integer);
begin inherited Create; Name := AName; Alter := AAlter; end;
procedure TPerson.Geburtstag; begin Inc(Alter); end;
function TPerson.Info: String; const; begin Result := Name + ' (' + IntToStr(Alter) + ')'; end;
function TPerson.BesterFreund: var optional TPerson const; const;
begin Result := Freund; end;                { Freund ist hier nur lesbar }

procedure Zeige(P: TPerson const);          { const-Parameter: Objekt nur lesbar }
begin
  Write(P.Info);
  if P.BesterFreund is some f then Write(', Freund: ', f.Info);
  WriteLn;
end;

procedure Feiern(P: TPerson);               { normale Ausleihe: darf aendern }
begin P.Geburtstag; end;

function Aelter(A, B: TPerson const): var TPerson const;
begin
  if A.Alter >= B.Alter then Result := A else Result := B;
end;

vao
  anna := TPerson.Create('Anna', 30);
  bert := TPerson.Create('Bert', 28);
begin
  anna.Freund := TPerson.Create('Carla', 31);
  Zeige(anna);
  Feiern(bert);
  Feiern(bert);
  Zeige(bert);
  var leser: TPerson const := anna;   { nur lesbare Ausleihe (veraenderbar -> nur lesbar) }
  begin
    WriteLn('leser: ', leser.Info);
    WriteLn('aelter: ', Aelter(anna, bert).Info);
    var k: TPerson const := bert;                         { const-Referenz: Objekt nur lesbar }
    begin
      WriteLn('k: ', k.Info);
    end;
    WriteLn('leser zuletzt: ', leser.Info); { letzte Benutzung von leser }
  end;
  anna.Geburtstag;                          { erlaubt: leser wird danach nicht mehr benutzt }
  WriteLn('anna nach Geburtstag: ', anna.Info);
end.
