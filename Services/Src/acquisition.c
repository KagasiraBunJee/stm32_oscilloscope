#include "acquisition.h"

#include <stddef.h>
#include <string.h>

#include "calibration.h"
#include "ui_events.h"
#include "recording.h"

#define ADC_TIMER_CLOCK_HZ 72000000UL
#define ADC_MAX_PER_CONVERTER_HZ 2400000UL
#define RAW_WORD_CAPACITY 1536U
#define CAPTURE_SECTION __attribute__((section(".ccmram"), aligned(4)))

typedef union {
    uint32_t words[RAW_WORD_CAPACITY];
    uint16_t halfwords[RAW_WORD_CAPACITY * 2U];
} acquisition_dma_buffer_t;

static ADC_HandleTypeDef g_adc[OSC_MAX_CHANNELS];
static DMA_HandleTypeDef g_adc_dma;
static TIM_HandleTypeDef g_sample_timer;
static acquisition_dma_buffer_t g_dma_buffer __attribute__((aligned(4)));
static osc_capture_t g_capture CAPTURE_SECTION;
/* Used only after DMA acquisition has stopped; normal SRAM is appropriate. */
static uint16_t g_rotate_scratch[REC_SETS]
    __attribute__((aligned(4)));
static acquisition_config_t g_config;
static volatile bool g_dma_error;
static acquisition_state_t g_state;
static uint8_t g_enabled_logical[OSC_MAX_CHANNELS];
static uint8_t g_enabled_count;
static uint32_t g_dma_transfers_per_frame;
static bool g_word_dma;
static bool g_continuous;
static bool g_interleaved;
static bool g_divider_sampling;
static uint16_t g_latest[REC_SETS * REC_CHANNELS];
static volatile bool g_latest_ready;
static uint64_t g_sample_index;
static uint64_t g_epoch_start;
static uint64_t g_epoch_index;
static uint32_t g_epoch;
static uint32_t g_display_at;

uint32_t acquisition_max_sample_rate(const acquisition_config_t *config)
{
    uint32_t count = 0U;
    bool divider = false;
    if (config == NULL) return 0U;
    for (uint32_t i = 0U; i < OSC_MAX_CHANNELS; ++i) {
        if (!config->channel[i].enabled) continue;
        if (config->channel[i].route != OSC_INPUT_DIRECT &&
            config->channel[i].route != OSC_INPUT_HIGH_RANGE) return 0U;
        ++count;
        divider |= config->channel[i].route == OSC_INPUT_HIGH_RANGE;
    }
    if (count == 0U) return 0U;
    return divider ? OSC_DIVIDER_MAX_RATE_HZ :
        ((count == 1U) ? 7200000U : ADC_MAX_PER_CONVERTER_HZ);
}

static uint32_t integer_sqrt_u64(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = 1ULL << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

static uint32_t adc_channel_for(uint8_t logical_channel,
                                osc_input_route_t route)
{
    static const uint32_t direct[] = {
        ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3
    };
    static const uint32_t high[] = {
        ADC_CHANNEL_10, ADC_CHANNEL_11, ADC_CHANNEL_12
    };
    return (route == OSC_INPUT_HIGH_RANGE) ? high[logical_channel]
                                           : direct[logical_channel];
}

static void gpio_init_analog_inputs(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void adc_deinit_all(void)
{
    uint32_t index;
    if (g_sample_timer.Instance != NULL) {
        (void)HAL_TIM_Base_Stop(&g_sample_timer);
        (void)HAL_TIM_Base_DeInit(&g_sample_timer);
        g_sample_timer.Instance = NULL;
    }
    if (g_adc[0].Instance != NULL) {
        (void)HAL_ADCEx_MultiModeStop_DMA(&g_adc[0]);
    }
    for (index = 0U; index < OSC_MAX_CHANNELS; ++index) {
        if (g_adc[index].Instance != NULL) {
            (void)HAL_ADC_Stop(&g_adc[index]);
            (void)HAL_ADC_DeInit(&g_adc[index]);
            g_adc[index].Instance = NULL;
        }
    }
    /*
     * acquisition_configure() can run before DMA has ever been initialized.
     * HAL_DMA_DeInit() dereferences Instance, so never pass it the reset-state
     * handle with a null Instance (that causes a precise bus fault on Start).
     */
    if (g_adc_dma.Instance != NULL) {
        HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
        (void)HAL_DMA_DeInit(&g_adc_dma);
        g_adc_dma.Instance = NULL;
    }
}

static bool timer_configure(uint32_t conversion_rate_hz)
{
    TIM_MasterConfigTypeDef master = {0};
    uint32_t ticks;

    if (conversion_rate_hz == 0U) {
        return false;
    }
    ticks = (ADC_TIMER_CLOCK_HZ + conversion_rate_hz / 2U) /
            conversion_rate_hz;
    if (ticks < 30U) {
        ticks = 30U;
    }
    if (ticks > 65536U) {
        ticks = 65536U;
    }

    __HAL_RCC_TIM2_CLK_ENABLE();
    g_sample_timer.Instance = TIM2;
    g_sample_timer.Init.Prescaler = 0U;
    g_sample_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_sample_timer.Init.Period = ticks - 1U;
    g_sample_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_sample_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&g_sample_timer) != HAL_OK) {
        return false;
    }
    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&g_sample_timer, &master) != HAL_OK) {
        return false;
    }
    g_capture.actual_sample_rate_hz = ADC_TIMER_CLOCK_HZ / ticks;
    if (g_interleaved) {
        g_capture.actual_sample_rate_hz *= 3U;
    }
    return true;
}

