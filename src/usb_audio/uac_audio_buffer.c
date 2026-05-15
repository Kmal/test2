#include "uac_audio_buffer.h"

#include <string.h>

esp_err_t uac_audio_buffer_init(uac_audio_buffer_t *buffer, uint8_t *storage, size_t capacity)
{
    if (buffer == NULL || storage == NULL || capacity == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer->storage = storage;
    buffer->capacity = capacity;
    uac_audio_buffer_reset(buffer);
    return ESP_OK;
}

void uac_audio_buffer_reset(uac_audio_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    buffer->head = 0;
    buffer->tail = 0;
    buffer->used = 0;
    buffer->underruns = 0;
    buffer->overruns = 0;
    buffer->bytes_written = 0;
    buffer->bytes_read = 0;
}

size_t uac_audio_buffer_write(uac_audio_buffer_t *buffer, const uint8_t *src, size_t len)
{
    if (buffer == NULL || src == NULL || buffer->storage == NULL || buffer->capacity == 0 || len == 0) {
        return 0;
    }
    size_t written = 0;
    while (written < len && buffer->used < buffer->capacity) {
        buffer->storage[buffer->head] = src[written++];
        buffer->head = (buffer->head + 1) % buffer->capacity;
        buffer->used++;
    }
    if (written < len) {
        buffer->overruns++;
    }
    buffer->bytes_written += (uint32_t)written;
    return written;
}

size_t uac_audio_buffer_read(uac_audio_buffer_t *buffer, uint8_t *dest, size_t len)
{
    if (buffer == NULL || dest == NULL || buffer->storage == NULL || buffer->capacity == 0 || len == 0) {
        return 0;
    }
    size_t read = 0;
    while (read < len && buffer->used > 0) {
        dest[read++] = buffer->storage[buffer->tail];
        buffer->tail = (buffer->tail + 1) % buffer->capacity;
        buffer->used--;
    }
    if (read < len) {
        buffer->underruns++;
    }
    buffer->bytes_read += (uint32_t)read;
    return read;
}

size_t uac_audio_buffer_read_or_silence(uac_audio_buffer_t *buffer, uint8_t *dest, size_t len)
{
    if (dest == NULL || len == 0) {
        return 0;
    }
    size_t read = uac_audio_buffer_read(buffer, dest, len);
    if (read < len) {
        memset(dest + read, 0, len - read);
    }
    return len;
}

uac_audio_buffer_stats_t uac_audio_buffer_get_stats(const uac_audio_buffer_t *buffer)
{
    if (buffer == NULL) {
        return (uac_audio_buffer_stats_t){0};
    }
    return (uac_audio_buffer_stats_t){
        .underruns = buffer->underruns,
        .overruns = buffer->overruns,
        .bytes_written = buffer->bytes_written,
        .bytes_read = buffer->bytes_read,
        .used = buffer->used,
        .capacity = buffer->capacity,
    };
}
