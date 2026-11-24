program Inferenz;
type
  TFoo = class
    V: Integer;
    constructor Create(AV: Integer);
    destructor Destroy; override;
  end;
constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
destructor TFoo.Destroy; begin WriteLn('  Destroy TFoo ', V); inherited Destroy; end;

procedure Demo;
vao
  I := 5;                     { besitzter Heapwert, Typ Integer aus dem Initialisierer }
val
  N := 3.5;                   { Wert, Typ Real }
  S := 'Hallo';               { Typ String }
begin
  I := I * 2;
  WriteLn('I = ', I, ', N = ', N:0:1, ', S = ', S);
  I.Free;
end;

val
  i := 0;
begin
  Demo;
  begin
    WriteLn('ohne Deklaration: normaler begin-Block');
  end;
  val
    j: Integer := default;
    k := 10;
    vao s := TFoo.Create(1);                  { Besitzer im Block }
  vao
    h := TFoo.Create(2);       
  begin
    j := i + k;
    
    WriteLn('im Block: j = ', j, ', s.V = ', s.V, ', h.V = ', h.V);
    h.Free;
  end;                        { s wird hier abgebaut }
  WriteLn('nach dem Block');
  while i < 2 do
    val q := i * 100;
    begin
      WriteLn('Schleife: q = ', q);
      Inc(i);
    end;
end.
