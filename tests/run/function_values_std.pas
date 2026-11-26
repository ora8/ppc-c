program FunctionValuesStd;
{* lambdas with ppc_std: SortBy, Filter, optional event fields *}
import ppc_std;
type
  TPerson = record Name: String; Age: Integer; end;
  TButton = class
    OnClick: optional procedure(n: Integer);
  end;
val
  people: array of TPerson := ((Name: 'Mia'; Age: 30), (Name: 'Bert'; Age: 25), (Name: 'Anna'; Age: 35));
  nums: array of Integer := (5, 8, 1, 9, 4);
vao b := TButton.Create;
begin
  SortBy(people, function(x, y: TPerson): Boolean begin Result := x.Age < y.Age; end);
  for val i := 0 to High(people) do Write(people[i].Name, ' ');
  WriteLn;
  val big := Filter(nums, function(x: Integer): Boolean begin Result := x > 4; end);
  begin
    for val i := 0 to High(big) do Write(big[i], ' ');
    WriteLn;
  end;
  b.OnClick := procedure(n: Integer) begin WriteLn('clicked ', n); end;
  if b.OnClick is some h then h(3);
end.
