{* EXPECT: 'object' types are not supported: use 'class' (objects are created with 'vao x := TFoo.Create(...)') *}
program T;
type A = object X: Integer; end;
begin end.
