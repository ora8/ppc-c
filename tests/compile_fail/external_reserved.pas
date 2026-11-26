{* EXPECT: the C name '__std_floor' is reserved (names starting with '__') *}
program T; function Floor(x: Real): Int64; external '__std_floor'; begin end.
