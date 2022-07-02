#include <logging/log.h>

LOG_MODULE_REGISTER(sys_mod, LOG_LEVEL_DBG);

#include <zephyr.h>
#include <zephyr/sys/reboot.h>


#include "msys.h"
#include "io_mgr.h"
#include "comms_mgr.h"

/*
*       State machine definitions
*/

typedef enum {
    S_PRE_INIT,
    S_INIT,
    S_IDLE_DCONN,
    S_CONNECTING,
    S_IDLE_CONN,
    S_DECISION_RX,
    S_DECISION_REQ,
    S_CONFIG,
    S_CONFIG_END
} state_t;

typedef enum {
    E_TICK,
    E_ANY,
    E_CONN_SUCCESS,
    E_CONN_LOST,
    E_INP_RED_DECISION,
    E_INP_BLK_DECISION,
    E_DECISION_HANDLED,
    E_DECISION_SEND_ERR,
    E_DECISION_REQ_RX,
    E_TIMEOUT,
    E_CONFIG,
    E_CONFIG_END
} event_t;

typedef struct {
    state_t curr_state;
    event_t evt;
    state_t next_state;
} state_trans_matrix_row_t;

typedef struct {
    const char *name;
    void (*func)(event_t evt);
} state_func_row_t;

typedef struct {
    state_t curr_state;
} state_machine_t;

void state_func_pre_init(event_t evt);
void state_func_pre_init_entry(event_t evt);
void state_func_init(event_t evt);
void state_func_init_entry(event_t evt);
void state_func_idle_dconn(event_t evt);
void state_func_idle_dconn_entry(event_t evt);
void state_func_connecting(event_t evt);
void state_func_connecting_entry(event_t evt);
void state_func_idle_conn(event_t evt);
void state_func_idle_conn_entry(event_t evt);
void state_func_decision_rx(event_t evt);
void state_func_decision_rx_entry(event_t evt);
void state_func_decision_req(event_t evt);
void state_func_decision_req_entry(event_t evt);
void state_func_config_entry(event_t evt);
void state_func_config(event_t evt);
void state_func_config_end_entry(event_t evt);
void state_func_config_end(event_t evt);


void state_machine_iterate();

static state_trans_matrix_row_t state_trans_matrix[] = {
    {S_PRE_INIT,        E_ANY,             S_INIT           },
    {S_INIT,            E_ANY,             S_IDLE_DCONN     },
    {S_IDLE_DCONN,      E_ANY,             S_CONNECTING     },
    {S_IDLE_DCONN,      E_CONFIG,          S_CONFIG         },
    {S_CONNECTING,      E_CONN_SUCCESS,    S_IDLE_CONN      },
    {S_CONNECTING,      E_CONN_LOST,       S_IDLE_DCONN     },
    {S_CONNECTING,      E_CONFIG,          S_CONFIG         },
    {S_IDLE_CONN,       E_INP_RED_DECISION,S_DECISION_RX    },
    {S_IDLE_CONN,       E_INP_BLK_DECISION,S_DECISION_RX    },
    {S_IDLE_CONN,       E_DECISION_REQ_RX, S_DECISION_REQ   },
    {S_IDLE_CONN,       E_CONFIG,          S_CONFIG         },
    {S_DECISION_RX,     E_DECISION_HANDLED,S_IDLE_CONN      },
    {S_DECISION_REQ,    E_TIMEOUT,         S_IDLE_CONN      },
    {S_DECISION_REQ,    E_INP_RED_DECISION,S_DECISION_RX    },
    {S_DECISION_REQ,    E_INP_BLK_DECISION,S_DECISION_RX    },
    {S_IDLE_CONN,       E_CONN_LOST,       S_IDLE_DCONN     },
    {S_DECISION_RX,     E_CONN_LOST,       S_IDLE_DCONN     },
    {S_IDLE_CONN,       E_CONN_LOST,       S_IDLE_DCONN     },
    {S_IDLE_CONN,       E_ANY,             S_IDLE_CONN      },
    {S_CONFIG,          E_ANY,             S_CONFIG         },
    {S_CONFIG,          E_CONFIG_END,      S_CONFIG_END     },
    {S_CONFIG_END,      E_ANY,             S_IDLE_DCONN      }
};

#define STATE_TRANS_MATRIX_NUM_ROWS 22

static state_func_row_t state_func_a[] = {
    {"S_PRE_INIT",      &state_func_pre_init     },
    {"S_INIT",          &state_func_init        },
    {"S_IDLE_DCONN",    &state_func_idle_dconn  },
    {"S_CONNECTING",    &state_func_connecting  },
    {"S_IDLE_CONN",     &state_func_idle_conn   },
    {"S_DECISION_RX",   &state_func_decision_rx },
    {"S_DECISION_REQ",  &state_func_decision_req},
    {"S_CONFIG",        &state_func_config      },
    {"S_CONFIG_END",    &state_func_config_end  }
};

static state_func_row_t state_func_entry[] = {
    {"S_PRE_INIT",      &state_func_pre_init_entry    },
    {"S_INIT",          &state_func_init_entry        },
    {"S_IDLE_DCONN",    &state_func_idle_dconn_entry  },
    {"S_CONNECTING",    &state_func_connecting_entry  },
    {"S_IDLE_CONN",     &state_func_idle_conn_entry   },
    {"S_DECISION_RX",   &state_func_decision_rx_entry },
    {"S_DECISION_REQ",  &state_func_decision_req_entry},
    {"S_CONFIG",        &state_func_config_entry      },
    {"S_CONFIG_END",    &state_func_config_end_entry  }
};

