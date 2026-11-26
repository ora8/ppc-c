{* EXPECT: the format needs more arguments than the 1 given *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%d %d', [n])); end.
