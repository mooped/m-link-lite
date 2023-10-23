#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "cJSON.h"

#include "event.h"
#include "hostname.h"
#include "mount.h"
#include "settings.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

#define SCRATCH_BUFSIZE 8192

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static const char* TAG = "m-link-http-server";

/*
 * M-Link WebSocket handler
 */
static void process_ws_payload(cJSON* root, cJSON* response)
{
  // Extract servo data
  cJSON* servos = cJSON_GetObjectItem(root, "servos");
  cJSON* servo = NULL;
  if (cJSON_IsArray(servos))
  {
    // Process servo data
    int index = 0;
    cJSON_ArrayForEach(servo, servos)
    {
      if (cJSON_IsNumber(servo))
      {
        process_servo_event(index++, servo->valueint);
      }
    }

    // Early out to avoid chewing up cycles - only if no other elements
    if (servos->next == NULL && servos->prev == NULL)
    {
      return;
    }
  }

  // Extract failsafe data
  cJSON* failsafes = cJSON_GetObjectItem(root, "failsafes");
  cJSON* failsafe = NULL;
  if (cJSON_IsArray(failsafes))
  {
    // Process failsafe data
    int index = 0;
    cJSON_ArrayForEach(failsafe, failsafes)
    {
      if (cJSON_IsNumber(failsafe))
      {
        process_failsafe_event(index, failsafe->valueint);
      }
    }
  }

  // Apply settings?
  cJSON* settings = cJSON_GetObjectItem(root, "settings");
  if (settings)
  {
    bool any_updates = false;
    cJSON* name = cJSON_GetObjectItem(settings, "name");
    if (cJSON_IsString(name))
    {
      ESP_LOGI(TAG, "Set name to %s", name->valuestring);
      settings_set_name(name->valuestring);
      any_updates = true;
    }
    cJSON* ap_ssid = cJSON_GetObjectItem(settings, "ap_ssid");
    if (cJSON_IsString(ap_ssid))
    {
      ESP_LOGI(TAG, "Set AP SSID to %s", ap_ssid->valuestring);
      settings_set_ap_ssid(ap_ssid->valuestring);
      any_updates = true;
    }
    cJSON* ap_password = cJSON_GetObjectItem(settings, "ap_password");
    if (cJSON_IsString(ap_password))
    {
      ESP_LOGI(TAG, "Set AP password to %s", ap_password->valuestring);
      settings_set_ap_password(ap_password->valuestring);
      any_updates = true;
    }
    cJSON* ssid = cJSON_GetObjectItem(settings, "ssid");
    if (cJSON_IsString(ssid))
    {
      ESP_LOGI(TAG, "Set SSID to %s", ssid->valuestring);
      settings_set_ssid(ssid->valuestring);
      any_updates = true;
    }
    cJSON* password = cJSON_GetObjectItem(settings, "password");
    if (cJSON_IsString(password))
    {
      ESP_LOGI(TAG, "Set password to %s", password->valuestring);
      settings_set_password(password->valuestring);
      any_updates = true;
    }
    if (any_updates)
    {
      ESP_LOGI(TAG, "Writing settings");
      settings_write();
    }
  }

  // Reset settings
  cJSON* reset_settings = cJSON_GetObjectItem(root, "reset_settings");
  if (reset_settings)
  {
    if (cJSON_IsString(reset_settings) && strcmp(reset_settings->valuestring,  "sgnittes_teser") == 0)
    {
      ESP_LOGI(TAG, "Restoring default settings");
      settings_reset_defaults();
    }
  }

  // Reboot
  cJSON* reboot = cJSON_GetObjectItem(root, "reboot");
  if (reboot)
  {
    if (cJSON_IsString(reboot) && strcmp(reboot->valuestring,  "toober") == 0)
    {
      ESP_LOGI(TAG, "Rebooting");
      esp_restart();
    }
  }

  // Handle queries
  cJSON* query= cJSON_GetObjectItem(root, "query");
  if (cJSON_IsString(query))
  {
    // Querying battery level?
    if (strcmp(query->valuestring, "battery") == 0)
    {
      // TODO: Figure out why cJSON_Print crashes on numbers!
      char voltage_buffer[16];
      snprintf(voltage_buffer, 15, "%d", query_battery_voltage());
      voltage_buffer[15] = '\0';
      cJSON* voltage = cJSON_CreateString(voltage_buffer);
      cJSON_AddItemToObject(response, "battery", voltage);
    }

    // Querying failsafe?
    if (strcmp(query->valuestring, "failsafes") == 0)
    {
      // TODO: Figure out why cJSON_Print crashes on numbers!
      char failsafe_buffer[16];
      cJSON* failsafes = cJSON_CreateArray();
      const int num_channels = query_supported_channels();
      for (int i = 0; i < num_channels; ++i)
      {
        snprintf(failsafe_buffer, 15, "%d", query_failsafe(i));
        failsafe_buffer[15] = '\0';
        cJSON* failsafe = cJSON_CreateString(failsafe_buffer);
        cJSON_AddItemToArray(failsafes, failsafe);
      }
      cJSON_AddItemToObject(response, "failsafes", failsafes);
    }

    // Querying settings?
    if (strcmp(query->valuestring, "settings") == 0)
    {
      cJSON* settings = cJSON_CreateObject();
      {
        cJSON* name = cJSON_CreateString(settings_get_name());
        if (name)
        {
          cJSON_AddItemToObject(settings, "name", name);
        }
      }
      {
        cJSON* ap_ssid = cJSON_CreateString(settings_get_ap_ssid());
        if (ap_ssid)
        {
          cJSON_AddItemToObject(settings, "ap_ssid", ap_ssid);
        }
      }
      {
        cJSON* ap_password = cJSON_CreateString(settings_get_ap_password());
        if (ap_password)
        {
          cJSON_AddItemToObject(settings, "ap_password", ap_password);
        }
      }
      {
        cJSON* ssid = cJSON_CreateString(settings_get_ssid());
        if (ssid)
        {
          cJSON_AddItemToObject(settings, "ssid", ssid);
        }
      }
      {
        cJSON* password = cJSON_CreateString(settings_get_password());
        if (password)
        {
          cJSON_AddItemToObject(settings, "password", password);
        }
      }
      cJSON_AddItemToObject(response, "settings", settings);
    }
  }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
  //static int packet_count = 0;
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  //ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
  if (ws_pkt.len) {
    /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      ESP_LOGE(TAG, "Failed to calloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    //ESP_LOGI(TAG, "Packet %d Message: %s", ++packet_count, ws_pkt.payload);
  }

  // Build response
  cJSON* response = cJSON_CreateObject();

  // Process packet
  cJSON* root = cJSON_Parse((char*)ws_pkt.payload);
  if (root)
  {
    process_ws_payload(root, response);
    cJSON_Delete(root);
  }

  // Failsafe status
  cJSON* status = cJSON_CreateString(query_failsafe_engaged() ? "failsafe" : "ok");
  cJSON_AddItemToObject(response, "status", status);

  char response_buffer[256];
  cJSON_PrintPreallocated(response, response_buffer, sizeof(response_buffer), false);
  //ESP_LOGI(TAG, "WS Response: %s", response_buffer);
  cJSON_Delete(response);
  httpd_ws_frame_t response_pkt = {
    .final = false,
    .fragmented = false,
    .type = HTTPD_WS_TYPE_TEXT,
    .payload = (unsigned char*)response_buffer,
    .len = strlen((char*)response_buffer)
  };

  ret = httpd_ws_send_frame(req, &response_pkt);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }
  free(buf);
  return ret;
}

