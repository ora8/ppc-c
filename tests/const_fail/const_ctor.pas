{* EXPECT: constructors and destructors cannot be const *}
program T; type TK = class constructor Create; const; end;

begin end.
