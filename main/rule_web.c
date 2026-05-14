#include "rule_web.h"
#include "capability_registry.h"
#include "action_http.h"
#include "app_wifi.h"
#include "app_time.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#include "esp_log.h"

#define RULE_WEB_MAX_BODY 512u
#define RULE_WEB_MAX_RESPONSE 16384u

static const char *TAG = "RULE_WEB";

static esp_err_t rule_web_http_handler(httpd_req_t *req)
{
    rule_web_t *web = (rule_web_t *)req->user_ctx;
    char *body = calloc(1, RULE_WEB_MAX_BODY);
    char *response = malloc(RULE_WEB_MAX_RESPONSE);
    if (body == NULL || response == NULL) {
        free(body);
        free(response);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "rule web allocation failed");
        return ESP_FAIL;
    }

    if (req->method == HTTP_POST) {
        size_t received = 0;
        while (received < req->content_len && received < RULE_WEB_MAX_BODY - 1u) {
            int chunk = httpd_req_recv(req, body + received, (RULE_WEB_MAX_BODY - 1u) - received);
            if (chunk <= 0) {
                free(body);
                free(response);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to read body");
                return ESP_FAIL;
            }
            received += (size_t)chunk;
        }
        body[received] = '\0';
        if (received < req->content_len) {
            free(body);
            free(response);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body too large");
            return ESP_FAIL;
        }
    }
    rule_web_method_t method = req->method == HTTP_POST ? RULE_WEB_METHOD_POST : RULE_WEB_METHOD_GET;
    if (!rule_web_handle_request(web, method, req->uri, body, response, RULE_WEB_MAX_RESPONSE)) {
        free(body);
        free(response);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "rule web request failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, strcmp(req->uri, "/") == 0 ? "text/html" : (strcmp(req->uri, "/favicon.ico") == 0 ? "image/x-icon" : "application/json"));
    esp_err_t err = httpd_resp_sendstr(req, response) == ESP_OK ? ESP_OK : ESP_FAIL;
    free(body);
    free(response);
    return err;
}

static bool register_uri(httpd_handle_t server, const char *uri, httpd_method_t method, rule_web_t *web)
{
    httpd_uri_t handler = {
        .uri = uri,
        .method = method,
        .handler = rule_web_http_handler,
        .user_ctx = web,
    };
    esp_err_t err = httpd_register_uri_handler(server, &handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register URI failed: method=%d uri=%s err=%s", (int)method, uri, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "registered URI: method=%d uri=%s", (int)method, uri);
    return true;
}
#endif


bool rule_web_start(rule_web_t *web, rule_runtime_t *runtime, rule_config_store_t *store)
{
    if (web == NULL || runtime == NULL || store == NULL) {
        return false;
    }
    memset(web, 0, sizeof(*web));
    web->runtime = runtime;
    web->store = store;
#ifdef ESP_PLATFORM
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 17;
    config.stack_size = 8192;
    ESP_LOGI(TAG, "starting HTTP server: port=%u max_uri_handlers=%u stack=%u",
             (unsigned)config.server_port,
             (unsigned)config.max_uri_handlers,
             (unsigned)config.stack_size);
    esp_err_t err = httpd_start(&web->server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "HTTP server started");
    if (!register_uri(web->server, "/", HTTP_GET, web) ||
        !register_uri(web->server, "/favicon.ico", HTTP_GET, web) ||
        !register_uri(web->server, "/api/config", HTTP_GET, web) ||
        !register_uri(web->server, "/api/config", HTTP_POST, web) ||
        !register_uri(web->server, "/api/capabilities", HTTP_GET, web) ||
        !register_uri(web->server, "/api/status", HTTP_GET, web) ||
        !register_uri(web->server, "/api/time", HTTP_GET, web) ||
        !register_uri(web->server, "/api/time", HTTP_POST, web) ||
        !register_uri(web->server, "/api/wifi/status", HTTP_GET, web) ||
        !register_uri(web->server, "/api/wifi/scan", HTTP_POST, web) ||
        !register_uri(web->server, "/api/wifi/connect", HTTP_POST, web) ||
        !register_uri(web->server, "/api/wifi/forget", HTTP_POST, web) ||
        !register_uri(web->server, "/api/wifi/ap", HTTP_POST, web) ||
        !register_uri(web->server, "/api/wifi/mode", HTTP_POST, web) ||
        !register_uri(web->server, "/api/rules/test", HTTP_POST, web) ||
        !register_uri(web->server, "/api/gpio/test", HTTP_POST, web) ||
        !register_uri(web->server, "/api/hat/probe", HTTP_POST, web)) {
        ESP_LOGE(TAG, "HTTP server URI registration failed; stopping server");
        httpd_stop(web->server);
        web->server = NULL;
        return false;
    }
#endif
    web->started = true;
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "rule web ready");
#endif
    return true;
}

void rule_web_stop(rule_web_t *web)
{
    if (web != NULL) {
#ifdef ESP_PLATFORM
        if (web->server != NULL) {
            ESP_LOGI(TAG, "stopping HTTP server");
            httpd_stop(web->server);
            web->server = NULL;
        }
#endif
        web->started = false;
    }
}