static bool adc_configure_instance(uint32_t index, ADC_TypeDef *instance,
                                   uint32_t channel)
{
    ADC_ChannelConfTypeDef channel_config = {0};
    ADC_HandleTypeDef *adc = &g_adc[index];

    adc->Instance = instance;
    adc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    adc->Init.Resolution = ADC_RESOLUTION_12B;
    adc->Init.ScanConvMode = DISABLE;
    adc->Init.ContinuousConvMode = g_continuous ? ENABLE : DISABLE;
    adc->Init.DiscontinuousConvMode = DISABLE;
    adc->Init.NbrOfDiscConversion = 0U;
    adc->Init.ExternalTrigConvEdge = (index == 0U && !g_continuous)
        ? ADC_EXTERNALTRIGCONVEDGE_RISING : ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc->Init.ExternalTrigConv = (index == 0U && !g_continuous)
        ? ADC_EXTERNALTRIGCONV_T2_TRGO : ADC_SOFTWARE_START;
    adc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->Init.NbrOfConversion = 1U;
    adc->Init.DMAContinuousRequests = (index == 0U) ? ENABLE : DISABLE;
    adc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(adc) != HAL_OK) {
        return false;
    }

    channel_config.Channel = channel;
    channel_config.Rank = 1U;
    channel_config.SamplingTime = g_divider_sampling
        ? ADC_SAMPLETIME_480CYCLES : ADC_SAMPLETIME_3CYCLES;
    channel_config.Offset = 0U;
    return HAL_ADC_ConfigChannel(adc, &channel_config) == HAL_OK;
}

