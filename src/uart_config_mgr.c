#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_config_mgr, LOG_LEVEL_DBG);

#include "uart_config_mgr.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log_ctrl.h>
#include <errno.h>
#include "msys.h"
#include "settings_util.h"
#include "stdlib.h"
#include "device_config.h"


#define UART_LOG_BACKEND        "log_backend_uart"
#define BUF_SIZE                    256
#define PARAM_BUF_SIZE              32
#define CRC_POLY                    0x16
#define UART_PKT_DELIM              0xFF


/**
 * -------------------------------------------------------------------------------------------------------------------
 * |            |           |              |              |             |                    |                       |
 * |  PREAMLE   |  CRC      |   CMD        |   PARAM LEN  |   VAL LEN   |  PARAM             |   VAL                 |
 * | (1 byte)   |  (1 byte) |   (1 byte)   |   (1 byte)   |    (1 byte) | (param_len bytes)  |   (val_len bytes)     |
 * -------------------------------------------------------------------------------------------------------------------
 * 
 *  Preambe is a start of pkt byte
 *  CRC does not include preamble (includes everything else)
 */
#define UART_PKT_OFFSET_PREAMBLE    0x0
#define UART_PKT_OFFSET_CRC         0x1
#define UART_PKT_OFFSET_CMD         0x2
#define UART_PKT_OFFSET_PARAM_LEN   0x3
#define UART_PKT_OFFSET_DATA_LEN    0x4
#define UART_PKT_OFFSET_PARAM       0x5

#define UART_MAGIC_UPPER            0x72
#define UART_MAGIC_LOWER            0x19 // magic value to switch modes
#define UART_PKT_PREAMBLE           0x69
#define UART_CMD_READY              0x01
#define UART_CMD_READ_PARAM         0x02
#define UART_CMD_WRITE_PARAM        0x03
#define UART_CMD_WRITE_COMMIT       0x04

#define UART_CMD_RSP_OK             0xB0
#define UART_CMD_RSP_WRITE_OK       0xB1
#define UART_CMD_RSP_WRITE_ERR      0xB2
#define UART_CMD_RSP_READ_OK        0xB3
#define UART_CMD_RSP_READ_ERR       0xB4
#define UART_CMD_RSP_ERR            0xBA
#define UART_CMD_RSP_RX_ERR         0xBB
#define UART_CMD_RSP_CRC_ERR        0xBC

static const struct log_backend *log_backend_uart;
static const struct device *uart_dev = DEVICE_DT_GET(UART_CONFIG_DEV_NAME_DT);
static struct k_work_delayable uart_proc_work;
static uint8_t uart_rx_msg_buf[BUF_SIZE];
static uint8_t uart_rx_buf_pos;
static uint8_t uart_tx_msg_buf[BUF_SIZE];
static uint8_t uart_tx_buf_pos;

static uint8_t param_buf[PARAM_BUF_SIZE];
static uint8_t data_buf[PARAM_BUF_SIZE];

typedef struct {
    uint8_t crc;
    uint8_t cmd;
    uint8_t param_len;
    uint8_t data_len;
    uint8_t *param;
    uint8_t *data;
} uart_pkt_t;

typedef struct {
    const char * param_name;
    uint8_t *param;
} config_params_t;

static bool uart_err_flag;

static struct config_settings settings;
static struct wifi_config_settings wifi_settings;
static struct mqtt_config_settings mqtt_settings;
static struct owlcms_config_settings owlcms_settings;
static uint8_t wifi_ssid[32] = {};
static uint8_t wifi_psk[32] = {};
static uint8_t mqtt_srv[32] = {};
static uint8_t mqtt_port[32] = {};
static uint8_t mqtt_client_name[32] = {};
static uint8_t owlcms_platform_name[32] = {};
#define NUM_PARAMS              6

static config_params_t config_params[NUM_PARAMS] = {
    { "wifi_ssid",              wifi_ssid              },
    { "wifi_psk",               wifi_psk               },
    { "mqtt_srv",               mqtt_srv               },
    { "mqtt_port",              mqtt_port              },
    { "mqtt_client_name",       mqtt_client_name       },
    { "owlcms_platform_name",   owlcms_platform_name   },
};

static uint8_t *get_config_param(const char *param, uint8_t len);
void uart_rx_irq(const struct device *dev, void *user_data);
void uart_pkt_proc();
uint8_t proc_read_cmd(uint8_t *buf, uint8_t buf_len, uint8_t *rsp_buf);
uint8_t proc_write_cmd(uint8_t *buf);
void uart_write_pkt(const struct device *dev, uart_pkt_t pkt);
static bool config_mgr_running = false;

// search through the list of config params for a matching string
// returns a pointer to the config param buffer or NULL if not found
static uint8_t *get_config_param(const char *param, uint8_t len)
{
    for (uint8_t i = 0; i < NUM_PARAMS; i++)
    {
        if (!strncmp(param, config_params[i].param_name, len))
        {
            return config_params[i].param;
        }
    }
    return NULL;
}