static const char s_rule_setup_page[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>StickS3 Control</title>"
    "<style>*{box-sizing:border-box}body{margin:0;background:#0b1020;color:#eef2ff;font-family:system-ui,-apple-system,Segoe UI,sans-serif}main{max-width:76rem;margin:auto;padding:1rem}.hero{display:flex;justify-content:space-between;gap:1rem;align-items:center}.hero h1{margin:.2rem 0;font-size:1.8rem}.hero p,label{color:#94a3b8}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(22rem,1fr));gap:1rem}.card,.panel{background:#151b2e;border:1px solid #26324d;border-radius:1rem;padding:1rem;margin:.7rem 0}input,select,textarea,button{font:inherit}input,select,textarea{width:100%;margin-top:.25rem;padding:.55rem;border-radius:.6rem;border:1px solid #26324d;background:#0f172a;color:#eef2ff}textarea{min-height:10rem;font-family:ui-monospace,monospace}button{margin:.25rem .25rem .25rem 0;padding:.55rem .75rem;border:0;border-radius:.65rem;background:#2563eb;color:white}button.secondary{background:#334155}button.danger{background:#b91c1c}button:disabled{opacity:.55}.tabs{display:flex;gap:.5rem;flex-wrap:wrap}.banner{display:none;border-radius:.75rem;padding:.7rem .9rem;margin:.75rem 0}.banner.info{display:block;background:#0c4a6e}.banner.ok{display:block;background:#14532d}.banner.warn{display:block;background:#713f12}.banner.err{display:block;background:#7f1d1d}.kv{display:grid;grid-template-columns:7rem 1fr;gap:.3rem .7rem}.kv b{color:#94a3b8}.status{white-space:pre-wrap;overflow:auto;max-height:16rem;background:#020617;border:1px solid #26324d;border-radius:.75rem;padding:.75rem;color:#cbd5e1}.network-row{display:block;width:100%;text-align:left;background:#1e293b}.pill{padding:.2rem .5rem;border-radius:999px;background:#334155;color:#cbd5e1}</style></head>"
    "<body><main><div class=\"hero\"><div><h1>StickS3 Control</h1><p>Network and automation dashboard.</p></div><span id=\"connection_pill\" class=\"pill\">Loading</span></div><div id=\"banner\" class=\"banner info\">Loading device status...</div>"
    "<section class=\"card\" id=\"network\"><h2>Network setup</h2><div id=\"net_summary\" class=\"kv\"></div><nav class=\"tabs\"><button id=\"tab_wifi\" type=\"button\">Wi-Fi Mode</button><button id=\"tab_ap\" type=\"button\">AP Mode</button><button id=\"tab_net_status\" type=\"button\" class=\"secondary\">Raw Status</button></nav>"
    "<div id=\"panel_wifi\" class=\"panel\"><h3>Join Wi-Fi</h3><button id=\"wifi_scan\" type=\"button\">Scan Nearby Wi-Fi</button><div id=\"wifi_networks\"></div>"
    "<label>Selected SSID <input id=\"wifi_ssid\" maxlength=\"32\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label>"
    "<label>Password <input id=\"wifi_password\" type=\"password\" maxlength=\"63\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label>"
    "<button id=\"wifi_connect\" type=\"button\">Connect and Save</button><button id=\"mode_wifi\" type=\"button\" class=\"secondary\">Use Wi-Fi Mode</button>"
    "<fieldset><legend>Saved Wi-Fi</legend><p id=\"wifi_saved\">Saved:-</p><button id=\"wifi_reconnect\" type=\"button\" class=\"secondary\">Reconnect saved Wi-Fi</button><button id=\"wifi_forget\" type=\"button\" class=\"danger\">Forget Saved Credentials</button></fieldset></div>"
    "<div id=\"panel_ap\" class=\"panel\" hidden><h3>Setup hotspot</h3><label>AP Name <input id=\"ap_ssid\" maxlength=\"32\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label>"
    "<label>AP Password <input id=\"ap_password\" type=\"password\" maxlength=\"63\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label><label>Channel <input id=\"ap_channel\" type=\"number\" min=\"1\" max=\"13\" value=\"6\"></label><button id=\"ap_start\" type=\"button\">Start AP Mode</button><button id=\"mode_ap\" type=\"button\" class=\"secondary\">Use AP Mode</button></div>"
    "<div id=\"panel_net_status\" class=\"panel\" hidden><h3>Raw network status</h3><pre id=\"wifi_status\" class=\"status\"></pre></div><h3>Last operation</h3><pre id=\"op_result\" class=\"status\">No operation yet.</pre></section>"
    "<section class=\"card\" id=\"time_settings\"><h2>Time settings</h2><div id=\"time_summary\" class=\"kv\"></div><label>Timezone <select id=\"timezone\"><option value=\"UTC\">UTC</option><option value=\"UTC-8\">Pacific Time (UTC-8)</option><option value=\"UTC-7\">Mountain Time (UTC-7)</option><option value=\"UTC-6\">Central Time (UTC-6)</option><option value=\"UTC-5\">Eastern Time (UTC-5)</option><option value=\"UTC+0\">London/Reykjavik (UTC+0)</option><option value=\"UTC+1\">Central Europe (UTC+1)</option><option value=\"UTC+2\">Eastern Europe/Africa (UTC+2)</option><option value=\"UTC+3\">Moscow/Arabia (UTC+3)</option><option value=\"UTC+5:30\">India (UTC+5:30)</option><option value=\"UTC+8\">China/Singapore (UTC+8)</option><option value=\"UTC+9\">Japan/Korea (UTC+9)</option><option value=\"UTC+10\">Eastern Australia (UTC+10)</option><option value=\"UTC+12\">New Zealand (UTC+12)</option></select></label><p>Clock uses 24-hour HH:MM format. Choose a common UTC offset; UTC-8 means eight hours behind UTC.</p><button id=\"save_time\" type=\"button\">Save Timezone</button><pre id=\"time_status\" class=\"status\"></pre></section>"
    "<div class=\"grid\"><section class=\"card\"><h2>Automation rule</h2><label>Rule name <input id=\"rule_name\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label><label><input id=\"rule_enabled\" type=\"checkbox\" checked> Enabled</label>"
    "<h3>WHEN trigger</h3><label>Trigger source <select id=\"source\"></select></label><label>Comparator <select id=\"comparator\"><option value=\"eq\">equals</option><option value=\"ne\">not equals</option><option value=\"gt\">greater than</option><option value=\"gte\">greater or equal</option><option value=\"lt\">less than</option><option value=\"lte\">less or equal</option></select></label><label>Threshold <input id=\"threshold\" value=\"true\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"><small>true/false or integer</small></label>"
    "<fieldset><legend>GPIO safety / conflict check</legend><label>GPIO pin <input id=\"gpio_pin\" type=\"number\" value=\"4\"></label><label>GPIO profile <select id=\"gpio_profile\"></select></label><label>Debounce ms <input id=\"gpio_debounce_ms\" type=\"number\" value=\"20\"></label><label><input id=\"gpio_active_low\" type=\"checkbox\"> Active low</label><button id=\"gpio_test\" type=\"button\" class=\"secondary\">Test GPIO safety</button></fieldset>"
    "<h3>DO action</h3><label>Action <select id=\"action\"></select></label><label>HTTP URL <input id=\"http_url\" size=\"48\" placeholder=\"https://example.invalid/hook\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label><label>HTTP bearer token <input id=\"http_bearer_token\" type=\"password\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></label><button id=\"save_config\" type=\"button\">Save Rule</button><button id=\"test_rule\" type=\"button\" class=\"secondary\">Test First Rule</button></section>"
    "<section class=\"card\"><h2>HAT capability status</h2><label>HAT source <select id=\"hat_source\"></select></label><button id=\"hat_probe\" type=\"button\" class=\"secondary\">Probe HAT</button><pre id=\"hat_status\" class=\"status\"></pre><h2>Import / export JSON</h2><textarea id=\"config_json\"></textarea><button id=\"load_config\" type=\"button\" class=\"secondary\">Reload</button><button id=\"export_config\" type=\"button\" class=\"secondary\">Export JSON</button><button id=\"import_config\" type=\"button\">Import JSON</button><h2>Status</h2><pre id=\"status\" class=\"status\"></pre><pre id=\"capabilities\" class=\"status\"></pre></section></div>"
    "<script>const $=id=>document.getElementById(id);let caps={sources:[],actions:[],gpio_profiles:[],hat_sources:[]};"
    "function banner(kind,msg){const b=$('banner');b.className='banner '+(kind||'info');b.textContent=msg||''}function dump(id,d){$(id).textContent=typeof d==='string'?d:JSON.stringify(d,null,2)}function busy(on){document.querySelectorAll('button').forEach(b=>b.disabled=!!on)}"
    "async function api(u,o){busy(true);try{const r=await fetch(u,Object.assign({headers:{'Content-Type':'application/json'}},o||{}));const t=await r.text();let d;try{d=t?JSON.parse(t):{};}catch(e){throw new Error('Bad response: '+t.slice(0,120))}if(!r.ok)throw new Error(d.error||r.statusText);return d}catch(e){banner('err',e.message);dump('op_result',{ok:false,error:e.message});throw e}finally{busy(false)}}"
    "function opt(sel,v,t){const o=document.createElement('option');o.value=v;o.textContent=t||v;sel.appendChild(o)}function showNetPanel(n){['wifi','ap','net_status'].forEach(p=>$('panel_'+p).hidden=p!==n)}"
    "function fill(){['source','action','gpio_profile','hat_source'].forEach(i=>$(i).innerHTML='');(caps.sources||[]).forEach(v=>opt($('source'),v));(caps.actions||[]).forEach(v=>opt($('action'),v));(caps.gpio_profiles||[]).forEach(g=>opt($('gpio_profile'),g.name||g,g.supported===false?(g.name+' (disabled)'):g.name||g));(caps.hat_sources||[]).forEach(h=>opt($('hat_source'),h.name,h.supported?(h.name+' (present)'):(h.name+' ('+h.reason+')')))}"
    "function thresholdBody(b){const v=$('threshold').value.trim();if(v==='true'||v==='false'){b.threshold_kind='bool';b.threshold_bool=v==='true'}else{const n=parseInt(v||'0',10);b.threshold_kind='i32';b.threshold_i32=Number.isFinite(n)?n:0}}"
    "function body(){let b={enabled:$('rule_enabled').checked,name:$('rule_name').value,source:$('source').value,action:$('action').value,comparator:$('comparator').value,http_url:$('http_url').value,http_bearer_token:$('http_bearer_token').value};thresholdBody(b);if(b.source.startsWith('gpio.'))Object.assign(b,{gpio_pin:+$('gpio_pin').value,gpio_profile:$('gpio_profile').value,gpio_debounce_ms:+$('gpio_debounce_ms').value,gpio_active_low:$('gpio_active_low').checked});return JSON.stringify(b)}"
    "function applyConfig(c){if(!c)return;$('config_json').value=JSON.stringify(c,null,2);if(c.source)$('source').value=c.source;if(c.action)$('action').value=c.action;if(c.name)$('rule_name').value=c.name;$('rule_enabled').checked=c.enabled!==false;if(c.comparator)$('comparator').value=c.comparator;$('threshold').value=c.threshold_kind==='i32'?String(c.threshold_i32||0):String(c.threshold_bool!==false);if(c.gpio_pin!==undefined)$('gpio_pin').value=c.gpio_pin;if(c.gpio_profile)$('gpio_profile').value=c.gpio_profile;$('gpio_active_low').checked=!!c.gpio_active_low;if(c.gpio_debounce_ms!==undefined)$('gpio_debounce_ms').value=c.gpio_debounce_ms;if(c.http_url)$('http_url').value=c.http_url}"
    "function renderNetworks(ns){const root=$('wifi_networks');root.innerHTML='';(ns||[]).forEach(n=>{const row=document.createElement('button');row.type='button';row.className='network-row';row.textContent=(n.ssid||'(hidden)')+' '+n.rssi+' dBm ch '+n.channel+(n.secure?' LOCK':' OPEN');row.onclick=()=>{$('wifi_ssid').value=n.ssid||''};root.appendChild(row)})}"
    "function netSummary(d){$('connection_pill').textContent=d.sta_connected?'Wi-Fi connected':(d.ap_started?'AP setup active':'Network off');$('net_summary').innerHTML='<b>Mode</b><span>'+d.mode+'</span><b>Station</b><span>'+(d.sta_connected?(d.sta_ssid+' @ '+d.sta_ip):(d.sta_ssid||'-'))+'</span><b>Setup AP</b><span>'+(d.ap_started?(d.ap_ssid+' @ '+d.ap_ip):'off')+'</span><b>Web URL</b><span>'+(d.web_url||'-')+'</span>'}"
    "function timeSummary(d){$('time_summary').innerHTML='<b>Timezone</b><span>'+(d.timezone||'UTC')+'</span><b>Time</b><span>'+(d.time_24h||'--:--')+'</span><b>Format</b><span>24-hour</span>';const z=d.timezone||'UTC';const sel=$('timezone');if([...sel.options].some(o=>o.value===z))sel.value=z;else sel.value='UTC'}"
    "async function timeStatus(){const d=await api('/api/time');dump('time_status',d);timeSummary(d);return d}"
    "async function wifiStatus(){const d=await api('/api/wifi/status');dump('wifi_status',d);netSummary(d);if(d.ap_ssid&&!$('ap_ssid').value)$('ap_ssid').value=d.ap_ssid;if(d.ap_channel&&!$('ap_channel').value)$('ap_channel').value=d.ap_channel;$('wifi_saved').textContent='Saved: '+(d.sta_ssid||'-');return d}"
    "async function op(p,msg){try{banner('info',msg||'Working...');const d=await p;dump('op_result',d);banner(d.ok===false?'warn':'ok',d.ok===false?(d.error||'Operation failed'):'Operation complete');return d}catch(e){return null}}async function setWifiMode(m){await op(api('/api/wifi/mode',{method:'POST',body:JSON.stringify({mode:m})}),'Changing Wi-Fi mode...');await wifiStatus()}"
    "async function refresh(){banner('info','Refreshing...');caps=await api('/api/capabilities');fill();dump('capabilities',caps);dump('status',await api('/api/status'));applyConfig(await api('/api/config'));await wifiStatus();await timeStatus();banner('ok','Ready')}"
    "$('tab_wifi').onclick=()=>showNetPanel('wifi');$('tab_ap').onclick=()=>showNetPanel('ap');$('tab_net_status').onclick=()=>showNetPanel('net_status');$('mode_wifi').onclick=()=>setWifiMode('wifi');$('mode_ap').onclick=()=>setWifiMode('ap');"
    "$('wifi_scan').onclick=async()=>{const d=await op(api('/api/wifi/scan',{method:'POST'}),'Scanning Wi-Fi...');if(d)renderNetworks(d.networks||[])};$('wifi_connect').onclick=async()=>{await op(api('/api/wifi/connect',{method:'POST',body:JSON.stringify({ssid:$('wifi_ssid').value,password:$('wifi_password').value})}),'Connecting Wi-Fi...');await wifiStatus()};"
    "$('wifi_reconnect').onclick=()=>setWifiMode('wifi');$('wifi_forget').onclick=async()=>{await op(api('/api/wifi/forget',{method:'POST'}),'Forgetting credentials...');await wifiStatus()};$('ap_start').onclick=async()=>{await op(api('/api/wifi/ap',{method:'POST',body:JSON.stringify({ssid:$('ap_ssid').value,password:$('ap_password').value,channel:+$('ap_channel').value})}),'Starting AP...');await wifiStatus()};"
    "$('load_config').onclick=refresh;$('export_config').onclick=async()=>applyConfig(await api('/api/config'));$('save_time').onclick=async()=>{const d=await op(api('/api/time',{method:'POST',body:JSON.stringify({timezone:$('timezone').value})}),'Saving timezone...');if(d)timeSummary(d)};$('save_config').onclick=async()=>{const d=await op(api('/api/config',{method:'POST',body:body()}),'Saving rule...');if(d)applyConfig(d)};$('import_config').onclick=async()=>{const d=await op(api('/api/config',{method:'POST',body:$('config_json').value}),'Importing JSON...');if(d)applyConfig(d)};"
    "$('test_rule').onclick=async()=>dump('status',await op(api('/api/rules/test',{method:'POST'}),'Testing rule...'));$('gpio_test').onclick=async()=>dump('status',await op(api('/api/gpio/test',{method:'POST',body:body()}),'Checking GPIO...'));$('hat_probe').onclick=async()=>dump('hat_status',await op(api('/api/hat/probe',{method:'POST',body:JSON.stringify({source:$('hat_source').value})}),'Probing HAT...'));refresh().catch(()=>{});</script></main></body></html>";

bool rule_web_get_status_json(const rule_web_t *web, char *out, size_t out_len)
{
    if (web == NULL || out == NULL || out_len == 0) {
        return false;
    }
    char wifi[512];
    if (!app_wifi_status_json(wifi, sizeof(wifi))) {
        (void)snprintf(wifi, sizeof(wifi), "{\"enabled\":false}");
    }
    action_result_t result = rule_runtime_get_last_action_result(web->runtime);
    const int written = snprintf(out, out_len,
                                 "{\"started\":%s,\"last_action\":%d,\"capabilities_ready\":true,\"http_network_ready\":%s,\"wifi\":%s}",
                                 web->started ? "true" : "false", (int)result.code,
                                 action_http_network_ready() ? "true" : "false", wifi);
    return written > 0 && (size_t)written < out_len;
}


static const char *comparator_name(rule_comparator_t comparator)
{
    switch (comparator) {
    case RULE_COMPARATOR_EQ:
        return "eq";
    case RULE_COMPARATOR_NE:
        return "ne";
    case RULE_COMPARATOR_GT:
        return "gt";
    case RULE_COMPARATOR_GTE:
        return "gte";
    case RULE_COMPARATOR_LT:
        return "lt";
    case RULE_COMPARATOR_LTE:
        return "lte";
    default:
        return "invalid";
    }
}

static const char *gpio_profile_name(rule_gpio_profile_t profile)
{
    switch (profile) {
    case RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW:
        return "digital_high_low";
    case RULE_GPIO_PROFILE_RISING_EDGE:
        return "rising_edge";
    case RULE_GPIO_PROFILE_FALLING_EDGE:
        return "falling_edge";
    case RULE_GPIO_PROFILE_DEBOUNCED_CONTACT:
        return "debounced_contact";
    case RULE_GPIO_PROFILE_PULSE_COUNT:
        return "pulse_count";
    case RULE_GPIO_PROFILE_FREQUENCY:
        return "frequency";
    default:
        return "none";
    }
}

static bool json_appendf(char **cursor, size_t *remaining, const char *fmt, ...)
{
    if (cursor == NULL || *cursor == NULL || remaining == NULL || *remaining == 0 || fmt == NULL) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(*cursor, *remaining, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }

    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

static bool json_append_string(char **cursor, size_t *remaining, const char *value)
{
    if (!json_appendf(cursor, remaining, "\"")) {
        return false;
    }

    if (value != NULL) {
        for (const unsigned char *ch = (const unsigned char *)value; *ch != '\0'; ++ch) {
            switch (*ch) {
            case '\"':
                if (!json_appendf(cursor, remaining, "\\\"")) {
                    return false;
                }
                break;
            case '\\':
                if (!json_appendf(cursor, remaining, "\\\\")) {
                    return false;
                }
                break;
            case '\b':
                if (!json_appendf(cursor, remaining, "\\b")) {
                    return false;
                }
                break;
            case '\f':
                if (!json_appendf(cursor, remaining, "\\f")) {
                    return false;
                }
                break;
            case '\n':
                if (!json_appendf(cursor, remaining, "\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!json_appendf(cursor, remaining, "\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!json_appendf(cursor, remaining, "\\t")) {
                    return false;
                }
                break;
            default:
                if (*ch < 0x20u) {
                    if (!json_appendf(cursor, remaining, "\\u%04x", (unsigned)*ch)) {
                        return false;
                    }
                } else if (!json_appendf(cursor, remaining, "%c", *ch)) {
                    return false;
                }
                break;
            }
        }
    }

    return json_appendf(cursor, remaining, "\"");
}

static bool write_config_json(const automation_config_t *config, char *out, size_t out_len)
{
    if (config == NULL || out == NULL || out_len == 0) {
        return false;
    }
    const automation_rule_t *rule = config->rule_count > 0 ? &config->rules[0] : NULL;
    const rule_action_t *action = (rule != NULL && rule->action_count > 0) ? &rule->actions[0] : NULL;
    const rule_value_t threshold = rule != NULL ? rule->when.threshold : rule_value_bool(false);
    const bool threshold_bool = threshold.kind == RULE_VALUE_BOOL && threshold.as.bool_value;
    const int32_t threshold_i32 = threshold.kind == RULE_VALUE_I32 ? threshold.as.i32_value : 0;
    const char *threshold_kind = threshold.kind == RULE_VALUE_I32 ? "i32" : "bool";
    const char *auth_state = (action != NULL && action->http_bearer_token[0] != '\0') ? "masked" : "empty";
    const rule_gpio_config_t empty_gpio = {.pin = RULE_GPIO_UNUSED_PIN, .profile = RULE_GPIO_PROFILE_NONE};
    const rule_gpio_config_t *gpio = rule != NULL ? &rule->when.gpio : &empty_gpio;
    char *cursor = out;
    size_t remaining = out_len;

    bool ok = json_appendf(&cursor, &remaining,
                           "{\"schema_version\":%lu,\"rule_count\":%u,"
                           "\"enabled\":%s,\"name\":",
                           (unsigned long)config->schema_version,
                           (unsigned)config->rule_count,
                           (rule != NULL && rule->enabled) ? "true" : "false") &&
              json_append_string(&cursor, &remaining, rule != NULL ? rule->name : "") &&
              json_appendf(&cursor, &remaining,
                           ",\"source\":\"%s\",\"action\":\"%s\","
                           "\"comparator\":\"%s\",\"threshold_kind\":\"%s\","
                           "\"threshold_bool\":%s,\"threshold_i32\":%ld,"
                           "\"cooldown_ms\":%lu,\"sustain_ms\":%lu,"
                           "\"gpio_pin\":%d,\"gpio_profile\":\"%s\","
                           "\"gpio_active_low\":%s,\"gpio_debounce_ms\":%lu,\"http_url\":",
                           rule != NULL ? rule_source_name(rule->when.source) : "invalid",
                           action != NULL ? rule_action_name(action->type) : "invalid",
                           rule != NULL ? comparator_name(rule->when.comparator) : "invalid",
                           threshold_kind,
                           threshold_bool ? "true" : "false",
                           (long)threshold_i32,
                           rule != NULL ? (unsigned long)rule->cooldown_ms : 0ul,
                           rule != NULL ? (unsigned long)rule->when.sustain_ms : 0ul,
                           gpio->pin,
                           gpio_profile_name(gpio->profile),
                           gpio->active_low ? "true" : "false",
                           (unsigned long)gpio->debounce_ms) &&
              json_append_string(&cursor, &remaining, action != NULL ? action->http_url : "") &&
              json_appendf(&cursor, &remaining, ",\"http_bearer_token\":\"%s\",\"rules\":[{\"id\":%lu,\"enabled\":%s,\"name\":",
                           auth_state,
                           rule != NULL ? (unsigned long)rule->id : 0ul,
                           (rule != NULL && rule->enabled) ? "true" : "false") &&
              json_append_string(&cursor, &remaining, rule != NULL ? rule->name : "") &&
              json_appendf(&cursor, &remaining, ",\"source\":\"%s\",\"action\":\"%s\",\"http_bearer_token\":\"%s\"}]}",
                           rule != NULL ? rule_source_name(rule->when.source) : "invalid",
                           action != NULL ? rule_action_name(action->type) : "invalid",
                           auth_state);
    if (!ok) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return false;
    }
    return true;
}

static bool json_get_string(const char *body, const char *key, char *out, size_t out_len)
{
    if (body == NULL || key == NULL || out == NULL || out_len == 0) {
        return false;
    }
    char pattern[40];
    int pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) {
        return false;
    }
    const char *pos = strstr(body, pattern);
    if (pos == NULL) {
        return false;
    }
    pos = strchr(pos + strlen(pattern), ':');
    if (pos == NULL) {
        return false;
    }
    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') {
        pos++;
    }
    if (*pos != '\"') {
        return false;
    }
    pos++;
    size_t used = 0;
    while (*pos != '\0' && *pos != '\"') {
        char decoded = *pos++;
        if (decoded == '\\') {
            switch (*pos) {
            case '\"':
            case '\\':
            case '/':
                decoded = *pos++;
                break;
            case 'b':
                decoded = '\b';
                pos++;
                break;
            case 'f':
                decoded = '\f';
                pos++;
                break;
            case 'n':
                decoded = '\n';
                pos++;
                break;
            case 'r':
                decoded = '\r';
                pos++;
                break;
            case 't':
                decoded = '\t';
                pos++;
                break;
            default:
                return false;
            }
        }
        if (used + 1u >= out_len) {
            out[0] = '\0';
            return false;
        }
        out[used++] = decoded;
    }
    out[used] = '\0';
    return *pos == '\"';
}

