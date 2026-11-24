program ReadonlyBorrows;
{ Nur lesbare Objekt-Ausleihen: 'const T' (wie &T in Rust, const T& in C++) }
type
  TPerson = class
    Name: String;
    Alter: Integer;
    Freund: optional TPerson;               { besitzt den Freund }
    constructor Create(const AName: String; AAlter: Integer);
    procedure Geburtstag;                   { aendert das Objekt }
    function Info: String; const;           { liest nur }
    function BesterFreund: borrowed optional const TPerson; const;
  end;

constructor TPerson.Create(const AName: String; AAlter: Integer);
begin inherited Create; Name := AName; Alter := AAlter; end;
procedure TPerson.Geburtstag; begin Inc(Alter); end;
function TPerson.Info: String; const; begin Result := Name + ' (' + IntToStr(Alter) + ')'; end;
function TPerson.BesterFreund: borrowed optional const TPerson; const;
begin Result := Freund; end;                { Freund ist hier nur lesbar }

procedure Zeige(const P: TPerson);          { const-Parameter: Objekt nur lesbar }
begin
  Write(P.Info);
  if P.BesterFreund is some f then Write(', Freund: ', f.Info);
  WriteLn;
end;

procedure Feiern(P: TPerson);               { normale Ausleihe: darf aendern }
begin P.Geburtstag; end;

function Aelter(constref A, B: TPerson): borrowed const TPerson;
begin
  if A.Alter >= B.Alter then Result := A else Result := B;
end;

var
  anna: TPerson.Create('Anna', 30);
  bert: TPerson.Create('Bert', 28);
val
  leser: borrowed const TPerson;            { nur lesbare Ausleihe }
begin
  anna.Freund := TPerson.Create('Carla', 31);
  Zeige(anna);
  Feiern(bert);
  Feiern(bert);
  Zeige(bert);
  leser := anna;                            { veraenderbar -> nur lesbar: erlaubt }
  WriteLn('leser: ', leser.Info);
  WriteLn('aelter: ', Aelter(anna, bert).Info);
  const k = bert;                           { const-Referenz: Objekt nur lesbar }
  begin
    WriteLn('k: ', k.Info);
  end;
  WriteLn('leser zuletzt: ', leser.Info);   { letzte Benutzung von leser }
  anna.Geburtstag;                          { erlaubt: leser wird danach nicht mehr benutzt }
  WriteLn('anna nach Geburtstag: ', anna.Info);
end.
