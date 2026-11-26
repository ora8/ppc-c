program Generics;
{* generic records and classes (stage 1): TPair<A, B>, TList<T>, recursive, inheritance from instances *}
type
  TPair<A, B> = record
    First: A;
    Second: B;
    function Text: String; const;
  end;

  TList<T> = class
    Items: array[1..10] of optional T;
    Size: Integer;
    procedure Add(vao x: T);
    function Get(i: Integer): var optional T;
    function Len: Integer; const;
  end;

  TNode<T> = class                     {* recursive *}
    Value: T;
    Next: optional TNode<T>;
  end;

  TCustomer = class
    Name: String;
    constructor Create(AName: String const);
    destructor Destroy; override;
  end;

  TCustomerList = class(TList<TCustomer>)
    function Find(N: String const): var optional TCustomer;
  end;

constructor TCustomer.Create(AName: String const); begin inherited Create; Name := AName; end;
destructor TCustomer.Destroy; begin WriteLn('  freed ', Name); inherited Destroy; end;

function TPair<A, B>.Text: String; const;
begin Result := 'MakePair'; end;

procedure TList<T>.Add(vao x: T);
begin
  Inc(Size);
  Items[Size] := x;
end;

function TList<T>.Get(i: Integer): var optional T;
begin Result := Items[i]; end;

function TList<T>.Len: Integer; const;
begin Result := Size; end;

function TCustomerList.Find(N: String const): var optional TCustomer;
begin
  Result := none;
  for val i := 1 to Size do
    if Items[i] is some k then
      if k.Name = N then Result := k;
end;

val
  p: TPair<String, Integer> := (First: 'Anna'; Second: 30);
vao
  numbers := TList<Integer>.Create;
  customers := TCustomerList.Create;
  k1 := TNode<String>.Create;
begin
  WriteLn(p.First, ' ', p.Second, ' ', p.Text);
  numbers.Add(10);
  numbers.Add(20);
  if numbers.Get(2) is some z then WriteLn('numbers[2] = ', z, ', Len ', numbers.Len);
  customers.Add(TCustomer.Create('Anna'));
  customers.Add(TCustomer.Create('Bert'));
  if customers.Find('Bert') is some k then WriteLn('found: ', k.Name);
  WriteLn(customers.ClassName, ' inherits from TList<TCustomer>, Len ', customers.Len);
  k1.Value := 'eins';
  k1.Next := TNode<String>.Create;
  if k1.Next is some n then n.Value := 'zwei';
  WriteLn(k1.Value);
end.
