program Results;
{ errors as return values: 'result T' (like Result<T, E> in Rust) }
type
  TError = (NoError, DivByZero, TooBig);
  TPoint = record
    X, Y: Integer;
  end;
  TCounter = class
    N: Integer;
    destructor Destroy; override;
  end;

destructor TCounter.Destroy; begin WriteLn('  freed: Counter ', N); inherited Destroy; end;

function Divide(a, b: Integer): result Integer;          { error: String }
begin
  if b = 0 then Result := error('division by 0')
  else Result := a div b;                               { Value = ok(...) }
end;

function Check(n: Integer): result of TError;         { no value, only success or error }
begin
  if n > 100 then Result := error(TooBig) else Result := ok;
end;

function Compute(a, b, c: Integer): result Integer;
vao z := TCounter.Create;                               { is freed on an error too }
begin
  val q := try Divide(a, b);                             { error -> return at once }
  begin
    z.N := q;
    Result := try Divide(q, c);
    Result := ok(q * 10);
  end;
end;

function Chain(n: Integer): result of TError;
begin
  try Check(n);                                        { statement: only the error counts }
  try Check(n * 2);
  WriteLn('  Chain ok for ', n);
end;

function MakePoint(ok_: Boolean): result TPoint;
begin
  if ok_ then Result := (X: 1; Y: 2) else Result := error('no MakePoint');
end;

val r := Divide(10, 2);
begin
  if r is ok v then WriteLn('10 / 2 = ', v);
  case Divide(1, 0) of
    ok v: WriteLn('Value ', v);
    error e: WriteLn('Error: ', e);
  end;
  WriteLn('ValueOr: ', Divide(7, 0).ValueOr(-1), ' ', Divide(7, 7).ValueOr(-1));
  WriteLn('IsOk: ', Divide(1, 1).IsOk, ', IsError: ', Divide(1, 0).IsError);
  WriteLn('-- try');
  case Compute(100, 5, 2) of ok v: WriteLn('Compute = ', v); error e: WriteLn('Error: ', e); end;
  case Compute(100, 0, 2) of ok v: WriteLn('Compute = ', v); error e: WriteLn('Error: ', e); end;
  case Compute(100, 5, 0) of ok v: WriteLn('Compute = ', v); error e: WriteLn('Error: ', e); end;
  WriteLn('-- without a value');
  if Chain(10) is ok then WriteLn('  ok');
  if Chain(60) is error f then WriteLn('  Error ', Ord(f));
  case Check(500) of
    ok: WriteLn('  fits');
    error f: WriteLn('  too big: ', f = TooBig);
  end;
  WriteLn('-- Record');
  if MakePoint(True) is ok p then WriteLn('  (', p.X, ',', p.Y, ')');
  if MakePoint(False) is error e then WriteLn('  ', e);
end.
