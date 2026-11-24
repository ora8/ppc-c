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
var
  I;                          { Heapwert, Typ kommt aus der ersten Zuweisung }
val
  N;                          { Wert, Typ kommt aus der ersten Zuweisung }
  S = 'Hallo';                { Typ aus dem Initialisierer }
begin
  I := 5;                     { Integer, Heap-Zelle wird angelegt }
  I := I * 2;
  N := 3.5;                   { Real }
  WriteLn('I = ', I, ', N = ', N:0:1, ', S = ', S);
  I.Free;
end;

val
  i;
begin
  Demo;
  i := 0;
  begin
    WriteLn('ohne Deklaration: normaler begin-Block');
  end;
  val
    j: Integer;
    k = 10;
    s: TFoo;                  { Stackobjekt im Block }
  var
    h = TFoo.Create(2);       { Heapobjekt im Block }
  begin
    j := i + k;
    s.Create(1);
    WriteLn('im Block: j = ', j, ', s.V = ', s.V, ', h.V = ', h.V);
    h.Free;
  end;                        { s wird hier abgebaut }
  WriteLn('nach dem Block');
  while i < 2 do
    val q = i * 100;
    begin
      WriteLn('Schleife: q = ', q);
      Inc(i);
    end;
end.
