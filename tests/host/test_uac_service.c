#include "uac_service.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(e,a) do { if ((e)!=(a)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(e), (long)(a)); exit(1);} } while(0)

int main(void)
{
    ASSERT_EQ(ESP_OK, uac_service_start_from_kconfig());
    puts("uac_service tests passed");
    return 0;
}
