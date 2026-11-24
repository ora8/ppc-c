{ EXPECT: constructors and destructors cannot be const }
program T; type TK = class const constructor Create; end;

begin end.
