program StackVsHeap;
{ Eine Klasse, zwei Speicherorte:
    val  z: TZaehler;   -> Stackobjekt: x.Create(...), Destroy automatisch am Blockende
    var  h: TZaehler;   -> Heapobjekt:  h := TZaehler.Create(...), h.Free von Hand }
type
  TZaehler = class
    Name: String;
    Wert: Integer;
    constructor Create(const AName: String);
    destructor Destroy; override;
    procedure Erhoehe;
  end;

constructor TZaehler.Create(const AName: String);
begin inherited Create; Name := AName; WriteLn('  ', Name, ' erzeugt'); end;
destructor TZaehler.Destroy;
begin WriteLn('  ', Name, ' abgebaut (Wert ', Wert, ')'); inherited Destroy; end;
procedure TZaehler.Erhoehe; begin Wert := Wert + 1; end;

{ Beide Arten werden gleich uebergeben: als Referenz auf dieselbe Instanz }
procedure ZweimalErhoehen(Z: TZaehler); begin Z.Erhoehe; Z.Erhoehe; end;

{ Nur ein Heapobjekt kann die Routine ueberleben (Besitz geht an den Aufrufer) }
function NeuerZaehler(const N: String): TZaehler;
begin Result := TZaehler.Create(N); end;

procedure Block;
val
  s: TZaehler;                    { Stackobjekt: Speicher liegt im Stackframe }
var
  h: TZaehler;                    { Heapobjekt: Speicher kommt vom Heap }
begin
  WriteLn('Block beginnt');
  s.Create('stack-s');            { nur Konstruktor, kein malloc }
  h := TZaehler.Create('heap-h'); { malloc + Konstruktor }
  ZweimalErhoehen(s);
  ZweimalErhoehen(h);
  WriteLn('  s=', s.Wert, ' h=', h.Wert);
  h.Free;                         { Heapobjekt: von Hand freigeben }
  WriteLn('Block endet');
end;                              { Stackobjekt s wird hier automatisch abgebaut }

var
  r: TZaehler;
val
  b: borrowed TZaehler;
begin
  Block;

  WriteLn('Heapobjekt aus einer Funktion:');
  r := NeuerZaehler('heap-r');
  b := r;                         { zweite Referenz (Ausleihe) auf dasselbe Objekt }
  b.Erhoehe;
  WriteLn('  r.Wert=', r.Wert, ' (ueber b erhoeht: gleiche Instanz)');
  r.Free;                         { ohne Free: Warnung "memory leak" }
end.
