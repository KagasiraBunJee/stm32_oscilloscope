#include "rtc_service.h"

#include <stdio.h>

#include "stm32f4xx_hal.h"

#define RTC_VALID_MAGIC 0x4F534331UL

static RTC_HandleTypeDef g_rtc;
static bool g_rtc_ready;

static bool is_leap(uint32_t year)
{
    return ((year % 4U) == 0U) &&
           (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

static uint32_t days_before_year(uint32_t year)
{
    uint32_t days = 0U;
    uint32_t current;
    for (current = 1970U; current < year; ++current) {
        days += is_leap(current) ? 366U : 365U;
    }
    return days;
}

static uint32_t days_before_month(uint32_t year, uint32_t month)
{
    static const uint16_t cumulative[] = {
        0U, 31U, 59U, 90U, 120U, 151U,
        181U, 212U, 243U, 273U, 304U, 334U
    };
    uint32_t days = cumulative[month - 1U];
    if ((month > 2U) && is_leap(year)) ++days;
    return days;
}

static uint64_t calendar_to_unix(uint32_t year, uint32_t month, uint32_t day,
                                 uint32_t hour, uint32_t minute,
                                 uint32_t second)
{
    uint64_t days = days_before_year(year) + days_before_month(year, month) +
                    day - 1U;
    return days * 86400ULL + hour * 3600ULL + minute * 60ULL + second;
}

static bool unix_to_calendar(uint64_t unix_seconds, uint32_t *year,
                             uint32_t *month, uint32_t *day,
                             uint32_t *hour, uint32_t *minute,
                             uint32_t *second)
{
    static const uint8_t month_days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };
    uint64_t days = unix_seconds / 86400ULL;
    uint32_t current_year = 1970U;
    uint32_t current_month = 1U;
    uint32_t days_in_month;
    uint32_t seconds_in_day;

    while (current_year <= 2099U) {
        uint32_t days_in_year = is_leap(current_year) ? 366U : 365U;
        if (days < days_in_year) break;
        days -= days_in_year;
        ++current_year;
    }
    if (current_year < 2000U || current_year > 2099U) return false;
    while (current_month <= 12U) {
        days_in_month = month_days[current_month - 1U];
        if ((current_month == 2U) && is_leap(current_year)) ++days_in_month;
        if (days < days_in_month) break;
        days -= days_in_month;
        ++current_month;
    }

    seconds_in_day = (uint32_t)(unix_seconds % 86400ULL);
    *year = current_year;
    *month = current_month;
    *day = (uint32_t)days + 1U;
    *hour = seconds_in_day / 3600U;
    *minute = (seconds_in_day % 3600U) / 60U;
    *second = seconds_in_day % 60U;
    return true;
}

bool rtc_service_init(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_PeriphCLKInitTypeDef peripheral_clock = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) {
        oscillator.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        oscillator.LSEState = RCC_LSE_ON;
        oscillator.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) return false;
    }
    peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    peripheral_clock.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock) != HAL_OK) return false;
    __HAL_RCC_RTC_ENABLE();

    g_rtc.Instance = RTC;
    g_rtc.Init.HourFormat = RTC_HOURFORMAT_24;
    g_rtc.Init.AsynchPrediv = 127U;
    g_rtc.Init.SynchPrediv = 255U;
    g_rtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    g_rtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    g_rtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    g_rtc_ready = HAL_RTC_Init(&g_rtc) == HAL_OK;
    return g_rtc_ready;
}

bool rtc_service_is_valid(void)
{
    return g_rtc_ready &&
           (HAL_RTCEx_BKUPRead(&g_rtc, RTC_BKP_DR0) == RTC_VALID_MAGIC);
}

bool rtc_service_set_utc(uint64_t unix_seconds)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint32_t year, month, day, hour, minute, second;
    uint32_t days;

    if (!g_rtc_ready || !unix_to_calendar(unix_seconds, &year, &month, &day,
                                          &hour, &minute, &second)) {
        return false;
    }
    days = (uint32_t)(unix_seconds / 86400ULL);
    time.Hours = (uint8_t)hour;
    time.Minutes = (uint8_t)minute;
    time.Seconds = (uint8_t)second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    date.Year = (uint8_t)(year - 2000U);
    date.Month = (uint8_t)month;
    date.Date = (uint8_t)day;
    date.WeekDay = (uint8_t)(((days + 3U) % 7U) + 1U);
    if ((HAL_RTC_SetTime(&g_rtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_SetDate(&g_rtc, &date, RTC_FORMAT_BIN) != HAL_OK)) {
        return false;
    }
    HAL_RTCEx_BKUPWrite(&g_rtc, RTC_BKP_DR0, RTC_VALID_MAGIC);
    return true;
}

bool rtc_service_get_utc(uint64_t *unix_seconds)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    if ((unix_seconds == NULL) || !rtc_service_is_valid()) return false;
    if ((HAL_RTC_GetTime(&g_rtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&g_rtc, &date, RTC_FORMAT_BIN) != HAL_OK)) {
        return false;
    }
    *unix_seconds = calendar_to_unix(2000U + date.Year, date.Month, date.Date,
                                     time.Hours, time.Minutes, time.Seconds);
    return true;
}

bool rtc_service_format_filename(char *buffer, size_t size,
                                 uint32_t session_id)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    if ((buffer == NULL) || (size < 24U) || !rtc_service_is_valid()) return false;
    if ((HAL_RTC_GetTime(&g_rtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&g_rtc, &date, RTC_FORMAT_BIN) != HAL_OK)) return false;
    (void)snprintf(buffer, size, "%04u%02u%02u_%02u%02u%02u_%03lu.OSC",
                   2000U + date.Year, date.Month, date.Date,
                   time.Hours, time.Minutes, time.Seconds,
                   (unsigned long)(session_id % 1000U));
    return true;
}

uint32_t rtc_service_fattime(void)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    if (!rtc_service_is_valid() ||
        (HAL_RTC_GetTime(&g_rtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&g_rtc, &date, RTC_FORMAT_BIN) != HAL_OK)) {
        return ((2020U - 1980U) << 25) | (1U << 21) | (1U << 16);
    }
    return ((uint32_t)(date.Year + 20U) << 25) |
           ((uint32_t)date.Month << 21) |
           ((uint32_t)date.Date << 16) |
           ((uint32_t)time.Hours << 11) |
           ((uint32_t)time.Minutes << 5) |
           ((uint32_t)time.Seconds >> 1);
}
