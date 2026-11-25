program generic_methods;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
  end;
  TPrinter = class
    Prefix: String;
    Count: Integer;
    function Show<T>(x: T): String;
    function Pick<T: Comparable>(a, b: T): T; const;
    procedure Each<T>(a: array of T; Visit: reference to procedure(x: T));
    function Twice<T: Numeric>(x: T): T;
    class function Make<T: class>(n: Integer): TPrinter;
  private
    function Wrap<T>(x: T): String;
  end;
  TBox = record
    V: Integer;
    function Scaled<T: Numeric>(f: T): T; const;
  end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
function TPrinter.Wrap<T>(x: T): String; begin Result := '[' + Prefix + ']'; end;
function TPrinter.Show<T>(x: T): String;
begin
  Inc(Count);
  Result := Wrap<T>(x) + ' #' + IntToStr(Count);
end;
function TPrinter.Pick<T>(a, b: T): T; const;
begin
  if a < b then Result := b else Result := a;
end;
procedure TPrinter.Each<T>(a: array of T; Visit: reference to procedure(x: T));
begin
  for val x in a do Visit(x);
end;
function TPrinter.Twice<T>(x: T): T; begin Result := x + x; end;
class function TPrinter.Make<T>(n: Integer): TPrinter;
begin
  Result := TPrinter.Create;
  Result.Prefix := 'made' + IntToStr(n);
end;
function TBox.Scaled<T>(f: T): T; const; begin Result := f * 2; end;

val
  nums: array of Integer := (3, 1, 2);
  b: TBox := (V: 4);
begin
  vao p := TPrinter.Create;
  vao q := TPrinter.Make<TItem>(7);
  begin
    p.Prefix := 'p';
    WriteLn(p.Show(5), ' ', p.Show('x'), ' ', p.Show<Real>(2.5));
    WriteLn(p.Pick(3, 9), ' ', p.Pick('b', 'a'), ' ', p.Twice(21), ' ', p.Twice(1.25):0:2);
    p.Each(nums, procedure(x: Integer) begin Write(x, ' '); end);
    WriteLn;
    WriteLn(q.Prefix, ' ', b.Scaled(3), ' ', b.Scaled(1.5):0:1);
  end;
end.
