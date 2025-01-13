#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <math.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

extern char *now;
extern char *time1;
extern char *time2;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
int compare_dates(const char *date1, const char *date2);
void extract_values_and_compare(const char *json_data);
esp_err_t http_event_handler(esp_http_client_event_t *evt);
long calculate_time_difference(const char *time1, const char *time2);
void fetch_bus_data_task(void *pvParameters);
void configure_dns();
int get_departure();


#endif // REQUEST_H