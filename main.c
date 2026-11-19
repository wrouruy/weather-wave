#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

struct Memory {
    char *response;
    size_t size;
};

static size_t write_cb(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;
    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;
    return realsize;
}

static char *http_get(CURL *curl, const char *url) {
    struct Memory chunk = { malloc(1), 0 };
    if (!chunk.response) return NULL;
    chunk.response[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "weather-wave/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(chunk.response);
        return NULL;
    }
    return chunk.response;
}

// WMO weather interpretation codes used by open-meteo
static const char *weather_desc(int code) {
    if (code == 0) return "Clear sky";
    if (code == 1) return "Mainly clear";
    if (code == 2) return "Partly cloudy";
    if (code == 3) return "Overcast";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 55) return "Drizzle";
    if (code == 56 || code == 57) return "Freezing drizzle";
    if (code >= 61 && code <= 65) return "Rain";
    if (code == 66 || code == 67) return "Freezing rain";
    if (code >= 71 && code <= 75) return "Snow fall";
    if (code == 77) return "Snow grains";
    if (code >= 80 && code <= 82) return "Rain showers";
    if (code == 85 || code == 86) return "Snow showers";
    if (code == 95) return "Thunderstorm";
    if (code == 96 || code == 99) return "Thunderstorm with hail";
    return "Unknown";
}

static void last_update(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%d.%m.%Y %H:%M", t);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <city>\n", argv[0]);
        return 1;
    }

    const char *city = argv[1];
    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    char *encoded_city = curl_easy_escape(curl, city, 0);
    if (!encoded_city) {
        curl_easy_cleanup(curl);
        return 1;
    }

    // 1. Geocode the city name
    char geo_url[256];
    snprintf(geo_url, sizeof(geo_url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json",
             encoded_city);
    curl_free(encoded_city);

    char *geo_json = http_get(curl, geo_url);
    if (!geo_json) { curl_easy_cleanup(curl); return 1; }

    cJSON *root = cJSON_Parse(geo_json);
    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        printf("City not found.\n");
        cJSON_Delete(root);
        free(geo_json);
        curl_easy_cleanup(curl);
        return 1;
    }

    cJSON *first = cJSON_GetArrayItem(results, 0);
    cJSON *lat_item = cJSON_GetObjectItemCaseSensitive(first, "latitude");
    cJSON *lon_item = cJSON_GetObjectItemCaseSensitive(first, "longitude");
    if (!cJSON_IsNumber(lat_item) || !cJSON_IsNumber(lon_item)) {
        printf("Unable to retrieve location data.\n");
        cJSON_Delete(root);
        free(geo_json);
        curl_easy_cleanup(curl);
        return 1;
    }
    double lat = lat_item->valuedouble;
    double lon = lon_item->valuedouble;

    cJSON *country_item = cJSON_GetObjectItemCaseSensitive(first, "country");
    const char *country = cJSON_IsString(country_item) ? country_item->valuestring : "";

    // 2. Fetch the current weather
    char weather_url[512];
    snprintf(weather_url, sizeof(weather_url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
             "&timezone=auto",
             lat, lon);

    char *weather_json = http_get(curl, weather_url);
    if (!weather_json) {
        cJSON_Delete(root);
        free(geo_json);
        curl_easy_cleanup(curl);
        return 1;
    }

    cJSON *wroot = cJSON_Parse(weather_json);
    cJSON *current = cJSON_GetObjectItemCaseSensitive(wroot, "current");
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
    cJSON *wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
    cJSON *code_item = cJSON_GetObjectItemCaseSensitive(current, "weather_code");

    if (cJSON_IsNumber(temp) && cJSON_IsNumber(code_item)) {
        char updated[32];
        last_update(updated, sizeof(updated));

        printf("⠀          ⣀⠴⠢⠤⣀            ⠀& WeatherWave &\n");
        printf("       ⡔⣝⢏⠁⠀⠀⢀⣤⣿⣤⣀⣀⡀         ---------------\n");
        printf("⠀⠀⢀⠤⢉⡩⠟⣓⡿⠁⠀⠀⡖⠁⠀⠀⠀⠀⠀⠉⠳⣄⡀⠀⠀⠀⠀⠀⠀location . : %s / %s\n", city, country);
        printf("⡠⣒⢯⢕⣫⡯⠗⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⣦⡀⠀⠀⠀⠀temperature: %.1f°C\n", temp->valuedouble);
        printf("⠉⡜⢛⣿⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⣿⡄⠀⠀status . . : %s\n", weather_desc(code_item->valueint));
        if (cJSON_IsNumber(wind) && cJSON_IsNumber(humidity))
            printf("  ⢻⣿⣿⣇⣀                ⡀⣀⣿⡿  description: wind %.1f km/h, humidity %.0f%%\n",
                   wind->valuedouble, humidity->valuedouble);
        printf("⠀  ⠉⠉⠁⠉⠛⠛⠉⠉⠉⠉⠁⠉⠁⠁⠁⠉⠉⠉⠒⠉⠁⠉⠀⠀⠀⠀last_uptade: %s\n", updated);
    } else {
        printf("Unable to retrieve data\n");
    }

    cJSON_Delete(wroot);
    free(weather_json);
    cJSON_Delete(root);
    free(geo_json);
    curl_easy_cleanup(curl);
    return 0;
}