static bool json_get_bool(const char *body, const char *key, bool *out)
{
    if (body == NULL || key == NULL || out == NULL) {
        return false;
    }
    char pattern[40];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(body, pattern);
    if (pos == NULL) {
        return false;
    }
    pos = strchr(pos + strlen(pattern), ':');
    if (pos == NULL) {
        return false;
    }
    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') {
        pos++;
    }
    if (strncmp(pos, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(pos, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}


static bool json_get_i32(const char *body, const char *key, int32_t *out)
{
    if (body == NULL || key == NULL || out == NULL) {
        return false;
    }
    char pattern[40];
    int pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) {
        return false;
    }
    const char *pos = strstr(body, pattern);
    if (pos == NULL) {
        return false;
    }
    pos = strchr(pos + strlen(pattern), ':');
    if (pos == NULL) {
        return false;
    }
    pos++;
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') {
        pos++;
    }
    bool negative = false;
    if (*pos == '-') {
        negative = true;
        pos++;
    }
    if (*pos < '0' || *pos > '9') {
        return false;
    }
    int64_t value = 0;
    const int64_t limit = negative ? 2147483648LL : 2147483647LL;
    while (*pos >= '0' && *pos <= '9') {
        value = (value * 10) + (*pos - '0');
        if (value > limit) {
            return false;
        }
        pos++;
    }
    *out = negative ? (int32_t)-value : (int32_t)value;
    return true;
}

