program OOP;
type
  { ---- eine Klassenhierarchie, als Stack- und als Heapinstanz nutzbar ---- }
  TShape = class
    X, Y: Integer;
    constructor Create(AX, AY: Integer);
    function Area: Real; virtual;
    function Name: String; virtual;
    procedure Describe;
    destructor Destroy; override;
  end;
  TCircle = class(TShape)
    R: Real;
    constructor Create(AX, AY: Integer; AR: Real);
    function Area: Real; override;
    function Name: String; override;
  end;
  TRect = class(TShape)
    W, H: Integer;
    constructor Create(AX, AY, AW, AH: Integer);
    function Area: Real; override;
    function Name: String; override;
  end;

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

constructor TShape.Create(AX, AY: Integer);
begin inherited Create; X := AX; Y := AY; end;
function TShape.Area: Real; begin Result := 0; end;
function TShape.Name: String; begin Result := 'Shape'; end;
procedure TShape.Describe;
begin
  WriteLn(Name, ' bei (', X, ',', Y, ') Flaeche=', Area:0:2);
end;
destructor TShape.Destroy; begin WriteLn('  Destroy ', Name); inherited Destroy; end;

constructor TCircle.Create(AX, AY: Integer; AR: Real);
begin inherited Create(AX, AY); R := AR; end;
function TCircle.Area: Real; begin Result := Pi * R * R; end;
function TCircle.Name: String; begin Result := 'Kreis'; end;

constructor TRect.Create(AX, AY, AW, AH: Integer);
begin inherited Create(AX, AY); W := AW; H := AH; end;
function TRect.Area: Real; begin Result := W * H; end;
function TRect.Name: String; begin Result := 'Rechteck(' + inherited Name + ')'; end;

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

procedure ShowShape(S: TShape);       { nimmt Stack- und Heapobjekte }
begin S.Describe; end;

procedure Verschiebe(S: TShape; DX: Integer);
begin S.X := S.X + DX; end;

procedure LokaleFiguren;
val
  r: TRect;                          { Stackobjekt: lebt nur in dieser Prozedur }
begin
  r.Create(0, 0, 3, 4);
  ShowShape(r);
end;                                 { hier wird r.Destroy automatisch gerufen }

val
  c: TCircle;                        { globales Stackobjekt }
  pets: array[1..2] of optional TAnimal;
  a: borrowed optional TAnimal;
  i: Integer;
var
  h: TShape;                         { Heapobjekte }
  dog: TDog;
  cat: TCat;
begin
  WriteLn('--- Stackobjekte ---');
  c.Create(1, 2, 1.5);
  c.Describe;
  ShowShape(c);
  Verschiebe(c, 10);                 { Wertparameter = Referenz: Original wird geaendert }
  c.Describe;
  LokaleFiguren;
  WriteLn('c is TShape: ', c is TShape, ', c is TRect: ', c is TRect, ', Klasse: ', c.ClassName);

  WriteLn('--- Heapobjekt derselben Klassen ---');
  h := TRect.Create(5, 5, 2, 2);
  h.Describe;
  ShowShape(h);
  h.Free;

  WriteLn('--- Klassen ---');
  dog := TDog.Create('Bello');
  cat := TCat.Create('Minka');
  dog.Speak;
  cat.Legs := 3;
  cat.Speak;
  a := cat;
  if a is some x then
    WriteLn('a is TCat: ', x is TCat, ', a is TDog: ', x is TDog, ', Leben: ', (x as TCat).Lives);
  a := none;
  pets[1] := dog;
  pets[2] := cat;
  for i := 1 to 2 do begin
    a := pets[i];
    if a is some x then Write(x.Name, ' ');
  end;
  WriteLn;
  for i := 1 to 2 do pets[i].Free;
  WriteLn('Ende');
end.
