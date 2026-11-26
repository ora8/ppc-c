{* EXPECT: Format: %d, %u and %x need a whole number *}
program T; val n := 3; s := 'ab'; f := '%d';
begin WriteLn(Format(f, [s])); end.