static bool gpio_profile_from_name(const char *name, rule_gpio_profile_t *profile)
{
    if (name == NULL || profile == NULL) {
        return false;
    }
    if (strcmp(name, "digital_high_low") == 0) {
        *profile = RULE_GPIO_PROFILE_DIGITAL_HIGH_LOW;
        return true;
    }
    if (strcmp(name, "debounced_contact") == 0) {
        *profile = RULE_GPIO_PROFILE_DEBOUNCED_CONTACT;
        return true;
    }
    if (strcmp(name, "rising_edge") == 0) {
        *profile = RULE_GPIO_PROFILE_RISING_EDGE;
        return true;
    }
    if (strcmp(name, "falling_edge") == 0) {
        *profile = RULE_GPIO_PROFILE_FALLING_EDGE;
        return true;
    }
    if (strcmp(name, "pulse_count") == 0) {
        *profile = RULE_GPIO_PROFILE_PULSE_COUNT;
        return true;
    }
    if (strcmp(name, "frequency") == 0) {
        *profile = RULE_GPIO_PROFILE_FREQUENCY;
        return true;
    }
    return false;
}

static bool json_get_gpio_config(const char *body, rule_gpio_config_t *gpio)
{
    if (body == NULL || gpio == NULL) {
        return false;
    }
    int32_t pin = 0;
    char profile_name[32];
    if (!json_get_i32(body, "gpio_pin", &pin) ||
        !json_get_string(body, "gpio_profile", profile_name, sizeof(profile_name)) ||
        !gpio_profile_from_name(profile_name, &gpio->profile)) {
        return false;
    }
    memset(gpio, 0, sizeof(*gpio));
    gpio->pin = (int)pin;
    (void)gpio_profile_from_name(profile_name, &gpio->profile);
    bool active_low = false;
    (void)json_get_bool(body, "gpio_active_low", &active_low);
    gpio->active_low = active_low;
    int32_t debounce_ms = 20;
    (void)json_get_i32(body, "gpio_debounce_ms", &debounce_ms);
    gpio->debounce_ms = debounce_ms > 0 ? (uint32_t)debounce_ms : 0u;
    return true;
}

