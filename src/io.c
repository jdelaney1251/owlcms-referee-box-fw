#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(io_mod, LOG_LEVEL_DBG);

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include "io.h"



#define LED0        DT_ALIAS(led0)
#define LED1        DT_ALIAS(led1)
#define LED2        DT_ALIAS(led2)
#define LED3        DT_ALIAS(led3)
// #define LED0_PIN    DT_GPIO_PIN(DT_ALIAS(led0), gpios)
// #define LED1_PIN    DT_GPIO_PIN(DT_ALIAS(led1), gpios)
// #define LED2_PIN    DT_GPIO_PIN(DT_ALIAS(led2), gpios)
// #define LED3_PIN    DT_GPIO_PIN(DT_ALIAS(led3), gpios)
#define NUM_LEDS    4

#define BTN_USR     DT_ALIAS(btn_usr)
#define BTN_RED     DT_ALIAS(btn_red)
#define BTN_BLK     DT_ALIAS(btn_blk)

#define NUM_BTNS    3

#define SW_ID0      DT_ALIAS(sw_id_0)
#define SW_ID1      DT_ALIAS(sw_id_1)
#define SW_ID2      DT_ALIAS(sw_id_2)

#define NUM_SW_ID   3

#define BZR_0       DT_ALIAS(bzr0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3, gpios);

static const struct gpio_dt_spec *leds[] = { 
                                        &led0, 
                                        &led1,
                                        &led2,
                                        &led3};

static const struct gpio_dt_spec btns[] = { GPIO_DT_SPEC_GET(BTN_USR, gpios),
                                            GPIO_DT_SPEC_GET(BTN_RED, gpios),
                                            GPIO_DT_SPEC_GET(BTN_BLK, gpios)};


static const struct gpio_dt_spec sw_id[] = {  GPIO_DT_SPEC_GET(SW_ID0, gpios),
                                            GPIO_DT_SPEC_GET(SW_ID1, gpios),
                                            GPIO_DT_SPEC_GET(SW_ID2, gpios)};

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(BZR_0);


#define BTN_USR_ID              0
#define BTN_RED_ID              1
#define BTN_BLK_ID              2

#define BTN_STATE_NULL          0
#define BTN_STATE_DOWN          1
#define BTN_STATE_UP            2
#define BTN_STATE_HOLD          3

#define BTN_DEBOUNCE_NONE       0
#define BTN_DEBOUNCE_PRESS      1
#define BTN_DEBOUNCE_RELEASE    2


#define BTN_DEBOUNCE_TIME_MS    10
#define BTN_EVT_TICK_PERIOD_MS  5

#define BZR_PERIOD_NS           1000000U // 1kHz
#define BZR_PULSE_WIDTH_NS      BZR_PERIOD_NS / 2U

struct gpio_callback btn_cb_data[NUM_BTNS];

typedef struct btn_data_state {
    uint8_t btn_id;
    uint8_t debounce_evt; 
    uint64_t evt_start;
    uint8_t debounced_state;
    const struct device *port;
    uint32_t pin_mask;
} btn_state_t;

static btn_state_t btn_state_data[NUM_BTNS];
void (*btn_evt_handlers[NUM_BTNS])(uint8_t);
void (*btn_blk_cb)(uint8_t evt_type);
struct k_thread btn_evt_th;
K_THREAD_STACK_DEFINE(btn_evt_thread_stack, 1024);
static struct k_mutex btn_state_data_mutex;
void (*btn_evt_cb)(uint8_t btn_id, uint8_t btn_evt);

struct k_thread led_mgmt_th;
K_THREAD_STACK_DEFINE(led_mgmt_thread_stack, 1024);

//static struct k_mutex leds_cfg_lock;
//static struct k_mutex buzzer_cfg_lock;
static struct k_msgq leds_cfg_queue;
static char __aligned(2) leds_cfg_msg_buf[5 * sizeof(leds_cfg_t)];

int io_led_bits_set(uint8_t led_bits);
int io_led_bits_toggle(uint8_t led_bits);
int io_led_bits_toggle_all();

void iterate_led_cfg(leds_cfg_t cfg);

void led_mgmt_thread();

