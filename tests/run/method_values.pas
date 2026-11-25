program method_values;
import ppc_std;
type
  IGreeter = interface
    procedure Greet(Name: String);
  end;
  TPrinter = class(IGreeter)
    Prefix: String;
    Count: Integer;
    procedure Print(x: Integer); virtual;
    function Less(a, b: Integer): Boolean; const;
    procedure Greet(Name: String);
    procedure Tick;
  end;
  TLoud = class(TPrinter)
    procedure Print(x: Integer); override;
  end;
  TAcc = record
    Sum: Integer;
    procedure Add(x: Integer);
  end;
procedure TPrinter.Print(x: Integer); begin Inc(Count); WriteLn(Prefix, x); end;
function TPrinter.Less(a, b: Integer): Boolean; const; begin Result := a > b; end;
procedure TPrinter.Greet(Name: String); begin WriteLn('hello ', Name); end;
procedure TPrinter.Tick; begin Inc(Count); end;
procedure TLoud.Print(x: Integer); begin Inc(Count); WriteLn(Prefix, x, '!'); end;
procedure TAcc.Add(x: Integer); begin Sum := Sum + x; end;

procedure ForEach(a: array of Integer; Visit: reference to procedure(x: Integer));
begin
  for val x in a do Visit(x);
end;
procedure Twice(Action: reference to procedure);
begin
  Action;
  Action();
end;
procedure Call(Hello: reference to procedure(Name: String)); begin Hello('Anna'); end;

val
  nums: array of Integer := (3, 1, 2);
  acc: TAcc := default;
begin
  vao p := TPrinter.Create;
  vao l := TLoud.Create;
  begin
    p.Prefix := '- ';
    l.Prefix := '* ';
    ForEach(nums, p.Print);
    ForEach(nums, l.Print);
    SortBy(nums, p.Less);
    for val x in nums do Write(x, ' ');
    WriteLn;
    Twice(p.Tick);
    WriteLn(p.Count, ' ', l.Count);
    ForEach(nums, acc.Add);
    WriteLn(acc.Sum);
    var g: IGreeter := p;
    begin
      Call(g.Greet);
    end;
  end;
end.
