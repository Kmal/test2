#include "action_ir.h"
#include "board_sticks3.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "driver/rmt_tx.h"
#include "esp_err.h"
#endif

#define NEC_LEADER_HIGH_US 9000u
#define NEC_LEADER_LOW_US 4500u
#define NEC_BIT_HIGH_US 560u
#define NEC_ZERO_LOW_US 560u
#define NEC_ONE_LOW_US 1690u
#define NEC_STOP_HIGH_US 560u
#define NEC_REPEAT_HIGH_US 9000u
#define NEC_REPEAT_LOW_US 2250u
#define NEC_SYMBOL_COUNT 34u
#define NEC_REPEAT_SYMBOL_COUNT 2u

bool action_ir_validate(const action_ir_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (config->protocol != RULE_IR_PROTOCOL_NEC) {
        return false;
    }
    if (config->carrier_hz < 30000u || config->carrier_hz > 60000u) {
        return false;
    }
    if (config->repeat_count > 5u || config->timeout_ms == 0 || config->timeout_ms > 1000u) {
        return false;
    }
    return true;
}

bool action_ir_config_from_action(const rule_action_t *action, action_ir_config_t *config)
{
    if (action == NULL || config == NULL || action->type != RULE_ACTION_IR_SEND) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    config->protocol = action->ir_protocol;
    config->carrier_hz = action->ir_carrier_hz;
    config->address = action->ir_address;
    config->command = action->ir_command;
    config->repeat_count = action->ir_repeat_count;
    config->timeout_ms = action->timeout_ms;
    return action_ir_validate(config);
}

#ifdef ESP_PLATFORM
static rmt_symbol_word_t nec_symbol(uint32_t high_us, uint32_t low_us)
{
    rmt_symbol_word_t symbol = {
        .level0 = 1,
        .duration0 = high_us,
        .level1 = 0,
        .duration1 = low_us,
    };
    return symbol;
}

static void build_nec_frame(const action_ir_config_t *config, rmt_symbol_word_t *symbols)
{
    uint32_t payload = ((uint32_t)config->address & 0xffu) |
                       ((((uint32_t)~config->address) & 0xffu) << 8u) |
                       (((uint32_t)config->command & 0xffu) << 16u) |
                       ((((uint32_t)~config->command) & 0xffu) << 24u);
    symbols[0] = nec_symbol(NEC_LEADER_HIGH_US, NEC_LEADER_LOW_US);
    for (size_t i = 0; i < 32u; ++i) {
        symbols[i + 1u] = nec_symbol(NEC_BIT_HIGH_US, (payload & (1u << i)) ? NEC_ONE_LOW_US : NEC_ZERO_LOW_US);
    }
    symbols[33] = nec_symbol(NEC_STOP_HIGH_US, 0u);
}

static bool transmit_symbols(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder,
                             const rmt_symbol_word_t *symbols, size_t symbol_count, uint32_t timeout_ms)
{
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(channel, encoder, symbols, symbol_count * sizeof(symbols[0]), &tx_config);
    if (err != ESP_OK) {
        return false;
    }
    return rmt_tx_wait_all_done(channel, timeout_ms) == ESP_OK;
}
#endif

bool action_ir_send(const action_ir_config_t *config, const rule_event_t *event)
{
    (void)event;
    if (!action_ir_validate(config)) {
        return false;
    }
#ifdef ESP_PLATFORM
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;
    rmt_tx_channel_config_t channel_config = {
        .gpio_num = BOARD_IR_TX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000u,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    if (rmt_new_tx_channel(&channel_config, &channel) != ESP_OK) {
        return false;
    }
    rmt_carrier_config_t carrier = {
        .frequency_hz = config->carrier_hz,
        .duty_cycle = 0.33f,
    };
    rmt_copy_encoder_config_t encoder_config = {0};
    bool ok = rmt_apply_carrier(channel, &carrier) == ESP_OK &&
              rmt_new_copy_encoder(&encoder_config, &encoder) == ESP_OK &&
              rmt_enable(channel) == ESP_OK;
    if (ok) {
        rmt_symbol_word_t frame[NEC_SYMBOL_COUNT];
        build_nec_frame(config, frame);
        ok = transmit_symbols(channel, encoder, frame, NEC_SYMBOL_COUNT, config->timeout_ms);
        rmt_symbol_word_t repeat[NEC_REPEAT_SYMBOL_COUNT] = {
            nec_symbol(NEC_REPEAT_HIGH_US, NEC_REPEAT_LOW_US),
            nec_symbol(NEC_STOP_HIGH_US, 0u),
        };
        for (uint8_t i = 0; ok && i < config->repeat_count; ++i) {
            ok = transmit_symbols(channel, encoder, repeat, NEC_REPEAT_SYMBOL_COUNT, config->timeout_ms);
        }
    }
    if (channel != NULL) {
        (void)rmt_disable(channel);
    }
    if (encoder != NULL) {
        (void)rmt_del_encoder(encoder);
    }
    if (channel != NULL) {
        (void)rmt_del_channel(channel);
    }
    return ok;
#else
    return false;
#endif
}

bool action_ir_send_event(const rule_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    action_ir_config_t config;
    if (!action_ir_config_from_action(&event->action_config, &config)) {
        return false;
    }
    return action_ir_send(&config, event);
}
