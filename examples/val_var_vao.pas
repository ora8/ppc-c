program ValVarVao;
{ Drei Arten von Variablen:
    val  x := 5;                 Wert (Integer, String, Record, Array, optional)
    var  r: Integer := x;        Referenz auf eine andere Variable, besitzt nichts
    vao  o := TKonto.Create;     besitzt das Objekt: am Ende des Bereichs freigegeben
  'T const' macht eine Variable oder einen Parameter unveraenderlich;
  ein 'T const'-Parameter wird per Referenz uebergeben, ohne Kopie. }
type
  TKonto = class
    Name: String;
    Saldo: Integer;
    constructor Create(AName: String const; ASaldo: Integer);
    destructor Destroy; override;
    class function Eroeffne(AName: String const): TKonto;   { ohne Self }
    procedure Einzahlen(Betrag: Integer);
    function Info: String; const;
  end;
  TPunkt = record
    X, Y: Integer;
    class function Ursprung: TPunkt;
  end;

constructor TKonto.Create(AName: String const; ASaldo: Integer);
begin inherited Create; Name := AName; Saldo := ASaldo; end;
destructor TKonto.Destroy; begin WriteLn('  frei: ', Name); inherited Destroy; end;
class function TKonto.Eroeffne(AName: String const): TKonto;
begin Result := TKonto.Create(AName, 10); end;              { Startguthaben }
procedure TKonto.Einzahlen(Betrag: Integer); begin Saldo := Saldo + Betrag; end;
function TKonto.Info: String; const; begin Result := Name + ': ' + IntToStr(Saldo); end;
class function TPunkt.Ursprung: TPunkt; begin Result := (X: 0; Y: 0); end;

procedure Zeige(K: TKonto const);           { Ausleihe, nur lesbar }
begin WriteLn('  ', K.Info); end;

procedure Verdopple(var N: Integer);        { var-Parameter: aendert das Original }
begin N := N * 2; end;

procedure Lokal;
vao
  k := TKonto.Eroeffne('lokal');            { gehoert Lokal }
begin
  Zeige(k);
end;                                        { k wird hier freigegeben }

val
  zahl := 21;
  p := TPunkt.Ursprung;
  limit: Integer const := 100;              { unveraenderlich }
vao
  anna := TKonto.Create('Anna', 50);
  bert := TKonto.Eroeffne('Bert');
begin
  WriteLn('--- vao: Besitzer');
  Lokal;
  anna.Einzahlen(25);
  Zeige(anna);
  Zeige(bert);

  WriteLn('--- var: Referenz ohne Besitz');
  var
    r: Integer := zahl;                     { r ist ein anderer Name fuer zahl }
    k: TKonto := anna;                      { k leiht anna aus, gibt nichts frei }
  begin
    r := r + 1;
    k.Einzahlen(5);
    Verdopple(r);
  end;
  WriteLn('  zahl = ', zahl, ', anna = ', anna.Saldo);

  WriteLn('--- const');
  p.X := limit;
  WriteLn('  p = (', p.X, ', ', p.Y, '), limit = ', limit);
  WriteLn('--- Programmende');
end.                                        { anna und bert werden freigegeben }
