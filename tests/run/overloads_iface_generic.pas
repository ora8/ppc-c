program overloads_iface_generic;
type
  IPrinter = interface
    procedure Print(s: String);
  end;
  TPrinter = class(IPrinter)
    procedure Print(i: Integer); overload;
    procedure Print(s: String); overload;
  end;
  TBox<T> = class
    V: optional T;
    procedure Put(vao x: T); overload;
    procedure Put(vao x: T; Twice: Boolean); overload;
  end;
procedure TPrinter.Print(i: Integer); begin WriteLn('int ', i); end;
procedure TPrinter.Print(s: String); begin WriteLn('str ', s); end;
procedure TBox<T>.Put(vao x: T); begin V := x; end;
procedure TBox<T>.Put(vao x: T; Twice: Boolean); begin V := x; if Twice then WriteLn('twice'); end;
procedure Use(var p: IPrinter); begin p.Print('via interface'); end;
begin
  vao p := TPrinter.Create;
  vao b := TBox<Integer>.Create;
  begin
    p.Print(3);
    p.Print('three');
    var q: IPrinter := p;
    begin Use(q); end;
    b.Put(5);
    b.Put(6, True);
    if b.V is some v then WriteLn(v);
  end;
end.