static bool source_from_name(const char *name, rule_source_t *source)
{
    if (name == NULL || source == NULL) {
        return false;
    }
    for (rule_source_t candidate = RULE_SOURCE_SOUND_RMS_DBFS; candidate < RULE_SOURCE_COUNT; ++candidate) {
        if (strcmp(name, rule_source_name(candidate)) == 0) {
            *source = candidate;
            return true;
        }
    }
    return false;
}

static bool action_from_name(const char *name, rule_action_type_t *action)
{
    if (name == NULL || action == NULL) {
        return false;
    }
    for (rule_action_type_t candidate = RULE_ACTION_BLE_MESSAGE; candidate < RULE_ACTION_COUNT; ++candidate) {
        if (strcmp(name, rule_action_name(candidate)) == 0) {
            *action = candidate;
            return true;
        }
    }
    return false;
}


static bool comparator_from_name(const char *name, rule_comparator_t *comparator)
{
    if (name == NULL || comparator == NULL) {
        return false;
    }
    if (strcmp(name, "eq") == 0) {
        *comparator = RULE_COMPARATOR_EQ;
        return true;
    }
    if (strcmp(name, "ne") == 0) {
        *comparator = RULE_COMPARATOR_NE;
        return true;
    }
    if (strcmp(name, "gt") == 0) {
        *comparator = RULE_COMPARATOR_GT;
        return true;
    }
    if (strcmp(name, "gte") == 0) {
        *comparator = RULE_COMPARATOR_GTE;
        return true;
    }
    if (strcmp(name, "lt") == 0) {
        *comparator = RULE_COMPARATOR_LT;
        return true;
    }
    if (strcmp(name, "lte") == 0) {
        *comparator = RULE_COMPARATOR_LTE;
        return true;
    }
    return false;
}

