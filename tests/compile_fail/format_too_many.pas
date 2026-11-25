{ EXPECT: 2 arguments given, but the format uses only 1 }
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%d', [n, n])); end.
