#include "usb_device.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "usb_protocol.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define OSC_USB_IN_EP 0x81U
#define OSC_USB_OUT_EP 0x01U
#define OSC_USB_PACKET_SIZE 64U
#define OSC_USB_CONFIG_DESC_SIZE 32U
#define OSC_USB_MS_VENDOR_CODE 0x20U
#define OSC_USB_MS_OS_20_SIZE 178U

typedef struct __attribute__((packed)) {
    uint16_t set_length;
    uint16_t set_type;
    uint32_t windows_version;
    uint16_t total_length;
    uint16_t config_length;
    uint16_t config_type;
    uint8_t config_value;
    uint8_t config_reserved;
    uint16_t config_total_length;
    uint16_t function_length;
    uint16_t function_type;
    uint8_t first_interface;
    uint8_t function_reserved;
    uint16_t function_total_length;
    uint16_t compatible_length;
    uint16_t compatible_type;
    uint8_t compatible_id[8];
    uint8_t sub_compatible_id[8];
    uint16_t property_length;
    uint16_t property_type;
    uint16_t property_data_type;
    uint16_t property_name_length;
    uint16_t property_name[21];
    uint16_t property_data_length;
    uint16_t property_data[40];
} ms_os_20_descriptor_t;

_Static_assert(sizeof(ms_os_20_descriptor_t) == OSC_USB_MS_OS_20_SIZE,
               "Microsoft OS descriptor length changed");

static PCD_HandleTypeDef g_pcd;
static USBD_HandleTypeDef g_usbd;
static volatile bool g_configured;
static volatile bool g_request_ready;
static volatile bool g_tx_busy;
static volatile bool g_tx_needs_zlp;
static uint8_t g_out_packet[OSC_USB_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t g_rx_frame[OSC_USB_MAX_FRAME] __attribute__((aligned(4)));
static uint8_t g_tx_frame[OSC_USB_MAX_FRAME] __attribute__((aligned(4)));
static volatile uint16_t g_rx_length;
static volatile uint16_t g_rx_expected;
static uint16_t g_tx_length;
static uint8_t g_alt_setting;

static const uint8_t g_device_descriptor[USB_LEN_DEV_DESC] = {
    0x12, USB_DESC_TYPE_DEVICE, 0x10, 0x02, 0x00, 0x00, 0x00,
    USB_MAX_EP0_SIZE, 0x09, 0x12, 0x07, 0xF4, 0x00, 0x01,
    USBD_IDX_MFC_STR, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR, 0x01
};

static const uint8_t g_config_descriptor[OSC_USB_CONFIG_DESC_SIZE] = {
    0x09, USB_DESC_TYPE_CONFIGURATION, OSC_USB_CONFIG_DESC_SIZE, 0x00,
    0x01, 0x01, 0x00, 0x80, 0x7D,
    0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00,
    USBD_IDX_INTERFACE_STR,
    0x07, USB_DESC_TYPE_ENDPOINT, OSC_USB_OUT_EP, 0x02,
    OSC_USB_PACKET_SIZE, 0x00, 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, OSC_USB_IN_EP, 0x02,
    OSC_USB_PACKET_SIZE, 0x00, 0x00
};

static const uint8_t g_device_qualifier[USB_LEN_DEV_QUALIFIER_DESC] = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0x00, 0x00, 0x00, USB_MAX_EP0_SIZE, 0x01, 0x00
};

static const uint8_t g_bos_descriptor[33] = {
    0x05, USB_DESC_TYPE_BOS, 0x21, 0x00, 0x01,
    0x1C, USB_DEVICE_CAPABITY_TYPE, 0x05, 0x00,
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
    0x00, 0x00, 0x03, 0x06,
    OSC_USB_MS_OS_20_SIZE, 0x00, OSC_USB_MS_VENDOR_CODE, 0x00
};

