{* EXPECT: the C name 'f' is exported twice *}
library T; procedure F; begin end; procedure G; begin end; exports F name 'f', G name 'f'; begin end.