static bool hardware_configure(void)
{
    ADC_MultiModeTypeDef multimode = {0};
    uint32_t conversion_rate;
    uint32_t first_channel;

    adc_deinit_all();
    g_divider_sampling = acquisition_max_sample_rate(&g_config) == OSC_DIVIDER_MAX_RATE_HZ;
    g_interleaved = (g_enabled_count == 1U) && !g_divider_sampling;
    gpio_init_analog_inputs();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA mode 2 is word-packed for interleaved triple and dual simultaneous.
       Triple simultaneous uses DMA mode 1 half-words (ADC1, ADC2, ADC3). */
    g_word_dma = (g_enabled_count != 3U);
    g_continuous = g_config.requested_sample_rate_hz ==
        ((g_enabled_count == 1U) ? 7200000U : 2400000U);
    g_adc_dma.Instance = DMA2_Stream0;
    g_adc_dma.Init.Channel = DMA_CHANNEL_0;
    g_adc_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_adc_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    g_adc_dma.Init.MemInc = DMA_MINC_ENABLE;
    g_adc_dma.Init.PeriphDataAlignment = g_word_dma ? DMA_PDATAALIGN_WORD
                                                    : DMA_PDATAALIGN_HALFWORD;
    g_adc_dma.Init.MemDataAlignment = g_word_dma ? DMA_MDATAALIGN_WORD
                                                 : DMA_MDATAALIGN_HALFWORD;
    g_adc_dma.Init.Mode = DMA_CIRCULAR;
    g_adc_dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    g_adc_dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    g_adc_dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    g_adc_dma.Init.MemBurst = DMA_MBURST_SINGLE;
    g_adc_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&g_adc_dma) != HAL_OK) {
        return false;
    }
    __HAL_LINKDMA(&g_adc[0], DMA_Handle, g_adc_dma);
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority(ADC_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    first_channel = adc_channel_for(g_enabled_logical[0],
                    g_config.channel[g_enabled_logical[0]].route);
    if (!adc_configure_instance(0U, ADC1, first_channel)) {
        return false;
    }

    if (g_interleaved) {
        if (!adc_configure_instance(1U, ADC2, first_channel) ||
            !adc_configure_instance(2U, ADC3, first_channel)) {
            return false;
        }
        multimode.Mode = ADC_TRIPLEMODE_INTERL;
        multimode.DMAAccessMode = ADC_DMAACCESSMODE_2;
        g_dma_transfers_per_frame = REC_SETS / 2U;
        conversion_rate = g_config.requested_sample_rate_hz / 3U;
    } else {
        if (!adc_configure_instance(1U, ADC2,
                (g_enabled_count == 1U) ? first_channel :
                adc_channel_for(g_enabled_logical[1],
                    g_config.channel[g_enabled_logical[1]].route))) {
            return false;
        }
        if (g_enabled_count == 3U &&
            !adc_configure_instance(2U, ADC3,
                adc_channel_for(g_enabled_logical[2],
                    g_config.channel[g_enabled_logical[2]].route))) {
            return false;
        }
        /* One divider channel uses dual simultaneous reads of the same pin.
           Keep ADC1 only: uniform timer-spaced samples, no interleaved bursts. */
        multimode.Mode = (g_enabled_count != 3U) ? ADC_DUALMODE_REGSIMULT
                                                 : ADC_TRIPLEMODE_REGSIMULT;
        multimode.DMAAccessMode = (g_enabled_count != 3U)
            ? ADC_DMAACCESSMODE_2 : ADC_DMAACCESSMODE_1;
        g_dma_transfers_per_frame = (g_enabled_count != 3U)
            ? REC_SETS
            : REC_SETS * g_enabled_count;
        conversion_rate = g_config.requested_sample_rate_hz;
    }
    multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    if (HAL_ADCEx_MultiModeConfigChannel(&g_adc[0], &multimode) != HAL_OK) {
        return false;
    }
    if (g_continuous) {
        g_capture.actual_sample_rate_hz = (g_enabled_count == 1U)
            ? 7200000U : 2400000U;
        return true;
    }
    return timer_configure(conversion_rate);
}

static void calculate_measurements(uint32_t channel_index)
{
    osc_capture_channel_t *channel = &g_capture.channel[channel_index];
    const calibration_path_t calibration = {
        channel->gain_uv_per_lsb_q16, channel->offset_uv
    };
    int64_t sum = 0;
    uint64_t sum_squares = 0U;
    int32_t minimum = INT32_MAX;
    int32_t maximum = INT32_MIN;
    uint32_t previous_crossing = 0U;
    uint64_t period_sum = 0U;
    uint32_t periods = 0U;
    uint32_t sample;
    int32_t threshold;
    int32_t previous;
    bool have_crossing = false;

    for (sample = 0U; sample < g_capture.sample_count; ++sample) {
        int32_t value = calibration_apply(&calibration,
                         g_capture.samples[channel_index][sample]);
        int64_t centered;
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
        sum += value;
        centered = value;
        sum_squares += (uint64_t)(centered * centered);
    }

    channel->measurements.minimum_uv = minimum;
    channel->measurements.maximum_uv = maximum;
    channel->measurements.average_uv = (int32_t)(sum / g_capture.sample_count);
    channel->measurements.peak_to_peak_uv = (uint32_t)(maximum - minimum);
    channel->measurements.rms_uv = integer_sqrt_u64(
        sum_squares / g_capture.sample_count);
    threshold = (int32_t)(sum / g_capture.sample_count);
    previous = calibration_apply(&calibration,
        g_capture.samples[channel_index][0]);
    for (sample = 1U; sample < g_capture.sample_count; ++sample) {
        int32_t value = calibration_apply(&calibration,
            g_capture.samples[channel_index][sample]);
        if ((previous < threshold) && (value >= threshold)) {
            if (have_crossing) {
                period_sum += sample - previous_crossing;
                ++periods;
            }
            previous_crossing = sample;
            have_crossing = true;
        }
        previous = value;
    }
    if (periods != 0U) {
        uint64_t average_period_samples = period_sum / periods;
        channel->measurements.frequency_millihz = (uint32_t)(
            ((uint64_t)g_capture.actual_sample_rate_hz * 1000ULL) /
            average_period_samples);
        channel->measurements.period_ns = (uint32_t)(
            (average_period_samples * 1000000000ULL) /
            g_capture.actual_sample_rate_hz);
    } else {
        channel->measurements.frequency_millihz = 0U;
        channel->measurements.period_ns = 0U;
    }
}

