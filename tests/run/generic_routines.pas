program GenericRoutines;
{* generic functions and procedures (stage 2): type from the arguments or written out *}
type
  TCustomer = class
    Name: String;
    constructor Create(AName: String const);
  end;
  TPair<A, B> = record
    First: A;
    Second: B;
  end;
constructor TCustomer.Create(AName: String const); begin inherited Create; Name := AName; end;

function Max<T: Comparable>(a, b: T): T;
begin
  if a > b then Result := a else Result := b;
end;

procedure Swap<T>(var a, b: T);
val h: T := a;
begin
  a := b;
  b := h;
end;

function MakePair<A: Copy; B: Copy>(x: A; y: B): TPair<A, B>;
begin
  Result.First := x;
  Result.Second := y;
end;

function First<A: Copy; B>(p: TPair<A, B>): A;
begin Result := p.First; end;

function Factorial<T: Numeric>(n: T): T;          {* recursive *}
begin
  if n <= 1 then Result := 1 else Result := n * Factorial(n - 1);
end;

function Name<T: TCustomer>(k: T): String;
begin Result := k.Name; end;

val
  i := 3;
  j := 9;
  s := 'Anna';
  t := 'Bert';
vao k := TCustomer.Create('Carla');
begin
  WriteLn(Max(3, 7), ' ', Max('Anna', 'Bert'), ' ', Max(2.5, 1.0):0:1);
  WriteLn(Max(3, 7.5):0:1, ' ', Max<Real>(1, 2):0:1);
  Swap(i, j);
  Swap(s, t);
  WriteLn(i, ' ', j, ' ', s, ' ', t);
  val p := MakePair('x', 42);
  begin
    WriteLn(p.First, ' ', p.Second, ' ', First(p));
  end;
  WriteLn(Factorial(10), ' ', Name(k));
end.
