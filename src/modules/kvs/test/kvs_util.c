#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdbool.h>
#include <jansson.h>

#include "src/common/libtap/tap.h"
#include "src/modules/kvs/kvs_util.h"
#include "src/modules/kvs/types.h"

int main (int argc, char *argv[])
{
    json_t *obj, *cpy, *o;
    href_t ref;
    const char *s;
    char *s1, *s2;

    plan (NO_PLAN);

    obj = json_object ();
    json_object_set_new (obj, "A", json_string ("foo"));
    json_object_set_new (obj, "B", json_string ("bar"));
    json_object_set_new (obj, "C", json_string ("cow"));

    ok ((cpy = kvs_util_json_copydir (obj)) != NULL,
        "kvs_util_json_copydir works");

    /* first manually verify */
    ok ((o = json_object_get (cpy, "A")) != NULL,
        "json_object_get got object A");
    ok ((s = json_string_value (o)) != NULL,
        "json_string_value got string A");
    ok (strcmp (s, "foo") == 0,
        "string A is correct");

    ok ((o = json_object_get (cpy, "B")) != NULL,
        "json_object_get got object B");
    ok ((s = json_string_value (o)) != NULL,
        "json_string_value got string B");
    ok (strcmp (s, "bar") == 0,
        "string B is correct");

    ok ((o = json_object_get (cpy, "C")) != NULL,
        "json_object_get got object C");
    ok ((s = json_string_value (o)) != NULL,
        "json_string_value got string C");
    ok (strcmp (s, "cow") == 0,
        "string C is correct");

    /* now use comparison to verify */
    ok (json_equal (cpy, obj) == true,
        "json_equal returns true on duplicate");

    ok (kvs_util_json_hash ("sha1", obj, ref) == 0,
        "kvs_util_json_hash works on sha1");

    ok (kvs_util_json_hash ("foobar", obj, ref) < 0,
        "kvs_util_json_hash error on bad hash name");

    json_decref (obj);
    json_decref (cpy);

    obj = json_object ();
    json_object_set_new (obj, "A", json_string ("a"));
    json_object_set_new (obj, "B", json_string ("b"));
    json_object_set_new (obj, "C", json_string ("c"));

    ok ((s1 = kvs_util_json_dumps (obj)) != NULL,
        "kvs_util_json_dumps works");

    /* json object is sorted and compacted */
    s2 = "{\"A\":\"a\",\"B\":\"b\",\"C\":\"c\"}";

    ok (!strcmp (s1, s2),
        "kvs_util_json_dumps dumps correct string");

    free (s1);
    s1 = NULL;
    json_decref (obj);

    obj = json_null ();

    ok ((s1 = kvs_util_json_dumps (obj)) != NULL,
        "kvs_util_json_dumps works");

    s2 = "null";

    ok (!strcmp (s1, s2),
        "kvs_util_json_dumps works on null object");

    free (s1);
    s1 = NULL;
    json_decref (obj);

    ok ((s1 = kvs_util_json_dumps (NULL)) != NULL,
        "kvs_util_json_dumps works on NULL pointer");

    s2 = "null";

    ok (!strcmp (s1, s2),
        "kvs_util_json_dumps works on NULL pointer");

    free (s1);
    s1 = NULL;

    done_testing ();
    return (0);
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
