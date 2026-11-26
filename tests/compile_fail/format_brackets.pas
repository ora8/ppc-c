{* EXPECT: Format needs its arguments in brackets *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format('%d', n)); end.
