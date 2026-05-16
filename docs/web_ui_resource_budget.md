# Web UI resource budget

The Web UI is an embedded-device control surface, not a general web-app host. These budgets are non-negotiable review gates for each UI redesign phase.

## Current firmware values

These values are intentionally documented before feature work so later changes are explicit:

| Resource | Current value | Source |
| --- | ---: | --- |
| `RULE_WEB_MAX_BODY` | 512 bytes | `src/rules/rule_web.c` |
| `RULE_WEB_MAX_RESPONSE` | 16,384 bytes | `src/rules/rule_web.c` |
| `httpd_config_t.max_uri_handlers` | 17 | `rule_web_start()` |
| `httpd_config_t.stack_size` | 8,192 bytes | `rule_web_start()` |

The current route table already consumes the 17 registered URI-handler slots, so Phase 1 ships as a single generated HTML document instead of adding CSS/JS asset routes.

## Accepted budgets

| Budget | Target | Hard ceiling | Enforcement |
| --- | ---: | ---: | --- |
| Added firmware ROM for generated Web UI assets | < 32 KiB | 64 KiB | `tools/check_web_ui_budget.py` |
| Generated single-document asset | Fit in existing response cap for host tests | 16,384 bytes | `tools/check_web_ui_budget.py` |
| Initial HTML shell source after minification, excluding inlined CSS/JS | < 8 KiB | 8 KiB | `tools/check_web_ui_budget.py` |
| CSS source before minification | < 12 KiB | 12 KiB | `tools/check_web_ui_budget.py` |
| JavaScript source before minification | < 25 KiB | 25 KiB | `tools/check_web_ui_budget.py` |
| Response buffer size | keep `RULE_WEB_MAX_RESPONSE` at 16 KiB | 16 KiB until reviewed | `tools/check_web_ui_budget.py` |
| Request body size | keep `RULE_WEB_MAX_BODY` at 512 bytes | 512 bytes until `/api/config/op` exists | `tools/check_web_ui_budget.py` |
| Peak additional heap during normal page load | < 4 KiB | 8 KiB | code review + hardware measurement |
| Peak additional heap during save/import | < 8 KiB | 16 KiB | code review + hardware measurement |
| Persistent background polling | no faster than 1 second | 1 second minimum interval | code review |


## Current Phase 1 measurements

Measured by `webui/build_webui.py --check` and `tools/check_web_ui_budget.py` for the extracted, behavior-equivalent single-document UI:

| Measurement | Current value | Budget status |
| --- | ---: | --- |
| Generated `webui_index_html` asset | 14,566 bytes | Under 32 KiB target |
| Minified HTML shell source, excluding injected CSS/JS | 5,902 bytes | Under 8 KiB hard ceiling |
| CSS source before minification | 1,666 bytes | Under 12 KiB hard ceiling |
| JavaScript source before minification | 7,009 bytes | Under 25 KiB hard ceiling |
| URI handlers added for UI assets | 0 | Preserves existing 17-handler table |

On ESP-IDF, `GET /` sends the generated const asset directly with `httpd_resp_send()` and does not allocate the `RULE_WEB_MAX_RESPONSE` heap buffer used by JSON/API routes. Host tests still exercise `rule_web_handle_request()` by copying the generated asset into the caller-provided test buffer.

## Design rules

- No frontend framework or runtime CSS framework.
- Prefer generated `const` assets stored in flash/ROM.
- Do not duplicate `automation_config_t` as a second long-lived Web UI model.
- Keep temporary parse/import buffers short-lived.
- Prefer existing APIs; add aggregate or operation APIs only when bounded payloads require them.
- Prefer manual refresh for expensive diagnostics, Wi-Fi scans, and hardware probes.
- Any change that raises `RULE_WEB_MAX_BODY`, `RULE_WEB_MAX_RESPONSE`, route count, or generated asset size must update this document and justify the impact.