static const httpd_uri_t ws = {
    .uri    = "/ws",
    .method   = HTTP_GET,
    .handler  = ws_handler,
    .user_ctx   = NULL,
    .is_websocket = true,
};

typedef struct
{
  /* Base path of file storage */
  char base_path[ESP_VFS_PATH_MAX + 1];

  /* Scratch buffer for temporary storage during file transfer */
  char scratch[SCRATCH_BUFSIZE];
} file_server_data;

file_server_data server_data;

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with 204 No Content */
static esp_err_t code_204_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Macro to declare a handler for a specific file embedded in Flash */
#define DEFINE_EMBEDDED_FILE_HANDLER(filename, mime_type) \
static esp_err_t filename ## _get_handler(httpd_req_t *req) \
{ \
    extern const unsigned char filename ## _start[] asm("_binary_" #filename "_start"); \
    extern const unsigned char filename ## _end[]   asm("_binary_" #filename "_end"); \
    const size_t file_size = (filename ## _end - filename ## _start); \
    httpd_resp_set_type(req, mime_type); \
    httpd_resp_send(req, (const char *)filename ## _start, file_size); \
    return ESP_OK; \
}

DEFINE_EMBEDDED_FILE_HANDLER(menu_html, "text/html");
DEFINE_EMBEDDED_FILE_HANDLER(info_html, "text/html");
DEFINE_EMBEDDED_FILE_HANDLER(settings_html, "text/html");
DEFINE_EMBEDDED_FILE_HANDLER(joystick_html, "text/html");
DEFINE_EMBEDDED_FILE_HANDLER(m_link_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(nipplejs_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(grid_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(hexapod_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(vector_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(jquery_min_js, "text/javascript");
DEFINE_EMBEDDED_FILE_HANDLER(favicon_ico, "image/x-icon");
DEFINE_EMBEDDED_FILE_HANDLER(hamburger_svg, "image/svg+xml");
DEFINE_EMBEDDED_FILE_HANDLER(style_css, "text/css");

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest)
  {
    pathlen = MIN(pathlen, quest - uri);
  }
  const char *hash = strchr(uri, '#');
  if (hash)
  {
    pathlen = MIN(pathlen, hash - uri);
  }

  if (base_pathlen + pathlen + 1 > destsize)
  {
    /* Full path string won't fit into destination buffer */
    return NULL;
  }

  /* Construct full path (base + path) */
  strcpy(dest, base_path);
  strlcpy(dest + base_pathlen, uri, pathlen + 1);

  /* Return pointer to path, skipping the base */
  return dest + base_pathlen;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
  char entrypath[FILE_PATH_MAX];
  char entrysize[16];
  const char *entrytype;

  struct dirent *entry;
  struct stat entry_stat;

  DIR *dir = opendir(dirpath);
  const size_t dirpath_len = strlen(dirpath);

  /* Retrieve the base path of file storage to construct the full path */
  strlcpy(entrypath, dirpath, sizeof(entrypath));

  ESP_LOGI(TAG, "Listing directory %s", dirpath);
  if (!dir)
  {
    ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
    return ESP_FAIL;
  }

  /* Send HTML file header */
  httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><title>M-Link File Manager</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body>");

  /* Get handle to embedded file upload script */
  extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
  extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
  const size_t upload_script_size = (upload_script_end - upload_script_start);

  /* Add file upload form and script which on execution sends a POST request to /upload */
  httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

  /* Send file-list table definition and column labels */
  httpd_resp_sendstr_chunk(req,
    "<table class=\"filemanager\" border=\"1\">"
    "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
    "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
    "<tbody>");

  /* Iterate over all files / folders and fetch their names and sizes */
  while ((entry = readdir(dir)) != NULL)
  {
    entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

    strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
    if (stat(entrypath, &entry_stat) == -1)
    {
      ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
      continue;
    }
    sprintf(entrysize, "%ld", entry_stat.st_size);
    ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

    /* Send chunk of HTML file containing table entries with file name and size */
    httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, "../");
    httpd_resp_sendstr_chunk(req, entry->d_name);
    if (entry->d_type == DT_DIR)
    {
      httpd_resp_sendstr_chunk(req, "/");
    }
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "</a></td><td>");
    httpd_resp_sendstr_chunk(req, entrytype);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, entrysize);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");
  }
  closedir(dir);

  /* Finish the file list table */
  httpd_resp_sendstr_chunk(req, "</tbody></table>");

  /* Link back to main page */
  httpd_resp_sendstr_chunk(req, "<br /><br /><a href=\"/\">Back</a>");

  /* Send remaining chunk of HTML file to complete it */
  httpd_resp_sendstr_chunk(req, "</body></html>");

  /* Send empty chunk to signal HTTP response completion */
  httpd_resp_sendstr_chunk(req, NULL);
  return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "text/javascript");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".webp")) {
        return httpd_resp_set_type(req, "image/webp");
    } else if (IS_FILE_EXT(filename, ".svg")) {
        return httpd_resp_set_type(req, "image/svg+xml");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/*
 * This handler serves files from SPIFFS
 * (or other mounted storage)
 */
static esp_err_t file_get_handler(httpd_req_t *req)
{
  char filepath[FILE_PATH_MAX];
  FILE *fd = NULL;
  struct stat file_stat;

  const char *filename = get_path_from_uri(filepath, ((file_server_data*)req->user_ctx)->base_path,
                                           req->uri, sizeof(filepath));
  if (!filename)
  {
    ESP_LOGE(TAG, "Filename is too long");
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  /* If file name is '/' respond with the main application page */
  if (strcmp(filename, "/") == 0)
  {
    return menu_html_get_handler(req);
  }
  if (strcmp(filename, "/drive") == 0)
  {
    return joystick_html_get_handler(req);
  }
  /* If file name is 'generate_204' or 'gen_204' respond with 204 No Content */
  /* to avoid triggering 'No Internet' detection on Android */
  if (strcmp(filename, "/generate_204") == 0 || strcmp(filename, "/gen_204") == 0)
  {
    return code_204_handler(req);
  }
  /* If file name is '/info' respond with the info page */
  if (strcmp(filename, "/info") == 0)
  {
    return info_html_get_handler(req);
  }
  /* If file name is '/settings' respond with the settings page */
  if (strcmp(filename, "/settings") == 0)
  {
    return settings_html_get_handler(req);
  }

  /* If name has trailing '/', respond with directory contents */
  if (filename[strlen(filename) - 1] == '/')
  {
    return http_resp_dir_html(req, "/data/");
  }

  if (stat(filepath, &file_stat) == -1)
  {
    /* If file not present on SPIFFS check if URI
     * corresponds to one of the hardcoded paths */
    if (strcmp(filename, "/index.html") == 0)
    {
      return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/jquery.min.js") == 0)
    {
      return jquery_min_js_get_handler(req);
    }
    else if (strcmp(filename, "/m-link.js") == 0)
    {
      return m_link_js_get_handler(req);
    }
    else if (strcmp(filename, "/nipplejs.js") == 0)
    {
      return nipplejs_js_get_handler(req);
    }
    else if (strcmp(filename, "/grid.js") == 0)
    {
      return grid_js_get_handler(req);
    }
    else if (strcmp(filename, "/hexapod.js") == 0)
    {
      return hexapod_js_get_handler(req);
    }
    else if (strcmp(filename, "/vector.js") == 0)
    {
      return vector_js_get_handler(req);
    }
    else if (strcmp(filename, "/favicon.ico") == 0)
    {
      return favicon_ico_get_handler(req);
    }
    else if (strcmp(filename, "/hamburger.svg") == 0)
    {
      return hamburger_svg_get_handler(req);
    }
    else if (strcmp(filename, "/style.css") == 0)
    {
      return style_css_get_handler(req);
    }
    ESP_LOGE(TAG, "Failed to stat file : %s", filepath);

    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");

    return ESP_FAIL;
  }

  fd = fopen(filepath, "r");
  if (!fd)
  {
      ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
      return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
  set_content_type_from_file(req, filename);

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char *chunk = ((file_server_data*)req->user_ctx)->scratch;
  size_t chunksize;
  do
  {
    /* Read file in chunks into the scratch buffer */
    chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

    ESP_LOGI(TAG, "Send chunk of %d", chunksize);
    if (chunksize > 0)
    {
      /* Send the buffer contents as HTTP response chunk */
      if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
      {
        fclose(fd);
        ESP_LOGE(TAG, "File sending failed!");
        /* Abort sending file */
        httpd_resp_sendstr_chunk(req, NULL);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
      }
    }

    /* Keep looping till the whole file is sent */
  }
  while (chunksize != 0);

  /* Close file after sending complete */
  fclose(fd);
  ESP_LOGI(TAG, "File sending complete");

  /* Respond with an empty chunk to signal HTTP response completion */
#if 1
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((file_server_data*)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((file_server_data*)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/files/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((file_server_data*)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete/files") - 1, sizeof(filepath));

    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/files/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

static const httpd_uri_t file_download = {
    .uri       = "/*",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = file_get_handler,
    .user_ctx  = &server_data,
};

static const httpd_uri_t file_upload = {
    .uri = "/upload/*", // Match URIs of type /upload/path/to/file
    .method    = HTTP_POST,
    .handler   = upload_post_handler,
    .user_ctx  = &server_data,
};

static const httpd_uri_t file_delete = {
   .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
   .method    = HTTP_POST,
   .handler   = delete_post_handler,
   .user_ctx  = &server_data,
};

typedef struct
{
  const unsigned char* data;
  unsigned int size;
} static_resource_t;

/* URI handler to GET a static resource */
esp_err_t get_handler_static(httpd_req_t *req)
{
  static_resource_t* resource = (static_resource_t*)(req->user_ctx);
  httpd_resp_send(req, (const char*)resource->data, resource->size);
  return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

 /* Use the URI wildcard matching function in order to
  * allow the same handler to respond to multiple different
  * target URIs which match the wildcard scheme */
 config.uri_match_fn = httpd_uri_match_wildcard;

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Registering the ws handler
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &file_upload);
    httpd_register_uri_handler(server, &file_delete);
    httpd_register_uri_handler(server, &file_download);
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
  // Stop the httpd server
  return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                 int32_t event_id, void* event_data)
{
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server) {
    ESP_LOGI(TAG, "Stopping webserver");
    if (stop_webserver(*server) == ESP_OK) {
      *server = NULL;
    } else {
      ESP_LOGE(TAG, "Failed to stop http server");
    }
  }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
              int32_t event_id, void* event_data)
{
  httpd_handle_t* server = (httpd_handle_t*) arg;
  if (*server == NULL) {
    ESP_LOGI(TAG, "Starting webserver");
    *server = start_webserver();
  }
}

void server_init(void)
{
  static httpd_handle_t server = NULL;

  const char* const base_path = "/data";
  ESP_ERROR_CHECK(mount_storage(base_path));
  strncpy(server_data.base_path, base_path, ESP_VFS_PATH_MAX + 1);

  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

  /* Start the server for the first time */
  server = start_webserver();
}