int uart_config_mgr_init()
{
    // log_backend_uart = log_backend_get_by_name(UART_LOG_BACKEND);

    k_work_init_delayable(&uart_proc_work, uart_pkt_proc);

    for (uint32_t i = 0; i < BUF_SIZE; i++)
    {
        uart_rx_msg_buf[i] = 0;
        uart_tx_msg_buf[i] = 0;
    }
    
    settings.wifi = &wifi_settings;
    settings.mqtt = &mqtt_settings;
    settings.owlcms = &owlcms_settings;

    if (!device_is_ready(uart_dev))
    {
        LOG_ERR("UART device not_found");
        return -1;
    }

    uart_irq_callback_set(uart_dev, uart_rx_irq);
    uart_irq_rx_enable(uart_dev);
    LOG_DBG("uart config init done");

    return 0;
}

int uart_config_mgr_start()
{
    if (!config_mgr_running)
    {
        // disable uart logging after a short delay
        LOG_INF("config enable");
        k_sleep(K_MSEC(100));
        // log_backend_disable(log_backend_uart);

        // load settings data from nvm util
        settings_util_load_wifi_config(settings.wifi);
        settings_util_load_mqtt_config(settings.mqtt);
        settings_util_load_owlcms_config(settings.owlcms);

        // copy settings to temporary buffers we can R/W from/to
        strcpy(wifi_ssid, settings.wifi->ssid);
        strcpy(wifi_psk, settings.wifi->psk);
        strcpy(mqtt_srv, settings.mqtt->broker_addr);
        strcpy(mqtt_client_name, settings.mqtt->client_name);
        mqtt_port[0] = settings.mqtt->port & 0x00FF;
        mqtt_port[1] = (settings.mqtt->port & 0xFF00) >> 8;
        strcpy(owlcms_platform_name, settings.owlcms->platform);

        // signal to the state machine that we are entering config mode
        config_mgr_running = true;
        msys_signal_evt(SYS_EVT_CONFIG);
        return 0;
    }
    else 
    {
        return -1;
    }
}

int uart_config_mgr_stop()
{
    if (config_mgr_running)
    {
        // log_backend_enable(log_backend_uart,
        //                    log_backend_uart->cb->ctx,
        //                    LOG_LEVEL_DBG);
        k_sleep(K_MSEC(100));
        LOG_INF("config disable");

        config_mgr_running = false;
        msys_signal_evt(SYS_EVT_CONFIG_END);
        return 0;
    }
    else
    {
        return -1;
    }
}

void uart_rx_irq(const struct device *dev, void *user_data)
{
    uint8_t read_char = 0;
    LOG_DBG("uart_irq");

    if (!uart_irq_update(uart_dev))
    {
        return;
    }

    while (uart_irq_rx_ready(uart_dev))
    {
        uart_fifo_read(uart_dev, &read_char, 1);
        LOG_DBG("got char on uart %x", read_char);

        if ((read_char == UART_PKT_DELIM) && uart_rx_buf_pos > 0)
        {
            uart_rx_msg_buf[uart_rx_buf_pos] = '\0';
            uart_rx_buf_pos = 0;
        }
        else if (uart_rx_buf_pos < (BUF_SIZE - 1))
        {
            uart_rx_msg_buf[uart_rx_buf_pos++] = read_char;
        }
        else
        {
            uart_err_flag = true;
        }
    }
    k_work_schedule(&uart_proc_work, K_NO_WAIT);
}

