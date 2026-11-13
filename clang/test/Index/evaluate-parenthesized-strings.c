const char *plain = "foobar";
const char *paren = ("foobar");
const char *nested = ((("foobar")));
const char *concat = ("foo" "bar");
const char *escaped = ("foo\nbar\t\x21");
#define TEXT ("foo" "bar")
__auto_type macro = TEXT;

// RUN: c-index-test -evaluate-cursor-at=%s:1:13 \
// RUN:   -evaluate-cursor-at=%s:2:13 \
// RUN:   -evaluate-cursor-at=%s:3:13 \
// RUN:   -evaluate-cursor-at=%s:4:13 \
// RUN:   -evaluate-cursor-at=%s:5:13 \
// RUN:   -evaluate-cursor-at=%s:7:13 %s | FileCheck %s
// CHECK: Kind: CString , Value: foobar
// CHECK: Kind: CString , Value: foobar
// CHECK: Kind: CString , Value: foobar
// CHECK: Kind: CString , Value: foobar
// CHECK: Kind: CString , Value: foo
// CHECK-NEXT: bar !
// CHECK: Kind: CString , Value: foobar
