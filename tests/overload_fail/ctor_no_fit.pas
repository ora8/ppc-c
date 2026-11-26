{* EXPECT: no overload of "Create" fits the arguments (Boolean) *}
program T;
type TA = class constructor Create(i: Integer); overload; constructor Create(s: String); overload; end; constructor TA.Create(i: Integer); begin end; constructor TA.Create(s: String); begin end; procedure Q; begin vao a := TA.Create(True); begin end; end;
begin end.
