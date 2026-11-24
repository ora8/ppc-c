{ EXPECT: the 'heap' section is now called 'var' (and the old 'var' section is called 'val') }
program T;
type TFoo = class end;
heap h: TFoo;
begin end.
