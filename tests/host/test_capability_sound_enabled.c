#include "capability_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)

int main(void)
{
    char json[4096];
    ASSERT_TRUE(capability_source_schema_supported(RULE_SOURCE_SOUND_RMS_DBFS));
    ASSERT_TRUE(capability_source_runtime_available(RULE_SOURCE_SOUND_RMS_DBFS));
    ASSERT_TRUE(strcmp(capability_source_availability_reason(RULE_SOURCE_SOUND_RMS_DBFS), "implemented") == 0);
    ASSERT_TRUE(capability_build_json(json, sizeof(json)) > 0);
    ASSERT_TRUE(strstr(json, "sound.rms_dbfs") != NULL);
    ASSERT_TRUE(strstr(json, "\"runtime_available\":true") != NULL);
    puts("capability_sound_enabled tests passed");
    return 0;
}