int init_btns();
int init_leds();
int init_dev_id_sw();

void btn_evt_thread(void *unused, void *unused1, void *unused2)
{
    uint64_t curr_time_ticks = 0;
    LOG_INF("entering btn evt thread");
    while (1)
    {
        if (k_mutex_lock(&btn_state_data_mutex, K_MSEC(100)) == 0)
        {
            curr_time_ticks = k_uptime_get();
            for (uint8_t i = 0; i < NUM_BTNS; i++)
            {
                if (btn_state_data[i].debounce_evt != BTN_DEBOUNCE_NONE)
                {
                    if ((curr_time_ticks - btn_state_data[i].evt_start) 
                                > BTN_DEBOUNCE_TIME_MS)
                    {
                        if ((gpio_pin_get_dt(&btns[i]) == 1)
                            && (btn_state_data[i].debounce_evt == BTN_DEBOUNCE_PRESS))
                        {
                            btn_state_data[i].debounce_evt = BTN_DEBOUNCE_NONE;
                            btn_state_data[i].debounced_state = BTN_STATE_DOWN;
                            btn_state_data[i].evt_start = curr_time_ticks;
                            btn_evt_cb(btn_state_data[i].btn_id, BTN_EVT_PRESSED);
                        }
                        else if ((gpio_pin_get_dt(&btns[i]) == 0)
                                && (btn_state_data[i].debounce_evt == BTN_DEBOUNCE_RELEASE))
                        {
                            btn_state_data[i].debounce_evt = BTN_DEBOUNCE_NONE;
                            btn_state_data[i].debounced_state = BTN_STATE_UP;
                            btn_state_data[i].evt_start = curr_time_ticks;
                            btn_evt_cb(btn_state_data[i].btn_id, BTN_EVT_RELEASED);
                        }
                    }
                }
                else {
                    if (btn_state_data[i].debounced_state == BTN_STATE_DOWN)
                    {
                        if ((curr_time_ticks - btn_state_data[i].evt_start) 
                                >= 2000)
                        {
                            btn_state_data[i].debounce_evt = BTN_DEBOUNCE_NONE;
                            btn_state_data[i].debounced_state = BTN_STATE_HOLD;
                            btn_evt_cb(btn_state_data[i].btn_id, BTN_EVT_HOLD_2s);
                        }
                    }
                }
            }
            k_mutex_unlock(&btn_state_data_mutex);
        }
        //LOG_INF("tick");
        //k_yield();
        //k_sleep(K_MSEC(BTN_EVT_TICK_PERIOD_MS));
        k_msleep(BTN_EVT_TICK_PERIOD_MS);
    }
}

void iterate_led_cfg(leds_cfg_t cfg)
{
    if (cfg.cfg_type == LED_CFG_TYPE_ON_OFF)
    {
        io_led_bits_set(cfg.pattern);
    }
    else if (cfg.cfg_type == LED_CFG_TYPE_BLINK_STATIC)
    {
        io_led_bits_toggle(cfg.pattern);
    }
    else if (cfg.cfg_type == LED_CFG_TYPE_BLINK_ALT)
    {
        io_led_bits_toggle_all(0x0F);
    }
    else
    {
        // LEDs disabled (off), do nothing 
    }
}

void led_mgmt_thread()
{
    int ret = 0;
    leds_cfg_t leds_cfg;
    static uint8_t leds_curr_state = 0;

    LOG_DBG("Entering LED mgmt thread");

    while(1)
    {
        ret = k_msgq_get(&leds_cfg_queue, &leds_cfg, K_NO_WAIT);
        if (ret == 0)
        {
            // clear all LEDs so they're off
            io_led_bits_set(leds_cfg.pattern);
            leds_curr_state = leds_cfg.pattern;
        }
        else 
        {
            // if we haven't changed the LEDs config, keep doing the same thing
            iterate_led_cfg(leds_cfg);
            
            
            
        }
        //LOG_DBG("tick");
        k_msleep(500);
    }
}

void io_btn_cb(uint8_t btn_id, uint8_t evt_type)
{
    LOG_DBG("btn: %d, evt: %d", btn_id, evt_type);
     
    if (btn_evt_handlers[btn_id] != NULL)
    {
        btn_evt_handlers[btn_id](evt_type); 
    }
}


