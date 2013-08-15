Checkout from https://src.chromium.org/chrome/trunk/src/url and
apply following patch.

Index: url_canon_icu.cc
===================================================================
--- url_canon_icu.cc	(revision 217736)
+++ url_canon_icu.cc	(working copy)
@@ -8,9 +8,9 @@
 #include <string.h>
 
 #include "base/logging.h"
-#include "third_party/icu/source/common/unicode/ucnv.h"
-#include "third_party/icu/source/common/unicode/ucnv_cb.h"
-#include "third_party/icu/source/common/unicode/uidna.h"
+#include "unicode/ucnv.h"
+#include "unicode/ucnv_cb.h"
+#include "unicode/uidna.h"
 #include "url/url_canon_icu.h"
 #include "url/url_canon_internal.h"  // for _itoa_s
 
