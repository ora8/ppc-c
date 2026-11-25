{ EXPECT: borrowed reference 'x' is used after its owner 'a' was resized by SetLength }
program T;
import ppc_std;
type
  TItem = class
    N: Integer;
    constructor Create(AN: Integer);
    destructor Destroy; override;
  end;
  TOrder = record
    Name: String;
    Item: optional TItem;
  end;
constructor TItem.Create(AN: Integer); begin N := AN; end;
destructor TItem.Destroy; begin WriteLn('free item ', N); end;

function MakeOrder(Name: String; n: Integer): TOrder;
begin
  Result.Name := Name;
  Result.Item := TItem.Create(n);
end;
val a: array of optional TOrder := default; begin SetLength(a, 1); a[0] := MakeOrder('z', 5); if a[0] is some x then begin SetLength(a, 0); WriteLn(x.Name); end; end.
