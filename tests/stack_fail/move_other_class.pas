{* EXPECT: stack object 't' is declared as "TFoo" but 's' is a "TBar": both must be the same class *}
program T;
type TFoo = class V: Integer; constructor Create(AV: Integer); end; TBar = class(TFoo) end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; val s := TBar.Create(1); val t: TFoo := s; begin end;
begin P; end.
