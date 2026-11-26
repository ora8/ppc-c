program FunctionValues;
{* function types, routines as values, lambdas *}
type
  TCompare = function(a, b: Integer): Integer;
  TAction = procedure(x: Integer);

val Total: Integer := 0;

function Desc(a, b: Integer): Integer; begin Result := b - a; end;
procedure Add(x: Integer); begin Total := Total + x; end;
function Tick: Integer; begin Result := 7; end;

procedure ForEach(a: array of Integer; act: TAction);
begin
  for val i := 0 to High(a) do act(a[i]);
end;

function Apply(f: function(x: Integer): Integer; v: Integer): Integer;
begin Result := f(v); end;

function Pick(up: Boolean): TCompare;
begin
  if up then Result := function(a, b: Integer): Integer begin Result := a - b; end
  else Result := Desc;
end;

val
  nums: array of Integer := (3, 1, 2);
  cmp: TCompare := Desc;
  t: function: Integer := Tick;
begin
  WriteLn(cmp(1, 5), ' ', Pick(True)(1, 5), ' ', t());
  ForEach(nums, Add);
  WriteLn('Total ', Total);
  ForEach(nums, procedure(x: Integer) begin Write(x * 10, ' '); end);
  WriteLn;
  WriteLn(Apply(function(x: Integer): Integer begin Result := x * x; end, 9));
  cmp := Pick(False);
  WriteLn(cmp(2, 9));
end.