void io_btn_handler(const struct device *dev, struct gpio_callback *cb, 
            uint32_t pins)
{
    //LOG_INF("btn press: %d\n", pins);
    k_mutex_lock(&btn_state_data_mutex, K_FOREVER);
    for (uint8_t i = 0; i < NUM_BTNS; i++)
    {
        if ((btn_state_data[i].pin_mask == pins) && 
                (btn_state_data[i].port == dev))
        {
            if (gpio_pin_get_dt(&btns[i]))
            {
                if (btn_state_data[i].debounce_evt == BTN_DEBOUNCE_NONE)
                {
                    btn_state_data[i].debounce_evt = BTN_DEBOUNCE_PRESS;
                    btn_state_data[i].evt_start = k_uptime_get();
                }
            }
            else {
                if (btn_state_data[i].debounce_evt == BTN_DEBOUNCE_NONE)
                {
                    btn_state_data[i].debounce_evt = BTN_DEBOUNCE_RELEASE;
                    btn_state_data[i].evt_start = k_uptime_get();
                }
            }
        }
    }
    k_mutex_unlock(&btn_state_data_mutex);
}

int init_btns()
{
    int ret = 0;
    btn_evt_cb = io_btn_cb;
    
    for (uint8_t i = 0; i < NUM_BTNS; i++)
    {
        if (btns[i].port == NULL)
        {
            LOG_ERR("Failed to initialise GPIO device for button %d", i);
            return EIO;
        }

        ret = gpio_pin_configure_dt(&btns[i], GPIO_INPUT);
        if (ret != 0)
        {
            LOG_ERR("Failed to configure IO pin for button %d", i);
            return EIO;
        }

        ret = gpio_pin_interrupt_configure_dt(&btns[i], GPIO_INT_EDGE_BOTH);
        if (ret != 0)
        {
            LOG_ERR("Failed to configure interrupt for button %d", i);
            return EIO;
        }

        gpio_init_callback(&(btn_cb_data[i]), io_btn_handler, BIT(btns[i].pin));
        gpio_add_callback(btns[i].port, &(btn_cb_data[i]));

        btn_state_data[i].btn_id = i;
        btn_state_data[i].port = btns[i].port;
        btn_state_data[i].pin_mask = BIT(btns[i].pin);
        btn_state_data[i].debounce_evt = 0;
        btn_state_data[i].evt_start = 0;
        btn_state_data[i].debounced_state = gpio_pin_get_dt(&btns[i]);

        k_mutex_init(&btn_state_data_mutex);

        k_thread_create(&btn_evt_th, btn_evt_thread_stack,
                        K_THREAD_STACK_SIZEOF(btn_evt_thread_stack),
                        btn_evt_thread,
                        NULL, NULL, NULL,
                        6, 0, K_NO_WAIT);
    }

    return ret;
}

int init_leds()
{
    int ret = 0;
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        //LOG_INF("init led %d\n", i);
        //k_sleep(K_MSEC(1000));
        if (leds[i]->port == NULL)
        {
            LOG_ERR("Failed to initialise GPIO device for LED %d", i);
            return EIO;
        }

        //LOG_INF("init led %d\n", i);
        //k_sleep(K_MSEC(1000));
        ret = gpio_pin_configure_dt(&(*leds[i]), GPIO_OUTPUT);
        if (ret != 0)
        {
            LOG_ERR("Failed to configure IO pin for LED %d", i);
            return EIO;
        }
        //LOG_INF("ret %d\n", i);

        ret = gpio_pin_set_dt(&(*leds[i]), 0);
        //LOG_INF("ret %d\n", i);
    }

    k_msgq_init(&leds_cfg_queue, leds_cfg_msg_buf,
                        sizeof(leds_cfg_t), 5);

    k_thread_create(&led_mgmt_th, led_mgmt_thread_stack,
                        K_THREAD_STACK_SIZEOF(led_mgmt_thread_stack),
                        led_mgmt_thread,
                        NULL, NULL, NULL,
                        6, 0, K_NO_WAIT);
    return ret;
}

