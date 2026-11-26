{* EXPECT: Runtime error 219: a TBar object does not fit into stack object 's' (TFoo) at runtime_fail/stack_subclass.pas(6,10) *}
{* FLAGS: *}
program T;
type TFoo = class V: Integer; end; TBar = class(TFoo) W: Integer; end;
procedure P(vao x: TFoo);
val s := x;
begin WriteLn(s.V); end;
begin P(TBar.Create); end.
