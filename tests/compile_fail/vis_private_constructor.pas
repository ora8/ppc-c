{ EXPECT: the constructor "Create" of "TOnly" is private }
program T; type TOnly = class private constructor Create; end; constructor TOnly.Create; begin end;
begin vao o := TOnly.Create; begin end; end.