void uart_pkt_proc()
{
    uart_pkt_t rsp_pkt = {
        .cmd = UART_CMD_RSP_ERR, 
        .crc = 0xFF, 
        .param_len = 0x00, 
        .data_len = 0x00
    };
    uint8_t buf_pos = 0;
    uint8_t data_len = 0;

    if (uart_err_flag)
    {
        // setup and ERR response packet
        rsp_pkt.cmd = UART_CMD_RSP_RX_ERR;
        // clear the error
        uart_err_flag = false;
        LOG_INF("rx err");
    }
    else if (uart_rx_msg_buf[buf_pos] == UART_PKT_PREAMBLE)
    {
        buf_pos++;
        LOG_INF("rx pkt");
        // check crc here. If not right, return err rsp
        uint8_t crc_rx = uart_rx_msg_buf[buf_pos++];
        uint8_t crc_calc = crc_rx;
        if (crc_rx == crc_calc)
        {
            // check pkt cmd type here, return err if bad
            uint8_t cmd = uart_rx_msg_buf[buf_pos++];
            rsp_pkt.param_len = uart_rx_msg_buf[buf_pos++];
            rsp_pkt.data_len = uart_rx_msg_buf[buf_pos++];
            switch (cmd)
            {
                case UART_CMD_READY:
                    rsp_pkt.cmd = UART_CMD_RSP_OK;
                    break;
                case UART_CMD_WRITE_PARAM:
                    if (proc_write_cmd(uart_rx_msg_buf+UART_PKT_OFFSET_DATA_LEN) > 0)
                    {
                        rsp_pkt.cmd = UART_CMD_RSP_WRITE_OK;
                    }
                    else 
                    {
                        rsp_pkt.cmd = UART_CMD_RSP_WRITE_ERR;
                    }
                    break;
                case UART_CMD_READ_PARAM:
                    // copy the param name we're reading to the buffer for the sending pkt
                    memcpy(param_buf, uart_rx_msg_buf+UART_PKT_OFFSET_PARAM, rsp_pkt.param_len);
                    data_len = proc_read_cmd(param_buf, rsp_pkt.param_len, data_buf);
                    if (data_len < 0xFF)
                    {
                        rsp_pkt.cmd = UART_CMD_RSP_READ_OK;
                        rsp_pkt.data_len = data_len;
                        rsp_pkt.param = param_buf;
                        rsp_pkt.data = data_buf;
                    }
                    else
                    {
                        rsp_pkt.cmd = UART_CMD_RSP_READ_ERR;
                    }
                    break;
                default:
                    rsp_pkt.cmd = UART_CMD_RSP_ERR;
                    break;
            }
        }
        else
        {
            rsp_pkt.cmd = UART_CMD_RSP_CRC_ERR;
        }
    }
    else if (uart_rx_msg_buf[buf_pos] == UART_MAGIC_UPPER &&
             uart_rx_msg_buf[buf_pos+1] == UART_MAGIC_LOWER)
    {
        if (!config_mgr_running)
        {
            if (!uart_config_mgr_start())
            {
                rsp_pkt.cmd = UART_CMD_RSP_OK;
                rsp_pkt.param_len = 0;
                rsp_pkt.data_len = 0;
            }
        }
        else 
        {
            if (!uart_config_mgr_stop())
            {
                rsp_pkt.cmd = UART_CMD_RSP_OK;
                rsp_pkt.param_len = 0;
                rsp_pkt.data_len = 0;
            }
        }
    }
    else
    {
        rsp_pkt.cmd = UART_CMD_RSP_ERR;
        rsp_pkt.param_len = 0;
        rsp_pkt.data_len = 0;
        LOG_INF("err");
    }

    // send the response packet out over the uart device
    uart_write_pkt(uart_dev, rsp_pkt);
}

uint8_t proc_read_cmd(uint8_t *buf, uint8_t buf_len, uint8_t *rsp_buf)
{
    uint8_t rsp_len = 0;
    uint8_t *param = NULL;
    // search through config params for a matching name
    param = get_config_param(buf, buf_len);
    if (param != NULL)
    {
        // if a match was found, copy the data from the returned param 
        // buffer into rsp_buf
        strcpy(rsp_buf, param);
        rsp_len = strlen(rsp_buf);
    }
    else
    {
        // if no match was found, set rsp_len to 0xFF to signal this
        // rsp_buf should be unchanged
        rsp_len = 0xFF;
    }
    return rsp_len;
}

uint8_t proc_write_cmd(uint8_t *buf)
{
    // ret is the number of bytes written
    uint8_t ret = 0;
    // extract the param_len and data_len bytes from the packet
    uint8_t param_len = buf[0];
    uint8_t data_len = buf[1];
    uint8_t *param = NULL;
    // search through the list of config params for a matching name
    param = get_config_param(buf+2, param_len);
    if (param != NULL)
    {
        // if a match was found, copy the data from buf at the offset
        // of param_len plus the param_len byte
        // copy data_len+1 so we get a null terminator at the end of our
        // string to comply with settings_util requirements
        strncpy(param, buf+(param_len+2), data_len+1);
        // return value is the number of bytes written to the config param
        ret = strlen(param);
    }
    else
    {
        ret = 0;
    }
    return ret;
}

void uart_write_pkt(const struct device *dev, uart_pkt_t pkt)
{
    uint8_t buf_pos = 0;
    uint8_t crc = 0x00;

    uart_tx_msg_buf[buf_pos++] = UART_PKT_PREAMBLE;
    // write a placeholder CRC for now
    uart_tx_msg_buf[buf_pos++] = 0x00;
    // write the cmd response
    uart_tx_msg_buf[buf_pos++] = pkt.cmd;
    // write the param and data field lengths
    uart_tx_msg_buf[buf_pos++] = pkt.param_len;
    uart_tx_msg_buf[buf_pos++] = pkt.data_len;

    // only write param and data values if both have non-zero length
    if (pkt.param_len > 0 && pkt.data_len > 0)
    {
        memcpy(uart_tx_msg_buf, pkt.param, pkt.param_len);
        buf_pos += pkt.param_len;
        memcpy(uart_tx_msg_buf, pkt.data, pkt.data_len);
        buf_pos += pkt.param_len;
    }

    // calculate and update CRC byte
    //crc = crc8(uart_tx_msg_buf+2, buf_pos-1, CRC_POLY, 69, false);
    crc = 0x11;
    uart_tx_msg_buf[1] = crc;

    for (uint8_t i = 0; i < buf_pos; i++)
    {
        uart_poll_out(dev, uart_tx_msg_buf[i]);
    }
    uart_poll_out(dev, UART_PKT_DELIM);
}
