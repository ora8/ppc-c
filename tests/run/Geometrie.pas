module Geometrie;
{ ein eigenes Modul: wird von modules.pas mit 'import Geometrie;' eingebunden }
import ppc_std;

const
  Pi2 = 6.283185307179586;

type
  TPunkt = record
    X, Y: Real;
    function Abstand(b: TPunkt): Real; const;
  end;
  TFigur = class
    Name: String;
    constructor Create(AName: String const);
    function Flaeche: Real; virtual;
  end;
  TKreis = class(TFigur)
    R: Real;
    constructor Create(AR: Real);
    function Flaeche: Real; override;
  end;

function TPunkt.Abstand(b: TPunkt): Real; const;
begin Result := Sqrt(Sqr(X - b.X) + Sqr(Y - b.Y)); end;

constructor TFigur.Create(AName: String const); begin inherited Create; Name := AName; end;
function TFigur.Flaeche: Real; begin Result := 0; end;
constructor TKreis.Create(AR: Real); begin inherited Create('Kreis'); R := AR; end;
function TKreis.Flaeche: Real; begin Result := Pi2 / 2 * R * R; end;

function Gesamtflaeche(l: TListe<TFigur>): Real;   { benutzt ppc_std }
begin
  Result := 0;
  for val i := 0 to l.Count - 1 do
    if l.Hole(i) is some f then Result := Result + f.Flaeche;
end;

end.
