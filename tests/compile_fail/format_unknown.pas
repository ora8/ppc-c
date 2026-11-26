{* EXPECT: unknown specifier '%q' *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%q', [n])); end.
