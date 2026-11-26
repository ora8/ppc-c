{* EXPECT: 'exports' belongs to the main level of a library *}
library T; procedure F; exports F; begin end; begin end.
