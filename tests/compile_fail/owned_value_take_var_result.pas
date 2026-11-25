{ EXPECT: the result of 'Get' refers into its container and cannot be taken over }
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
begin vao l := TList<TOrder>.Create; begin l.Add(MakeOrder('a', 1)); val o := l.Get(0); begin end; end; end.
