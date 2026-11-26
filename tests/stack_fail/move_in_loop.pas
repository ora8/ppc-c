{* EXPECT: use of possibly moved stack object 's' ('s' was moved to 't' at line 4) *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s := TFoo.Create(1); begin for val i := 1 to 2 do val t := s; begin t.V := i; end; end;
begin P; end.
