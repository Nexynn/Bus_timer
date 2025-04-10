#include "request.h"



extern const uint8_t ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t ca_cert_pem_end[] asm("_binary_ca_cert_pem_end");

// Macros pour les paramètres
#define MONITORING_REF "STIF:StopArea:SP:24341:"
#define LINE_REF "STIF:Line::C01287:"
#define API_KEY "uDqBgtAO4xwCBMkSthcU1fjtRQVwi3im"  //change this

// URL de l'API
#define URL "https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring"
#define MAX_HTTP_OUTPUT_BUFFER 1024 * 100 // Taille du tampon pour la réponse HTTP

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("WIFI_EVENT_HANDLER", "Reconnection...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("WIFI_EVENT_HANDLER", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}



// Variables globales pour le tampon
static char response_buffer[MAX_HTTP_OUTPUT_BUFFER];
static int buffer_offset = 0;
static const char *TAG = "HTTP_JSON";

int compare_dates(const char *date1, const char *date2) {
    for (int i = 0; i < 24; i++) {
        if (date1[i] < date2[i]) {
            return -1;
        } else if (date1[i] > date2[i]) {
            return 1;
        }
    }
    return 0;
}

void extract_values_and_compare(const char *json_data) {
    char response_timestamp[30];
    char expected_arrival_time[30];
    int index = 1; // Pour suivre quel ExpectedArrivalTime nous traitons

    // Trouver la position de "ResponseTimestamp" dans la chaîne JSON
    char *response_timestamp_start = strstr(json_data, "\"ResponseTimestamp\":");
    if (response_timestamp_start) {
        // Extraire la valeur de ResponseTimestamp
        sscanf(response_timestamp_start, "\"ResponseTimestamp\": \"%[^\"]\"", response_timestamp);
        // printf("ResponseTimestamp: %s\n", response_timestamp);
    }
    strcpy(now, response_timestamp);

    // Trouver la première occurrence de "ExpectedDepartureTime"
    char *expected_arrival_time_start = strstr(json_data, "\"ExpectedDepartureTime\":");
    while (expected_arrival_time_start && index <= 2) {
        // Extraire la valeur d'ExpectedDepartureTime
        sscanf(expected_arrival_time_start, "\"ExpectedDepartureTime\": \"%[^\"]\"", expected_arrival_time);

        // Comparer ExpectedDepartureTime avec ResponseTimestamp
        int cmp_result = compare_dates(response_timestamp, expected_arrival_time);
        if (cmp_result < 0) {
            if (index == 1)
                strcpy(time1, expected_arrival_time);
            else
                strcpy(time2, expected_arrival_time);
            
            // printf("ExpectedDepartureTime %d (before ResponseTimestamp): %s\n", index, expected_arrival_time);
            index++;
        }

        // Chercher la prochaine occurrence de "ExpectedDepartureTime"
        expected_arrival_time_start = strstr(expected_arrival_time_start + 1, "\"ExpectedDepartureTime\":");
    }
    now[19] = '\0';
    time1[19] = '\0';
    time2[19] = '\0';
}

// Callback pour la gestion de la réponse HTTP
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!evt->user_data) {
                ESP_LOGE(TAG, "Pas de tampon alloué pour la réponse.");
                return ESP_FAIL;
            }
            if ((buffer_offset + evt->data_len) < MAX_HTTP_OUTPUT_BUFFER) {
                memcpy(response_buffer + buffer_offset, evt->data, evt->data_len);
                buffer_offset += evt->data_len;
            } else {
                ESP_LOGE(TAG, "Tampon plein, données tronquées.");
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            // response_buffer[buffer_offset] = '\0'; // Null-terminate le tampon
            // printf("Taille réponse reçue: %d\n", strlen(response_buffer));
            extract_values_and_compare(response_buffer);
            buffer_offset = 0; // Réinitialiser le tampon
            break;

        default:
            break;
    }
    return ESP_OK;
}

long calculate_time_difference(const char *time1, const char *time2) {
    struct tm tm1 = {0}, tm2 = {0};
    
    // Convert the time strings to struct tm
    if (strptime(time1, "%Y-%m-%dT%H:%M:%S", &tm1) == NULL) {
        printf("Error parsing time1.\n");
        return -1;
    }
    if (strptime(time2, "%Y-%m-%dT%H:%M:%S", &tm2) == NULL) {
        printf("Error parsing time2.\n");
        return -1;
    }

    // Convert struct tm to time_t (seconds since epoch)
    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    // Calculer la différence en secondes
    long diff_in_seconds = difftime(t2, t1);

    // Return the difference in seconds
    return (long)floor(diff_in_seconds / 60.0);
}

int get_departure() {
    char url[512];
    snprintf(url, sizeof(url), "%s?MonitoringRef=%s&LineRef=%s", URL, MONITORING_REF, LINE_REF);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .cert_pem = (const char *)ca_cert_pem_start, 
        .user_data = response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "apiKey", API_KEY);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP GET Status = %d, Content Length = %lld\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        printf("HTTP GET request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    // Calculate differences
    long diff1 = calculate_time_difference(now, time1);
    long diff2 = calculate_time_difference(now, time2);

    if (diff1 >= 0) {
        printf("Time1-Now: %ld minutes\n", diff1);
    }
    if (diff2 >= 0) {
        printf("Time2-Now: %ld minutes\n", diff2);
    }

    return diff1 * 100 + diff2;
}


// Tâche pour effectuer le GET
void fetch_bus_data_task(void *pvParameters) {
    

    while (1) {
        char url[512];
        snprintf(url, sizeof(url), "%s?MonitoringRef=%s&LineRef=%s", URL, MONITORING_REF, LINE_REF);

        esp_http_client_config_t config = {
            .url = url,
            .event_handler = http_event_handler,
            .cert_pem = (const char *)ca_cert_pem_start, 
            .user_data = response_buffer,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "apiKey", API_KEY);

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            printf("HTTP GET Status = %d, Content Length = %lld\n",
                   esp_http_client_get_status_code(client),
                   esp_http_client_get_content_length(client));
        } else {
            printf("HTTP GET request failed: %s\n", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        // printf("Now: %s\n", now);
        // printf("Time1: %s\n", time1);
        // printf("Time2: %s\n", time2);

        // Calculate differences
        long diff1 = calculate_time_difference(now, time1);
        long diff2 = calculate_time_difference(now, time2);

        if (diff1 >= 0) {
            printf("Time1 - Now: %ld minutes\n", diff1);
        }
        if (diff2 >= 0) {
            printf("Time2 - Now: %ld minutes\n", diff2);
        }

        // Attendre 2 minutes avant la prochaine requête
        vTaskDelay(pdMS_TO_TICKS(2 * 60 * 1000));
    }
}
