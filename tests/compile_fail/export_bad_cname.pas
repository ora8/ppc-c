{* EXPECT: 'my-name' is not a valid C name for the export *}
library T; procedure F; begin end; exports F name 'my-name'; begin end.