static uint16_t find_trigger(void)
{
    uint32_t index;
    const uint16_t level = g_config.trigger_level_raw;
    const uint16_t *samples = g_capture.samples[0];

    for (index = 1U; index < g_capture.sample_count; ++index) {
        bool match = (g_config.trigger_edge == OSC_TRIGGER_RISING)
            ? ((samples[index - 1U] < level) && (samples[index] >= level))
            : ((samples[index - 1U] > level) && (samples[index] <= level));
        if (match) {
            return (uint16_t)index;
        }
    }
    return UINT16_MAX;
}

static void rotate_to_trigger(uint16_t found)
{
    uint32_t channel;
    uint32_t sample;
    uint32_t source_start = (found + g_capture.sample_count -
                            g_config.trigger_position % g_capture.sample_count) %
                            g_capture.sample_count;

    for (channel = 0U; channel < g_enabled_count; ++channel) {
        for (sample = 0U; sample < g_capture.sample_count; ++sample) {
            g_rotate_scratch[sample] = g_capture.samples[channel]
                [(source_start + sample) % g_capture.sample_count];
        }
        memcpy(g_capture.samples[channel], g_rotate_scratch,
               g_capture.sample_count * sizeof(uint16_t));
    }
}

static void decode_bank(uint8_t bank)
{
    uint32_t sample;
    uint32_t channel;
    (void)bank;
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    for (sample=0; sample<REC_SETS; ++sample)
        for (channel=0; channel<g_enabled_count; ++channel)
            g_capture.samples[channel][sample]=g_latest[sample*3U+channel]&0xfffU;
    g_latest_ready=false;
    __set_PRIMASK(irq);
    g_capture.sample_count = REC_SETS;
    g_capture.channel_count = g_enabled_count;
    g_capture.trigger_mode = g_config.trigger_mode;
    g_capture.trigger_edge = g_config.trigger_edge;
    g_capture.trigger_level_raw = g_config.trigger_level_raw;
    for (channel = 0U; channel < g_enabled_count; ++channel) {
        uint8_t logical = g_enabled_logical[channel];
        const calibration_path_t *calibration = calibration_get(
            logical, g_config.channel[logical].route);
        g_capture.channel[channel].logical_channel = logical;
        g_capture.channel[channel].route = g_config.channel[logical].route;
        g_capture.channel[channel].adc_channel = (uint8_t)adc_channel_for(
            logical, g_config.channel[logical].route);
        g_capture.channel[channel].gain_uv_per_lsb_q16 =
            calibration->gain_uv_per_lsb_q16;
        g_capture.channel[channel].offset_uv = calibration->offset_uv;
    }

    {
        uint16_t trigger = find_trigger();
        g_capture.trigger_valid = trigger != UINT16_MAX;
        if (trigger != UINT16_MAX) {
            rotate_to_trigger(trigger);
            g_capture.trigger_index = g_config.trigger_position % REC_SETS;
        } else {
            g_capture.trigger_index = 0U;
        }
    }
    for (channel = 0U; channel < g_enabled_count; ++channel) {
        calculate_measurements(channel);
    }
    ++g_capture.sequence;
    (void)ui_post_event(UI_EVENT_CAPTURE_READY, g_capture.sequence);
}