static bool make_json_config(automation_config_t *config, const char *body)
{
    if (config == NULL || body == NULL) {
        return false;
    }
    char source_name[48];
    char action_name[32];
    rule_source_t source;
    rule_action_type_t action;
    if (!json_get_string(body, "source", source_name, sizeof(source_name)) || !source_from_name(source_name, &source) ||
        !json_get_string(body, "action", action_name, sizeof(action_name)) || !action_from_name(action_name, &action)) {
        return false;
    }

    automation_config_set_defaults(config);
    automation_rule_t *rule = &config->rules[0];
    bool enabled = true;
    (void)json_get_bool(body, "enabled", &enabled);
    rule->enabled = enabled;
    rule->when.source = source;
    rule->action_count = 1;
    rule->actions[0].type = action;
    rule->actions[0].timeout_ms = 1000;
    rule->cooldown_ms = 1000;
    if (source == RULE_SOURCE_SOUND_RMS_DBFS || source == RULE_SOURCE_SOUND_PEAK_DBFS) {
        rule->when.comparator = RULE_COMPARATOR_GT;
        rule->when.threshold = rule_value_i32(-20 * 256);
    } else {
        rule->when.comparator = RULE_COMPARATOR_EQ;
        rule->when.threshold = rule_value_bool(true);
    }
    char comparator_name_buf[16];
    rule_comparator_t parsed_comparator;
    if (json_get_string(body, "comparator", comparator_name_buf, sizeof(comparator_name_buf)) &&
        comparator_from_name(comparator_name_buf, &parsed_comparator)) {
        rule->when.comparator = parsed_comparator;
    }
    char threshold_kind[8];
    int32_t threshold_i32 = 0;
    bool threshold_bool = false;
    if (json_get_string(body, "threshold_kind", threshold_kind, sizeof(threshold_kind)) &&
        strcmp(threshold_kind, "i32") == 0 && json_get_i32(body, "threshold_i32", &threshold_i32)) {
        rule->when.threshold = rule_value_i32(threshold_i32);
    } else if (json_get_bool(body, "threshold_bool", &threshold_bool)) {
        rule->when.threshold = rule_value_bool(threshold_bool);
    }
    int32_t cooldown_ms = 0;
    if (json_get_i32(body, "cooldown_ms", &cooldown_ms) && cooldown_ms > 0) {
        rule->cooldown_ms = (uint32_t)cooldown_ms;
    }
    int32_t sustain_ms = 0;
    if (json_get_i32(body, "sustain_ms", &sustain_ms) && sustain_ms >= 0) {
        rule->when.sustain_ms = (uint32_t)sustain_ms;
    }
    (void)json_get_string(body, "name", rule->name, sizeof(rule->name));
    if (rule->name[0] == '\0') {
        (void)snprintf(rule->name, sizeof(rule->name), "Imported rule");
    }
    if (rule_source_is_gpio(source)) {
        if (!json_get_gpio_config(body, &rule->when.gpio)) {
            return false;
        }
        (void)snprintf(rule->when.source_key, sizeof(rule->when.source_key), "%s.%d",
                       rule_source_name(source), rule->when.gpio.pin);
    }
    if (action == RULE_ACTION_HTTP_POST) {
        if (!json_get_string(body, "http_url", rule->actions[0].http_url, sizeof(rule->actions[0].http_url))) {
            return false;
        }
        char token[RULE_HTTP_AUTH_MAX];
        if (json_get_string(body, "http_bearer_token", token, sizeof(token)) && strcmp(token, "masked") != 0) {
            (void)snprintf(rule->actions[0].http_bearer_token, sizeof(rule->actions[0].http_bearer_token), "%s", token);
        }
    }
    return true;
}