int init_dev_id_sw()
{
    int ret = 0;
    for (uint8_t i = 0; i < NUM_SW_ID; i++)
    {
        if (sw_id[i].port == NULL)
        {
            LOG_ERR("Failed to initialise GPIO device for id switch %d", i);
            return EIO;
        }

        ret = gpio_pin_configure_dt(&btns[i], GPIO_INPUT);
        if (ret != 0)
        {
            LOG_ERR("Failed to configure IO pin for id switch %d, ret %d", i, ret);
            return EIO;
        }
    }
    return ret;
}

int init_buzzer()
{
    int ret = 0;
    if (!device_is_ready(buzzer.dev))
    {
        LOG_ERR("Failed to initialise PWM device for buzzer");
        return EIO;
    }

    // make sure the buzzer is definitely off for good measure...
    ret = pwm_set_dt(&buzzer, 0, 0);
    if (ret != 0)
    {
        LOG_ERR("Failed to set initial buzzer PWM config");
        return -EIO;
    }

    return ret;
}

int io_init()
{
    int ret = 0;
    LOG_INF("Starting init\n");
    init_leds();
    init_btns();
    init_dev_id_sw();
    LOG_INF("Finishing init\n");
    return 0;
}

int io_set_leds_cfg(leds_cfg_t cfg)
{
    int ret = 0;
    ret = k_msgq_put(&leds_cfg_queue, &cfg, K_NO_WAIT);
    return ret;
}

int io_set_led(uint8_t led)
{
    int ret = 0;
    if (led <= NUM_LEDS-1)
    {
        ret = gpio_pin_set_dt(&(*leds[led]), 1);
        return ret;
    }
    else
        return EIO;
}

int io_clr_led(uint8_t led)
{
    int ret = 0;
    if (led < NUM_LEDS)
    {
        ret = gpio_pin_set_dt(&(*leds[led]), 0);
        return ret;
    }
    else
        return EIO;
}

int io_led_bits_set(uint8_t led_bits)
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        gpio_pin_set_dt(&(*leds[i]), (led_bits & (1 << i)));
    }
    return 0;
}

int io_led_bits_toggle(uint8_t led_bits)
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        // toggle led if bit set
        if (led_bits & (1 << i))
            gpio_pin_toggle_dt(&(*leds[i]));
    }
    return 0;
}

int io_led_bits_toggle_all()
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        gpio_pin_toggle_dt(&(*leds[i]));
    }
    return 0;
}

int io_get_red_btn_state()
{
    int val = gpio_pin_get_dt(&btns[BTN_RED_ID]);
    return val;
}

int io_get_dev_id()
{
    int ret = 0;
    int id0 = gpio_pin_get_dt(&sw_id[0]);
    int id1 = gpio_pin_get_dt(&sw_id[1]);
    int id2 = gpio_pin_get_dt(&sw_id[2]);

    ret = (id0) + (id1 << 1) + (id2 << 2);
    return ret;
}

void io_reg_cb_btn_usr(void (*cb)(uint8_t))
{
    btn_evt_handlers[BTN_USR_ID] = cb;
}

void io_reg_cb_btn_red(void (*cb)(uint8_t))
{
    btn_evt_handlers[BTN_RED_ID] = cb;
}

void io_reg_cb_btn_blk(void (*cb)(uint8_t))
{
    btn_evt_handlers[BTN_BLK_ID] = cb;
}

int io_buzzer_on()
{
    int ret = 0;
    LOG_DBG("turning buzzer on");
    ret = pwm_set_dt(&buzzer, BZR_PERIOD_NS, BZR_PULSE_WIDTH_NS);
    if (ret != 0)
    {
        LOG_ERR("Failed to turn buzzer on, ret: %d", ret);
    }
    return ret;
}

int io_buzzer_off()
{
    int ret = 0;
    LOG_DBG("turning buzzer off");
    ret = pwm_set_dt(&buzzer, BZR_PERIOD_NS, 0);
    if (ret != 0)
    {
        LOG_ERR("Failed to turn buzzer off, ret: %d", ret);
    }
    return ret;
}