static const ms_os_20_descriptor_t g_ms_os_20_descriptor = {
    .set_length = 10, .set_type = 0, .windows_version = 0x06030000,
    .total_length = OSC_USB_MS_OS_20_SIZE,
    .config_length = 8, .config_type = 1, .config_value = 0,
    .config_total_length = 168,
    .function_length = 8, .function_type = 2, .first_interface = 0,
    .function_total_length = 160,
    .compatible_length = 20, .compatible_type = 3,
    .compatible_id = {'W','I','N','U','S','B',0,0},
    .property_length = 132, .property_type = 4, .property_data_type = 7,
    .property_name_length = 42,
    .property_name = {'D','e','v','i','c','e','I','n','t','e','r','f','a','c','e','G','U','I','D','s',0},
    .property_data_length = 80,
    .property_data = {'{','5','D','4','E','3','F','6','6','-','6','A','9','0','-','4','A','5','7','-','9','C','0','C','-','6','F','6','C','1','B','8','A','4','0','7','E','}',0,0}
};

static uint8_t g_string_descriptor[USBD_MAX_STR_DESC_SIZ];
static uint8_t g_lang_descriptor[USB_LEN_LANGID_STR_DESC] = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING, 0x09, 0x04
};

static uint8_t *device_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed; *length = sizeof(g_device_descriptor);
    return (uint8_t *)(uintptr_t)g_device_descriptor;
}

static uint8_t *lang_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed; *length = sizeof(g_lang_descriptor); return g_lang_descriptor;
}

static uint8_t *string_desc(const char *text, uint16_t *length)
{
    USBD_GetString((uint8_t *)(uintptr_t)text, g_string_descriptor, length);
    return g_string_descriptor;
}

static uint8_t *manufacturer_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; return string_desc("Open Oscilloscope", length); }
static uint8_t *product_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; return string_desc("STM32F407 Oscilloscope", length); }
static uint8_t *configuration_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; return string_desc("WinUSB Configuration", length); }
static uint8_t *interface_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; return string_desc("Oscilloscope Bulk Interface", length); }

static uint8_t *serial_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    char serial[25];
    (void)speed;
    (void)snprintf(serial, sizeof(serial), "%08lX%08lX%08lX",
                   (unsigned long)HAL_GetUIDw0(), (unsigned long)HAL_GetUIDw1(),
                   (unsigned long)HAL_GetUIDw2());
    return string_desc(serial, length);
}

static uint8_t *bos_desc(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed; *length = sizeof(g_bos_descriptor);
    return (uint8_t *)(uintptr_t)g_bos_descriptor;
}

static USBD_DescriptorsTypeDef g_descriptors = {
    device_desc, lang_desc, manufacturer_desc, product_desc, serial_desc,
    configuration_desc, interface_desc, bos_desc
};

static void prepare_out(USBD_HandleTypeDef *pdev)
{
    (void)USBD_LL_PrepareReceive(pdev, OSC_USB_OUT_EP, g_out_packet,
                                 sizeof(g_out_packet));
}

static uint8_t vendor_init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    (void)USBD_LL_OpenEP(pdev, OSC_USB_IN_EP, USBD_EP_TYPE_BULK,
                         OSC_USB_PACKET_SIZE);
    (void)USBD_LL_OpenEP(pdev, OSC_USB_OUT_EP, USBD_EP_TYPE_BULK,
                         OSC_USB_PACKET_SIZE);
    pdev->ep_in[OSC_USB_IN_EP & 0x0FU].is_used = 1U;
    pdev->ep_out[OSC_USB_OUT_EP & 0x0FU].is_used = 1U;
    g_rx_length = 0U;
    g_rx_expected = 0U;
    g_request_ready = false;
    g_tx_busy = false;
    g_configured = true;
    prepare_out(pdev);
    return USBD_OK;
}

static uint8_t vendor_deinit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    (void)USBD_LL_CloseEP(pdev, OSC_USB_IN_EP);
    (void)USBD_LL_CloseEP(pdev, OSC_USB_OUT_EP);
    g_configured = false;
    g_request_ready = false;
    g_tx_busy = false;
    return USBD_OK;
}

