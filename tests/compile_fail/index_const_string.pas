{ EXPECT: string index 300 is outside 0..255 }
program T;
val s := 'abc';
begin WriteLn(s[300]); end.
