#include "uac_esp_device.h"

#include "sdkconfig.h"

#if CONFIG_APP_USB_UAC_DEVICE
#include "usb_device_uac.h"
#endif

esp_err_t uac_esp_device_start(uac_device_adapter_t *adapter)
{
    if (adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_APP_USB_UAC_DEVICE
    uac_device_descriptor_plan_t plan = uac_device_adapter_descriptor_plan(adapter);
    uac_device_config_t config = {
        .skip_tinyusb_init = plan.skip_tinyusb_init,
        .output_cb = plan.output_enabled ? uac_device_adapter_output_cb : NULL,
        .input_cb = plan.input_enabled ? uac_device_adapter_input_cb : NULL,
        .set_mute_cb = uac_device_adapter_set_mute_cb,
        .set_volume_cb = uac_device_adapter_set_volume_cb,
        .cb_ctx = adapter,
    };
    return uac_device_init(&config);
#else
    (void)adapter;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