void state_func_pre_init_entry(event_t evt)
{
    // this function never gets called...
    LOG_DBG("Enter pre-init state");
}

void state_func_pre_init(event_t evt)
{
    
}

void state_func_init_entry(event_t evt)
{
    LOG_DBG("Enter init state");
    io_mgr_init();
}

void state_func_init(event_t evt)
{
    LOG_DBG("inside init state");
}

void state_func_idle_dconn_entry(event_t evt)
{
    LOG_DBG("Enter idle-dconn state");
}

void state_func_idle_dconn(event_t evt)
{
    LOG_DBG("inside idle dconn state");
}

void state_func_connecting_entry(event_t evt)
{
    LOG_DBG("Enter connecting state");
    io_mgr_set_leds_connecting();
    comms_mgr_connect();
}

void state_func_connecting(event_t evt)
{
    
}

void state_func_idle_conn_entry(event_t evt)
{
    LOG_DBG("Enter idle-conn state");

    io_mgr_set_leds_disable();
}

void state_func_idle_conn(event_t evt)
{
    
}

void state_func_decision_rx_entry(event_t evt)
{
    LOG_DBG("Enter decision rx state");

    if (evt == SYS_EVT_INP_BLK_DECISION)
    {
        comms_mgr_notify_decision(COMMS_MGR_DEC_BLK);
    }
    else if (evt == SYS_EVT_INP_RED_DECISION)
    {
        comms_mgr_notify_decision(COMMS_MGR_DEC_RED);
    }
}

void state_func_decision_rx(event_t evt)
{
    LOG_DBG("Enter decision req state");
}

void state_func_decision_req_entry(event_t evt)
{

}

void state_func_decision_req(event_t evt)
{
    
}

void state_func_config_entry(event_t evt)
{
    LOG_DBG("Enter config state e:%d", evt);
    io_mgr_set_leds_config();
    comms_mgr_start_config();
}

void state_func_config(event_t evt)
{

}

void state_func_config_end_entry(event_t evt)
{
    LOG_DBG("Enter config end state");
    comms_mgr_end_config();
    sys_reboot(SYS_REBOOT_COLD);
}

void state_func_config_end(event_t evt)
{
    LOG_DBG("config end evt: %d", evt);
}

void state_machine_iterate(state_machine_t *state_machine, event_t evt)
{
    uint8_t state_updated = false;
    for (uint8_t i = 0; i < STATE_TRANS_MATRIX_NUM_ROWS; i++)
    {
        if (state_trans_matrix[i].curr_state == state_machine->curr_state)
        {
            if ((state_trans_matrix[i].evt == evt) || (state_trans_matrix[i].evt == E_ANY)) 
            {
                if (state_machine->curr_state != state_trans_matrix[i].next_state)
                {    
                    state_updated = true;
                    state_machine->curr_state = state_trans_matrix[i].next_state;
                    (state_func_entry[state_machine->curr_state].func)(evt);
                }    
                else
                    (state_func_a[state_machine->curr_state].func)(evt);
            }
        }
    }

    

}



/*
*       System thread and execution stuff
*/

#define SIGNAL_EVT_MAX_RETRIES          10
static struct k_thread msys_th;
K_THREAD_STACK_DEFINE(msys_thread_stack, 2048);

static struct k_msgq  msys_evt_queue;
char __aligned(1) evt_msg_buf[10 * sizeof(event_t)];

void msys_thread();

int msys_init()
{
    k_msgq_init(&msys_evt_queue, evt_msg_buf, sizeof(event_t), 10);   
    return 0;
}

int msys_run()
{
    LOG_DBG("Creating sys thread");
    k_thread_create(&msys_th, msys_thread_stack, 
                             K_THREAD_STACK_SIZEOF(msys_thread_stack),
                             msys_thread,
                             NULL, NULL, NULL,
                             6, 0, K_NO_WAIT);

    return 0;
}

int msys_signal_evt(uint8_t evt)
{
    event_t e = evt;
    uint8_t count = 0;
    LOG_DBG("msys evt %d", evt);
    while (k_msgq_put(&msys_evt_queue, &e, K_NO_WAIT) != 0)
    {
        if (count >= SIGNAL_EVT_MAX_RETRIES)
        {
            LOG_DBG("Failed to put event on msgq");
            return -ETIMEDOUT;
        }
        count++;
    }
    return 0;
}

void msys_thread()
{
    LOG_DBG("Sys thread started");

    event_t evt = E_ANY;
    state_machine_t msys_state_machine;

    msys_state_machine.curr_state = S_PRE_INIT;
    int ret = 0;
    while (1)
    {
        ret = k_msgq_get(&msys_evt_queue, &evt, K_NO_WAIT);
        if (ret != 0)
            evt = E_ANY;
        else 
            LOG_DBG("msys evt rx %d", evt);
        state_machine_iterate(&msys_state_machine, evt);

        k_msleep(1);
        //k_yield();
        //LOG_DBG("tick");
    }
}