static void make_preset_config(automation_config_t *config, const char *preset)
{
    automation_config_set_defaults(config);
    if (preset == NULL || strcmp(preset, "defaults") == 0) {
        return;
    }
    automation_rule_t *rule = &config->rules[0];
    rule->enabled = true;
    rule->action_count = 1;
    rule->actions[0].type = RULE_ACTION_LOCAL_UI;
    rule->actions[0].timeout_ms = 100;
    rule->cooldown_ms = 1000;
    rule->when.comparator = RULE_COMPARATOR_EQ;
    rule->when.threshold = rule_value_bool(true);
    if (strcmp(preset, "sound_local_ui") == 0) {
        (void)snprintf(rule->name, sizeof(rule->name), "Sound clipped alert");
        rule->when.source = RULE_SOURCE_SOUND_CLIPPED;
    } else if (strcmp(preset, "button_local_ui") == 0) {
        (void)snprintf(rule->name, sizeof(rule->name), "KEY1 local alert");
        rule->when.source = RULE_SOURCE_KEY1_SHORT;
    }
}

static const char *config_preset_from_body(const char *body)
{
    if (body == NULL || body[0] == '\0' || strcmp(body, "defaults") == 0) {
        return "defaults";
    }
    if (strcmp(body, "sound_local_ui") == 0 || strstr(body, "sound_local_ui") != NULL) {
        return "sound_local_ui";
    }
    if (strcmp(body, "button_local_ui") == 0 || strstr(body, "button_local_ui") != NULL) {
        return "button_local_ui";
    }
    return NULL;
}

