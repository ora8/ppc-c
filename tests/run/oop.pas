program OOP;
type
  {* ---- a class hierarchy, objects belong to a vao variable ---- *}
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
    constructor Create(AName: String const);
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
    constructor Create(AName: String const);
    function Sound: String; override;
  end;

constructor TShape.Create(AX, AY: Integer);
begin inherited Create; X := AX; Y := AY; end;
function TShape.Area: Real; begin Result := 0; end;
function TShape.Name: String; begin Result := 'Shape'; end;
procedure TShape.Describe;
begin
  WriteLn(Name, ' at (', X, ',', Y, ') Area=', Area:0:2);
end;
destructor TShape.Destroy; begin WriteLn('  Destroy ', Name); inherited Destroy; end;

constructor TCircle.Create(AX, AY: Integer; AR: Real);
begin inherited Create(AX, AY); R := AR; end;
function TCircle.Area: Real; begin Result := Pi * R * R; end;
function TCircle.Name: String; begin Result := 'Circle'; end;

constructor TRect.Create(AX, AY, AW, AH: Integer);
begin inherited Create(AX, AY); W := AW; H := AH; end;
function TRect.Area: Real; begin Result := W * H; end;
function TRect.Name: String; begin Result := 'Rectangle(' + inherited Name + ')'; end;

constructor TAnimal.Create(AName: String const);
begin inherited Create; FName := AName; FLegs := 4; end;
destructor TAnimal.Destroy;
begin WriteLn('  ', FName, ' is freed'); inherited Destroy; end;
procedure TAnimal.SetLegs(V: Integer); begin if V >= 0 then FLegs := V; end;
procedure TAnimal.Speak; begin WriteLn(FName, ' (', ClassName, ', ', Legs, ' legs) says ', Sound); end;
function TDog.Sound: String; begin Result := 'Wuff'; end;
constructor TCat.Create(AName: String const);
begin inherited Create(AName); Lives := 7; end;
function TCat.Sound: String; begin Result := 'Miau'; end;

procedure ShowShape(S: TShape);       {* only borrows the object *}
begin S.Describe; end;

procedure Shift(S: TShape; DX: Integer);
begin S.X := S.X + DX; end;

procedure LocalFigures;
vao
  r := TRect.Create(0, 0, 3, 4);                          {* owns r: freed at the end of the procedure *}
begin
  
  ShowShape(r);
end;                                 {* r is freed automatically here *}

vao
  c := TCircle.Create(1, 2, 1.5);                        {* global owner *}
val
  pets: array[1..2] of optional TAnimal := default;
var
  a: optional TAnimal := none;
val
  i: Integer := default;
vao
  h: TShape := TRect.Create(5, 5, 2, 2);                         {* owners *}
  dog: TDog := TDog.Create('Bello');
  cat: TCat := TCat.Create('Minka');
begin
  WriteLn('--- objects ---');
  
  c.Describe;
  ShowShape(c);
  Shift(c, 10);                 {* value parameter = reference: the original is changed *}
  c.Describe;
  LocalFigures;
  WriteLn('c is TShape: ', c is TShape, ', c is TRect: ', c is TRect, ', class: ', c.ClassName);

  WriteLn('--- freed early ---');
  
  h.Describe;
  ShowShape(h);
  h.Free;

  WriteLn('--- classes ---');
  
  
  dog.Speak;
  cat.Legs := 3;
  cat.Speak;
  a := cat;
  if a is some x then
    WriteLn('a is TCat: ', x is TCat, ', a is TDog: ', x is TDog, ', lives: ', (x as TCat).Lives);
  a := none;
  pets[1] := dog;
  pets[2] := cat;
  for i := 1 to 2 do begin
    a := pets[i];
    if a is some x then Write(x.Name, ' ');
  end;
  WriteLn;

  WriteLn('End');
end.