static uint8_t vendor_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if (((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR) &&
        (req->bRequest == OSC_USB_MS_VENDOR_CODE) && (req->wIndex == 0x0007U)) {
        uint16_t length = MIN(req->wLength, (uint16_t)sizeof(g_ms_os_20_descriptor));
        (void)USBD_CtlSendData(pdev,
            (uint8_t *)(uintptr_t)&g_ms_os_20_descriptor, length);
        return USBD_OK;
    }
    if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
        if (req->bRequest == USB_REQ_GET_INTERFACE) {
            (void)USBD_CtlSendData(pdev, &g_alt_setting, 1U);
            return USBD_OK;
        }
        if (req->bRequest == USB_REQ_SET_INTERFACE && req->wValue == 0U) {
            g_alt_setting = 0U;
            (void)USBD_CtlSendStatus(pdev);
            return USBD_OK;
        }
    }
    USBD_CtlError(pdev, req);
    return USBD_FAIL;
}

static uint8_t vendor_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if ((epnum & 0x0FU) == (OSC_USB_IN_EP & 0x0FU)) {
        if (g_tx_needs_zlp) {
            g_tx_needs_zlp = false;
            (void)USBD_LL_Transmit(pdev, OSC_USB_IN_EP, NULL, 0U);
        } else {
            g_tx_busy = false;
        }
    }
    return USBD_OK;
}

static uint32_t load_u32(const uint8_t *value)
{
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static uint8_t vendor_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    uint16_t packet_length;
    if ((epnum & 0x0FU) != (OSC_USB_OUT_EP & 0x0FU)) return USBD_OK;
    packet_length = (uint16_t)USBD_LL_GetRxDataSize(pdev, OSC_USB_OUT_EP);
    if (!g_request_ready && packet_length <= (OSC_USB_MAX_FRAME - g_rx_length)) {
        memcpy(&g_rx_frame[g_rx_length], g_out_packet, packet_length);
        g_rx_length = (uint16_t)(g_rx_length + packet_length);
        if (g_rx_length >= OSC_USB_PROTOCOL_HEADER_SIZE && g_rx_expected == 0U) {
            uint32_t payload_length = load_u32(&g_rx_frame[16]);
            uint32_t expected = OSC_USB_PROTOCOL_HEADER_SIZE + payload_length;
            if (expected <= OSC_USB_MAX_FRAME) g_rx_expected = (uint16_t)expected;
            else g_rx_length = 0U;
        }
        if (g_rx_expected != 0U && g_rx_length >= g_rx_expected) {
            g_request_ready = true;
        }
    } else if (!g_request_ready) {
        g_rx_length = 0U;
        g_rx_expected = 0U;
    }
    if (!g_request_ready) prepare_out(pdev);
    return USBD_OK;
}

static uint8_t *config_desc(uint16_t *length)
{ *length = sizeof(g_config_descriptor); return (uint8_t *)(uintptr_t)g_config_descriptor; }
static uint8_t *qualifier_desc(uint16_t *length)
{ *length = sizeof(g_device_qualifier); return (uint8_t *)(uintptr_t)g_device_qualifier; }

static USBD_ClassTypeDef g_vendor_class = {
    vendor_init, vendor_deinit, vendor_setup, NULL, NULL, vendor_data_in,
    vendor_data_out, NULL, NULL, NULL, config_desc, config_desc, config_desc,
    qualifier_desc
};

bool usb_device_init(void)
{
    g_configured = false;
    if (USBD_Init(&g_usbd, &g_descriptors, 0U) != USBD_OK) return false;
    if (USBD_RegisterClass(&g_usbd, &g_vendor_class) != USBD_OK) return false;
    if (USBD_Start(&g_usbd) != USBD_OK) return false;
    return true;
}

void usb_device_process(void)
{
    usb_protocol_process();
}

bool usb_device_is_configured(void) { return g_configured; }

bool usb_device_receive_frame(uint8_t *buffer, uint16_t capacity,
                              uint16_t *length)
{
    uint32_t primask;
    uint16_t size;
    if (buffer == NULL || length == NULL || !g_request_ready) return false;
    primask = __get_PRIMASK();
    __disable_irq();
    size = g_rx_expected;
    if (size > capacity) {
        if (primask == 0U) __enable_irq();
        return false;
    }
    memcpy(buffer, g_rx_frame, size);
    g_rx_length = 0U;
    g_rx_expected = 0U;
    g_request_ready = false;
    if (g_configured) prepare_out(&g_usbd);
    if (primask == 0U) __enable_irq();
    *length = size;
    return true;
}