bool rule_web_handle_request(rule_web_t *web, rule_web_method_t method, const char *path, const char *body, char *out, size_t out_len)
{
    if (web == NULL || path == NULL || out == NULL || out_len == 0 || !web->started) {
        return false;
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/") == 0) {
        const int written = snprintf(out, out_len, "%s", s_rule_setup_page);
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/favicon.ico") == 0) {
        out[0] = '\0';
        return true;
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/api/capabilities") == 0) {
        return capability_build_json(out, out_len) > 0;
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/api/status") == 0) {
        return rule_web_get_status_json(web, out, out_len);
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/api/time") == 0) {
        return app_time_config_json(out, out_len);
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/time") == 0) {
        char timezone[APP_TIME_TIMEZONE_MAX_LEN + 1u];
        if (!json_get_string(body, "timezone", timezone, sizeof(timezone))) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"missing_timezone\"}");
            return written > 0 && (size_t)written < out_len;
        }
        if (!app_time_set_timezone(timezone, true)) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"invalid_timezone\"}");
            return written > 0 && (size_t)written < out_len;
        }
        return app_time_config_json(out, out_len);
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/api/wifi/status") == 0) {
        return app_wifi_status_json(out, out_len);
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/wifi/scan") == 0) {
        return app_wifi_scan_json(out, out_len);
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/wifi/connect") == 0) {
        char ssid[33];
        char password[65];
        if (!json_get_string(body, "ssid", ssid, sizeof(ssid))) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"missing_ssid\"}");
            return written > 0 && (size_t)written < out_len;
        }
        if (!json_get_string(body, "password", password, sizeof(password))) {
            password[0] = '\0';
        }
        const bool ok = app_wifi_connect(ssid, password, true);
        char status[512];
        if (!app_wifi_status_json(status, sizeof(status))) {
            (void)snprintf(status, sizeof(status), "{\"enabled\":false}");
        }
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"status\":%s}", ok ? "true" : "false", status);
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/wifi/forget") == 0) {
        const bool ok = app_wifi_forget_sta_credentials();
        char status[512];
        if (!app_wifi_status_json(status, sizeof(status))) {
            (void)snprintf(status, sizeof(status), "{\"enabled\":false}");
        }
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"status\":%s}", ok ? "true" : "false", status);
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/wifi/ap") == 0) {
        app_wifi_config_t config;
        char ap_ssid[33];
        char ap_password[65];
        int32_t ap_channel = 0;
        if (!app_wifi_get_config(&config)) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"config_unavailable\"}");
            return written > 0 && (size_t)written < out_len;
        }
        (void)snprintf(ap_ssid, sizeof(ap_ssid), "%s", config.ap_ssid);
        (void)snprintf(ap_password, sizeof(ap_password), "%s", config.ap_password);
        ap_channel = config.ap_channel;
        if (body != NULL && body[0] != '\0') {
            const bool body_has_ssid = json_get_string(body, "ssid", ap_ssid, sizeof(ap_ssid));
            if (body_has_ssid && !json_get_string(body, "password", ap_password, sizeof(ap_password))) {
                ap_password[0] = '\0';
            } else if (!body_has_ssid) {
                (void)json_get_string(body, "password", ap_password, sizeof(ap_password));
            }
            (void)json_get_i32(body, "channel", &ap_channel);
        }
        const size_t ap_ssid_len = strlen(ap_ssid);
        const size_t ap_password_len = strlen(ap_password);
        if (ap_ssid_len == 0u || ap_ssid_len > 32u) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"invalid_ap_ssid\"}");
            return written > 0 && (size_t)written < out_len;
        }
        if (ap_password_len > 0u && (ap_password_len < 8u || ap_password_len > 63u)) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"invalid_ap_password\"}");
            return written > 0 && (size_t)written < out_len;
        }
        if (ap_channel < 1 || ap_channel > 13) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"invalid_ap_channel\"}");
            return written > 0 && (size_t)written < out_len;
        }
        const bool ok = app_wifi_start_ap_configured(ap_ssid, ap_password, (uint8_t)ap_channel, true);
        char status[512];
        if (!app_wifi_status_json(status, sizeof(status))) {
            (void)snprintf(status, sizeof(status), "{\"enabled\":false}");
        }
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"status\":%s}", ok ? "true" : "false", status);
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/wifi/mode") == 0) {
        char mode_name[16];
        app_wifi_mode_t mode = APP_WIFI_MODE_OFF;
        if (!json_get_string(body, "mode", mode_name, sizeof(mode_name))) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"missing_mode\"}");
            return written > 0 && (size_t)written < out_len;
        }
        if (strcmp(mode_name, "wifi") == 0 || strcmp(mode_name, "sta") == 0) {
            mode = APP_WIFI_MODE_STA;
        } else if (strcmp(mode_name, "ap") == 0) {
            mode = APP_WIFI_MODE_AP;
        } else if (strcmp(mode_name, "apsta") == 0) {
            mode = APP_WIFI_MODE_APSTA;
        } else if (strcmp(mode_name, "off") == 0) {
            mode = APP_WIFI_MODE_OFF;
        } else {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"error\":\"invalid_mode\"}");
            return written > 0 && (size_t)written < out_len;
        }
        const bool ok = app_wifi_set_mode(mode);
        char status[512];
        if (!app_wifi_status_json(status, sizeof(status))) {
            (void)snprintf(status, sizeof(status), "{\"enabled\":false}");
        }
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"status\":%s}", ok ? "true" : "false", status);
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_GET && strcmp(path, "/api/config") == 0) {
        return write_config_json(&web->runtime->engine.config, out, out_len);
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/config") == 0) {
        automation_config_t *config = calloc(1, sizeof(*config));
        if (config == NULL) {
            const int written = snprintf(out, out_len, "{\"error\":\"config allocation failed\"}");
            return written > 0 && (size_t)written < out_len;
        }
        const char *preset = config_preset_from_body(body);
        bool accepted = true;
        if (preset != NULL) {
            make_preset_config(config, preset);
        } else if (!make_json_config(config, body)) {
            accepted = false;
            const int written = snprintf(out, out_len, "{\"error\":\"unsupported config body\"}");
            free(config);
            return written > 0 && (size_t)written < out_len;
        }
        if (accepted && (!rule_config_store_save(web->store, config) || !rule_runtime_replace_config(web->runtime, config))) {
            const int written = snprintf(out, out_len, "{\"error\":\"config rejected\"}");
            free(config);
            return written > 0 && (size_t)written < out_len;
        }
        const bool written = write_config_json(config, out, out_len);
        free(config);
        return written;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/rules/test") == 0) {
        const automation_rule_t *rule = web->runtime->engine.config.rule_count > 0 ? &web->runtime->engine.config.rules[0] : NULL;
        if (rule == NULL || !rule->enabled || rule->action_count == 0) {
            const int written = snprintf(out, out_len, "{\"ok\":false,\"reason\":\"no_enabled_rule\"}");
            return written > 0 && (size_t)written < out_len;
        }
        rule_event_t event;
        memset(&event, 0, sizeof(event));
        event.sequence = web->runtime->engine.next_event_sequence++;
        event.rule_id = rule->id;
        event.source = rule->when.source;
        event.action = rule->actions[0].type;
        event.action_config = rule->actions[0];
        event.measured_value = rule->when.threshold;
        (void)snprintf(event.rule_name, sizeof(event.rule_name), "%s", rule->name);
        bool queued = action_enqueue(&web->runtime->dispatcher, &event);
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"queued\":%s}", queued ? "true" : "false", queued ? "true" : "false");
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/gpio/test") == 0) {
        rule_gpio_config_t gpio;
        char error[RULE_ERROR_MAX];
        char source_name[48];
        rule_source_t source = RULE_SOURCE_GPIO_DIGITAL;
        memset(error, 0, sizeof(error));
        if (json_get_string(body, "source", source_name, sizeof(source_name)) && !source_from_name(source_name, &source)) {
            source = RULE_SOURCE_INVALID;
        }
        const bool supported = source != RULE_SOURCE_INVALID && capability_source_supported(source);
        bool valid = supported && json_get_gpio_config(body, &gpio) &&
                     capability_gpio_source_profile_validate(source, &gpio, error, sizeof(error));
        const int written = snprintf(out, out_len, "{\"ok\":%s,\"supported\":%s,\"source\":\"%s\",\"reason\":\"%s\"}",
                                     valid ? "true" : "false",
                                     supported ? "true" : "false",
                                     source != RULE_SOURCE_INVALID ? rule_source_name(source) : "invalid",
                                     valid ? "valid" : (error[0] != '\0' ? error : capability_source_reason(source)));
        return written > 0 && (size_t)written < out_len;
    }
    if (method == RULE_WEB_METHOD_POST && strcmp(path, "/api/hat/probe") == 0) {
        char source_name[48];
        rule_source_t source = RULE_SOURCE_HAT_PIR_MOTION;
        if (json_get_string(body, "source", source_name, sizeof(source_name)) && !source_from_name(source_name, &source)) {
            source = RULE_SOURCE_INVALID;
        }
        const bool supported = source != RULE_SOURCE_INVALID && capability_hat_supported(source);
        const int written = snprintf(out, out_len,
                                     "{\"ok\":%s,\"present\":%s,\"supported\":%s,\"source\":\"%s\",\"reason\":\"%s\"}",
                                     supported ? "true" : "false",
                                     supported ? "true" : "false",
                                     supported ? "true" : "false",
                                     source != RULE_SOURCE_INVALID ? rule_source_name(source) : "invalid",
                                     supported ? "present" : capability_source_reason(source));
        return written > 0 && (size_t)written < out_len;
    }
    const int written = snprintf(out, out_len, "{\"error\":\"not found\"}");
    return written > 0 && (size_t)written < out_len;
}