void acquisition_init(void)
{
    memset(&g_config, 0, sizeof(g_config));
    memset(&g_capture, 0, sizeof(g_capture));
    memset(&g_adc, 0, sizeof(g_adc));
    memset(&g_adc_dma, 0, sizeof(g_adc_dma));
    memset(&g_sample_timer, 0, sizeof(g_sample_timer));
    g_config.channel[0].enabled = true;
    g_config.channel[0].route = OSC_INPUT_HIGH_RANGE;
    g_config.channel[0].vertical_offset_uv = 8000000;
    g_config.channel[0].microvolts_per_div = 4000000U;
    g_config.channel[1].vertical_offset_uv = 1650000;
    g_config.channel[1].microvolts_per_div = 825000U;
    g_config.channel[2].vertical_offset_uv = 1650000;
    g_config.channel[2].microvolts_per_div = 825000U;
    for (uint32_t i=0; i<OSC_MAX_CHANNELS; ++i) {
        g_config.channel[i].enabled=true;
        g_config.channel[i].route=OSC_INPUT_HIGH_RANGE;
        g_config.channel[i].vertical_offset_uv=8000000;
        g_config.channel[i].microvolts_per_div=4000000U;
        g_enabled_logical[i]=(uint8_t)i;
    }
    g_config.requested_sample_rate_hz = OSC_DIVIDER_MAX_RATE_HZ;
    g_config.trigger_mode = OSC_TRIGGER_AUTO;
    g_config.trigger_edge = OSC_TRIGGER_RISING;
    g_config.trigger_level_raw = 2048U;
    g_config.trigger_position = REC_SETS / 2U;
    g_enabled_logical[0] = 0U;
    g_enabled_count = 3U;
    g_state = ACQUISITION_IDLE;
}

bool acquisition_configure(const acquisition_config_t *config)
{
    uint32_t index;
    uint32_t max_rate;
    if ((config == NULL) || (g_state == ACQUISITION_RUNNING) || recording_busy()) {
        return false;
    }
    g_enabled_count = 0U;
    for (index = 0U; index < OSC_MAX_CHANNELS; ++index) {
        if (config->channel[index].enabled) {
            g_enabled_logical[g_enabled_count++] = (uint8_t)index;
        }
    }
    if ((g_enabled_count == 0U) ||
        (config->trigger_level_raw > 4095U) ||
        (config->trigger_position >= OSC_SAMPLES_PER_CHANNEL)) {
        return false;
    }
    max_rate = acquisition_max_sample_rate(config);
    if ((config->requested_sample_rate_hz < 1100U) ||
        (config->requested_sample_rate_hz > max_rate)) {
        return false;
    }
    g_config = *config;
    return hardware_configure();
}

void acquisition_set_display(const acquisition_config_t *config)
{
    if(!config)return;
    g_config.trigger_mode=config->trigger_mode;
    g_config.trigger_edge=config->trigger_edge;
    g_config.trigger_level_raw=config->trigger_level_raw;
    g_config.trigger_position=config->trigger_position;
    for(unsigned i=0;i<3;i++) {
        g_config.channel[i].microvolts_per_div=config->channel[i].microvolts_per_div;
        g_config.channel[i].vertical_offset_uv=config->channel[i].vertical_offset_uv;
    }
}

bool acquisition_start(void)
{
    uint32_t dma_length;
    if (g_state == ACQUISITION_RUNNING) return false;
    if (!hardware_configure()) {
        g_state = ACQUISITION_ERROR;
        return false;
    }
    g_dma_error = false;
    g_capture.sequence = 0U;
    /* ADC3 participates in single-channel triple-interleaved mode as well as
       three-channel simultaneous mode. Start every configured slave. */
    if (g_adc[2].Instance != NULL && HAL_ADC_Start(&g_adc[2]) != HAL_OK) {
        g_state = ACQUISITION_ERROR;
        return false;
    }
    if (HAL_ADC_Start(&g_adc[1]) != HAL_OK) {
        g_state = ACQUISITION_ERROR;
        return false;
    }
    dma_length = g_dma_transfers_per_frame * 2U;
    ++g_epoch;
    g_epoch_start=recording_clock_us();
    g_epoch_index=g_sample_index;
    if (HAL_ADCEx_MultiModeStart_DMA(&g_adc[0], g_dma_buffer.words,
                                     dma_length) != HAL_OK ||
        (!g_continuous && HAL_TIM_Base_Start(&g_sample_timer) != HAL_OK)) {
        g_state = ACQUISITION_ERROR;
        return false;
    }
    g_state = ACQUISITION_RUNNING;
    return true;
}

