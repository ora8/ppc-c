program OOP;
type
  { ---- Stack-Objekte (Turbo-Pascal-Stil 'object') ---- }
  TShape = object
    X, Y: Integer;
    constructor Init(AX, AY: Integer);
    function Area: Real; virtual;
    function Name: String; virtual;
    procedure Describe;
    destructor Done; virtual;
  end;
  TCircle = object(TShape)
    R: Real;
    constructor Init(AX, AY: Integer; AR: Real);
    function Area: Real; virtual;
    function Name: String; virtual;
  end;
  TRect = object(TShape)
    W, H: Integer;
    constructor Init(AX, AY, AW, AH: Integer);
    function Area: Real; virtual;
    function Name: String; virtual;
  end;
  PShape = ^TShape;
  PCircle = ^TCircle;

  TCounter = object          { ohne virtuelle Methoden: keine VMT }
    Count: Integer;
    procedure Inc;
  end;

  { ---- Heap-Klassen (Delphi-Stil 'class') ---- }
  TAnimal = class
  private
    FName: String;
    FLegs: Integer;
    procedure SetLegs(V: Integer);
  public
    constructor Create(const AName: String);
    destructor Destroy; override;
    function Sound: String; virtual; abstract;
    procedure Speak;
    property Name: String read FName;
    property Legs: Integer read FLegs write SetLegs;
  end;
  TDog = class(TAnimal)
    function Sound: String; override;
  end;
  TCat = class(TAnimal)
    Lives: Integer;
    constructor Create(const AName: String);
    function Sound: String; override;
  end;

constructor TShape.Init(AX, AY: Integer);
begin X := AX; Y := AY; end;
function TShape.Area: Real; begin Result := 0; end;
function TShape.Name: String; begin Result := 'Shape'; end;
procedure TShape.Describe;
begin
  WriteLn(Name, ' bei (', X, ',', Y, ') Flaeche=', Area:0:2);
end;
destructor TShape.Done; begin WriteLn('  Done ', Name); end;

constructor TCircle.Init(AX, AY: Integer; AR: Real);
begin inherited Init(AX, AY); R := AR; end;
function TCircle.Area: Real; begin Result := Pi * R * R; end;
function TCircle.Name: String; begin Result := 'Kreis'; end;

constructor TRect.Init(AX, AY, AW, AH: Integer);
begin inherited Init(AX, AY); W := AW; H := AH; end;
function TRect.Area: Real; begin Result := W * H; end;
function TRect.Name: String; begin Result := 'Rechteck(' + inherited Name + ')'; end;

procedure TCounter.Inc; begin Count := Count + 1; end;

constructor TAnimal.Create(const AName: String);
begin inherited Create; FName := AName; FLegs := 4; end;
destructor TAnimal.Destroy;
begin WriteLn('  ', FName, ' wird freigegeben'); inherited Destroy; end;
procedure TAnimal.SetLegs(V: Integer); begin if V >= 0 then FLegs := V; end;
procedure TAnimal.Speak; begin WriteLn(FName, ' (', ClassName, ', ', Legs, ' Beine) sagt ', Sound); end;
function TDog.Sound: String; begin Result := 'Wuff'; end;
constructor TCat.Create(const AName: String);
begin inherited Create(AName); Lives := 7; end;
function TCat.Sound: String; begin Result := 'Miau'; end;

procedure ShowShape(var S: TShape);   { polymorph ueber var-Parameter }
begin S.Describe; end;

function MakeCounter(n: Integer): TCounter;  { Objekt als Rueckgabewert (Kopie) }
var k: Integer;
begin
  Result.Count := 0;
  for k := 1 to n do Result.Inc;
end;

var
  c: TCircle;          { Stackobjekte: kein New/Free noetig }
  r: TRect;
  base: TShape;
  shapes: array[1..2] of TShape;
  cnt: TCounter;
  p: PShape;
  a: borrowed nilable TAnimal;  { nicht-besitzende Referenz }
  dog: TDog;
  cat: TCat;
begin
  WriteLn('--- Stackobjekte ---');
  c.Init(1, 2, 1.5);
  r.Init(0, 0, 3, 4);
  c.Describe;
  r.Describe;
  ShowShape(c);
  ShowShape(r);
  base := c;            { Kopie des Basisteils, VMT bleibt TShape }
  base.Describe;
  WriteLn('SizeOf(TShape)=', SizeOf(TShape) > 0, ' SizeOf(TCircle)>SizeOf(TShape): ', SizeOf(TCircle) > SizeOf(TShape));
  cnt := MakeCounter(5);
  cnt.Inc;
  WriteLn('Counter = ', cnt.Count);
  shapes[1].Init(5, 5);
  shapes[2] := r;
  shapes[2].Describe;
  c.Done;

  WriteLn('--- Objekte auf dem Heap (New/Dispose) ---');
  New(p, Init(9, 9));
  p^.Describe;
  Dispose(p, Done);
  p := New(PCircle, Init(2, 2, 1.0));
  p^.Describe;
  WriteLn('Klassenname via Typtest nicht moeglich bei objects, aber Area=', p^.Area:0:3);
  Dispose(p, Done);

  WriteLn('--- Klassen ---');
  dog := TDog.Create('Bello');
  cat := TCat.Create('Minka');
  dog.Speak;
  cat.Legs := 3;
  cat.Speak;
  a := cat;
  WriteLn('a is TCat: ', a is TCat, ', a is TDog: ', a is TDog, ', Leben: ', (a as TCat).Lives);
  a := nil;
  dog.Free;
  cat.Free;
  WriteLn('Ende');
end.
