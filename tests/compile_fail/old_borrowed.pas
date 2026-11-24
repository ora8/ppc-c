{ EXPECT: 'borrowed' was replaced by the 'var' section ('var r: T := x') }
program T;
type TK = class end; val k: borrowed TK := none;
begin end.