static bool halt(acquisition_state_t new_state)
{
    if (g_state != ACQUISITION_RUNNING) {
        return false;
    }
    if (!g_continuous) (void)HAL_TIM_Base_Stop(&g_sample_timer);
    (void)HAL_ADCEx_MultiModeStop_DMA(&g_adc[0]);
    (void)HAL_ADC_Stop(&g_adc[1]);
    if (g_adc[2].Instance != NULL) {
        (void)HAL_ADC_Stop(&g_adc[2]);
    }
    g_state = new_state;
    return g_capture.sequence != 0U;
}

bool acquisition_pause(void)
{
    return halt(ACQUISITION_PAUSED);
}

bool acquisition_resume(void)
{
    uint32_t sequence;
    if (g_state != ACQUISITION_PAUSED) {
        return false;
    }
    sequence = g_capture.sequence;
    if (!acquisition_start()) return false;
    /* Keep the displayed paused frame valid until the next coherent DMA bank. */
    g_capture.sequence = sequence;
    return true;
}

bool acquisition_stop(void)
{
    if (g_state == ACQUISITION_PAUSED) {
        g_state = ACQUISITION_STOPPED;
        return g_capture.sequence != 0U;
    }
    return halt(ACQUISITION_STOPPED);
}

void acquisition_process(void)
{
    if (g_dma_error) {
        recording_adc_error();
        bool paused=g_state==ACQUISITION_PAUSED;
        g_state = ACQUISITION_ERROR;
        (void)ui_post_event(UI_EVENT_ACQUISITION_ERROR, 0U);
        g_dma_error = false;
        if(paused){g_state=ACQUISITION_PAUSED;return;}
        (void)acquisition_start();
        return;
    }
    /* Display decimation only: ISR-owned recording blocks remain untouched. */
    if (g_latest_ready && (uint32_t)(HAL_GetTick()-g_display_at)>=100U) {
        g_display_at=HAL_GetTick();
        decode_bank(0U);
    }
}

acquisition_state_t acquisition_get_state(void)
{
    return g_state;
}

const acquisition_config_t *acquisition_get_config(void)
{
    return &g_config;
}

const osc_capture_t *acquisition_get_frozen_capture(void)
{
    return (g_capture.sequence != 0U) ? &g_capture : NULL;
}

bool acquisition_has_capture(void)
{
    return g_capture.sequence != 0U;
}

DMA_HandleTypeDef *acquisition_dma_handle(void)
{
    return &g_adc_dma;
}

static void completed_half(uint8_t bank)
{
    if(g_dma_error)return; /* A half affected by an ADC fault is not trustworthy. */
    const uint16_t *half=&g_dma_buffer.halfwords[bank*g_dma_transfers_per_frame];
    const uint32_t *words=&g_dma_buffer.words[bank*g_dma_transfers_per_frame];
    if (!g_word_dma) {
        memcpy(g_latest,half,REC_PAYLOAD);
    } else for(uint32_t i=0;i<REC_SETS;i++) {
        if(g_interleaved) g_latest[i*3U]=(uint16_t)(words[i/2U]>>((i%2U)*16U));
        else {
            g_latest[i*3U]=(uint16_t)words[i];
            if(g_enabled_count==2U)g_latest[i*3U+1U]=(uint16_t)(words[i]>>16);
        }
    }
    g_latest_ready=true;
    bool pause=false;
    if(g_enabled_count==3U && g_capture.actual_sample_rate_hz==10000U)
        pause=recording_ingest(half,g_sample_index,g_epoch,
            g_epoch_start+(g_sample_index-g_epoch_index)*100ULL);
    g_sample_index+=REC_SETS;
    if(pause) {
        __HAL_TIM_DISABLE(&g_sample_timer);
        g_state=ACQUISITION_PAUSED;
    }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc == &g_adc[0]) {
        completed_half(0U);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc == &g_adc[0]) {
        completed_half(1U);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc)
{
    if (adc == &g_adc[0]) {
        g_dma_error = true;
        if(g_sample_timer.Instance!=NULL)__HAL_TIM_DISABLE(&g_sample_timer);
    }
}

void acquisition_adc_irq(void)
{
    if(g_adc[0].Instance!=NULL)HAL_ADC_IRQHandler(&g_adc[0]);
}
