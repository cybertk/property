#include <stdio.h>
#include <assert.h>
#include "property.h"


void listprop(const char *key, const char *value, void *cookie)
{
    printf("%s: %s\n", key, value);
}

void test_set_persist()
{
    int rc;
    const char *key = "persist.test.key1";
    const char *value = property_get(key);

    rc = property_set(key, "hi");
    assert(rc == 0);
}

void test_set_ro()
{
    int rc;
    const char *key = "ro.test.key2";
    const char *value = property_get(key);

    rc = property_set(key, "hi");

    if (value)
        assert(rc != 0);
    else
        assert(rc == 0);

    rc = property_set(key, "hi2");
    assert(rc != 0);
}

void test_set_normal()
{
    int rc;

    rc = property_set("test.key1", "test_value1");
    assert(rc == 0);

    rc = property_set("test.key2", "test_value2");
    assert(rc == 0);
}

void test()
{
    char *value;

    property_init();

    test_set_persist();
    test_set_normal();
    test_set_ro();

    property_list(&listprop, "");
}

int main()
{
    test();
}