bool usb_device_send_frame(const uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length > sizeof(g_tx_frame) || !g_configured ||
        g_tx_busy) return false;
    memcpy(g_tx_frame, buffer, length);
    g_tx_length = length;
    g_tx_needs_zlp = (length != 0U) && ((length % OSC_USB_PACKET_SIZE) == 0U);
    g_tx_busy = true;
    if (USBD_LL_Transmit(&g_usbd, OSC_USB_IN_EP, g_tx_frame,
                         g_tx_length) != USBD_OK) {
        g_tx_busy = false;
        return false;
    }
    return true;
}

void usb_device_irq_handler(void) { HAL_PCD_IRQHandler(&g_pcd); }

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    GPIO_InitTypeDef gpio = {0};
    if (hpcd->Instance != USB_OTG_FS) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance != USB_OTG_FS) return;
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{ (void)USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup); }
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ (void)USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff); }
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ (void)USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff); }
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) { (void)USBD_LL_SOF(hpcd->pData); }
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{ (void)USBD_LL_Reset(hpcd->pData); (void)USBD_LL_SetSpeed(hpcd->pData, USBD_SPEED_FULL); }
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) { (void)USBD_LL_Suspend(hpcd->pData); }
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) { (void)USBD_LL_Resume(hpcd->pData); }
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ (void)USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum); }
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ (void)USBD_LL_IsoINIncomplete(hpcd->pData, epnum); }
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) { (void)USBD_LL_DevConnected(hpcd->pData); }
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{ g_configured = false; (void)USBD_LL_DevDisconnected(hpcd->pData); }

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    memset(&g_pcd, 0, sizeof(g_pcd));
    g_pcd.Instance = USB_OTG_FS;
    g_pcd.Init.dev_endpoints = 4U;
    g_pcd.Init.use_dedicated_ep1 = DISABLE;
    g_pcd.Init.dma_enable = DISABLE;
    g_pcd.Init.low_power_enable = DISABLE;
    g_pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
    g_pcd.Init.Sof_enable = DISABLE;
    g_pcd.Init.speed = PCD_SPEED_FULL;
    g_pcd.Init.vbus_sensing_enable = DISABLE;
    g_pcd.Init.lpm_enable = DISABLE;
    g_pcd.pData = pdev; pdev->pData = &g_pcd;
    if (HAL_PCD_Init(&g_pcd) != HAL_OK) return USBD_FAIL;
    (void)HAL_PCDEx_SetRxFiFo(&g_pcd, 0x80U);
    (void)HAL_PCDEx_SetTxFiFo(&g_pcd, 0U, 0x40U);
    (void)HAL_PCDEx_SetTxFiFo(&g_pcd, 1U, 0x80U);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{ return HAL_PCD_DeInit(pdev->pData) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{ return HAL_PCD_Start(pdev->pData) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{ return HAL_PCD_Stop(pdev->pData) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep, uint8_t type, uint16_t mps)
{ return HAL_PCD_EP_Open(pdev->pData, ep, mps, type) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep)
{ return HAL_PCD_EP_Close(pdev->pData, ep) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep)
{ return HAL_PCD_EP_Flush(pdev->pData, ep) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep)
{ return HAL_PCD_EP_SetStall(pdev->pData, ep) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep)
{ return HAL_PCD_EP_ClrStall(pdev->pData, ep) == HAL_OK ? USBD_OK : USBD_FAIL; }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep)
{ PCD_HandleTypeDef *pcd = pdev->pData; return (ep & 0x80U) ? pcd->IN_ep[ep & 0x0FU].is_stall : pcd->OUT_ep[ep].is_stall; }
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t address)
{ return HAL_PCD_SetAddress(pdev->pData, address) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep, uint8_t *data, uint32_t size)
{ return HAL_PCD_EP_Transmit(pdev->pData, ep, data, size) == HAL_OK ? USBD_OK : USBD_FAIL; }
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep, uint8_t *data, uint32_t size)
{ return HAL_PCD_EP_Receive(pdev->pData, ep, data, size) == HAL_OK ? USBD_OK : USBD_FAIL; }
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep)
{ return HAL_PCD_EP_GetRxCount(pdev->pData, ep); }
void USBD_LL_Delay(uint32_t delay) { HAL_Delay(delay); }
