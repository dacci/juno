Apply following patch under "url" directory.

diff --git a/url_canon_icu.cc b/url_canon_icu.cc
index cabbbf2..42da8b3 100644
--- a/url_canon_icu.cc
+++ b/url_canon_icu.cc
@@ -9,9 +9,9 @@
 
 #include "base/lazy_instance.h"
 #include "base/logging.h"
-#include "third_party/icu/source/common/unicode/ucnv.h"
-#include "third_party/icu/source/common/unicode/ucnv_cb.h"
-#include "third_party/icu/source/common/unicode/uidna.h"
+#include "unicode/ucnv.h"
+#include "unicode/ucnv_cb.h"
+#include "unicode/uidna.h"
 #include "url/url_canon_icu.h"
 #include "url/url_canon_internal.h"  // for _itoa_s
 
