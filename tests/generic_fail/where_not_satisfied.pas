{* EXPECT: "TList<TPerson>.Sort" needs 'T: Comparable' ("TPerson" is not Comparable) *}
program T; import std; type TPerson = class Name: String; end; vao l := TList<TPerson>.Create; begin l.Sort; end.
