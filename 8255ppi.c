#include "8255ppi.h"
pthread_t ppi_threads[MAX_PPI_CONNECTIONS];
pthread_mutex_t cpu_access_mutex;
extern int program_loaded;
extern pthread_cond_t cpu_started;
extern pthread_mutex_t broadcast_mutex;
extern pthread_mutex_t io_signal_mutex;
pthread_cond_t external_strobe_signal = PTHREAD_COND_INITIALIZER;
pthread_mutex_t external_strobe_signal_mutex;
int address_latch, data_latch;
int current_thread_count = 0;
int cpu_mutex_created = 0;
void reset_thread_count() {
    current_thread_count = 0;
}
void connect_ppi(ppi_8255 * ppi, Sim8085 * simulator, int chip_select, int int_a_pin, int int_b_pin) {
    ppi->mpu = simulator;
    ppi->chip_select = chip_select;
    ppi->control_register = 0;
    ppi->thread_id = current_thread_count++;
    ppi->intr_a_pin = int_a_pin;
    ppi->intr_b_pin = int_b_pin; 
    if (!cpu_mutex_created) {
        cpu_mutex_created = 1;
        initialize_cpu_mutex();
    }
    pthread_create(&ppi_threads[current_thread_count++], 0, ppi_mpu_thread_handler, (void*)ppi);
    pthread_create(&ppi_threads[current_thread_count++], 0, ppi_peripheral_thread_handler, (void*)ppi);
}

int get_ppi_port_value(ppi_8255 * ppi, int port_no) {
    port_no &= 3;//avoid out of bounds
    if (port_no == 0) return ppi->porta;
    if (port_no == 1) return ppi->portb;
    if (port_no == 2) return ppi->portc;
    return ppi->control_register;
}
int is_bsr(int mode_info) {
    return (mode_info >> 7) == 0;
}
int port_a_dir(ppi_8255 * ppi) {
    return (ppi->control_register >> 4) & 1;
}
int port_b_dir(ppi_8255 * ppi) {
    return (ppi->control_register >> 1) & 1;
}
int group_a_mode(ppi_8255 * ppi) {
    return ((ppi->control_register) >> 5) & 3;
}
int group_b_mode(ppi_8255 * ppi) {
    return (ppi->control_register >> 2) & 1;
}
void interrupt_mpu(ppi_8255 * pinfo, int pin) {
    if (pin == PIN_TRAP) pinfo->mpu->trap = 1;
    else if (pin == PIN_RST_7_5) pinfo->mpu->rst7_5 = 1;
    else if (pin == PIN_RST_6_5) pinfo->mpu->rst6_5 = 1;
    else if (pin == PIN_RST_5_5) pinfo->mpu->rst5_5 = 1;
}
//needs redo, only works for mode 1 input for now 
void * ppi_peripheral_thread_handler(void * t_info) {
    ppi_8255 * pinfo = (ppi_8255*)t_info;
    while (1) {
        pthread_mutex_lock(&broadcast_mutex);
        pthread_cond_wait(&cpu_started, &broadcast_mutex);
        pthread_mutex_unlock(&broadcast_mutex);
        while (program_loaded && !hasHalted(pinfo->mpu)) {
            pthread_mutex_lock(&external_strobe_signal_mutex);
            pthread_cond_wait(&external_strobe_signal, &external_strobe_signal_mutex);
            if (pinfo->stb_a_signal) {
                pinfo->stb_a_signal = 0;
                if (group_a_mode(pinfo) > 0) {//if ppi is in mode 1 or 2
                    if (port_a_dir(pinfo) == 1) {
                        pinfo->porta = pinfo->port_a_input_latch;
                        if (pinfo->inte_a) {
                            interrupt_mpu(pinfo, pinfo->intr_a_pin);
                        }
                    } //else no meaning to stb_a signal
                } else printf("group a not in mode 1 or 2\n");
            } else if (pinfo->stb_b_signal) {
                pinfo->stb_b_signal = 0;
                if (group_b_mode(pinfo) == 1) {//if ppi is in mode 1
                    if (port_b_dir(pinfo) == 1) {
                        pinfo->portb = pinfo->port_b_input_latch;
                        if (pinfo->inte_b) interrupt_mpu(pinfo, pinfo->intr_b_pin);
                    } //else no meaning to stb_a signal
                } else printf("group b not in mode 1\n");
            }
            pthread_mutex_unlock(&external_strobe_signal_mutex);
        }
    }
}
void* ppi_mpu_thread_handler(void * t_info) { //each ppi device has its own thread
    ppi_8255 * pinfo = (ppi_8255*)t_info;
    while (1) {
        pthread_mutex_lock(&broadcast_mutex);
        pthread_cond_wait(&cpu_started, &broadcast_mutex);
        pthread_mutex_unlock(&broadcast_mutex);
        while (program_loaded && !hasHalted(pinfo->mpu)) {
            pthread_mutex_lock(&io_signal_mutex);
            pthread_cond_wait(&io_signal, &io_signal_mutex); //wait for io signal
            int addr = pinfo->mpu->address_latch & 3;
            if ((pinfo->mpu->address_latch >> 2) == pinfo->chip_select) {
                if (pinfo->mpu->iow == 0) {//ior signal
                    if (addr == 0) { //read from port a
                        int mode = group_a_mode(pinfo);
                        if (mode == 0) pinfo->mpu->data_latch = pinfo->porta;
                    } else if (addr == 1) {
                        int mode = group_b_mode(pinfo);
                        if (mode == 0) pinfo->mpu->data_latch = pinfo->portb;
                    } else if (addr == 2) {
                        int mode = group_b_mode(pinfo);
                        if (mode == 0) pinfo->mpu->data_latch = pinfo->portc;
                        else if (mode == 1) {
                            //mode 1 => status read from port c
                            //add more information to status word
                            //currently only interrupt flip bits are set
                            //status word is dependent on direction of group A and group B
                            //TODO: change this later to account for each 4 possible cases
                            int val = (pinfo->inte_a << 4) | (pinfo->inte_b << 2);
                            pinfo->mpu->data_latch = val;
                        } else if (mode == 2) {
                            //todo:
                        }
                    }
                } else {//iow signal
                    //printf("write signal\n");
                    int data = pinfo->mpu->data_latch;
                    if (addr == 3 ) { //write to control register
                        if (is_bsr(data)) {
                            int bitpos = (data >> 1) & 0x7;
                            pinfo->portc = (pinfo->portc) & (~(1 << bitpos)); 
                            pinfo->portc = (pinfo->portc) | ((data & 1) << bitpos);
                        } else { //new io configuration
                            pinfo->control_register = data;
                        }
                    } if (addr == 0) { //read from port a
                        int mode = group_a_mode(pinfo);
                        if (mode == 0) pinfo->porta = data;
                    } else if (addr == 1) {
                        int mode = group_b_mode(pinfo);
                        if (mode == 0) pinfo->portb = data;
                    } else if (addr == 2) {
                        pinfo->portc = pinfo->mpu->data_latch;
                    }
                    //update interrupt flip-flops
                    pinfo->inte_a = (pinfo->portc >> 4) & 1;
                    pinfo->inte_b = (pinfo->portc >> 2) & 1;
                }
            }
            pthread_mutex_unlock(&io_signal_mutex);
        }
    }
    pthread_exit((void*)0);
}
void initialize_cpu_mutex() {
    pthread_mutex_init(&cpu_access_mutex, 0);
    pthread_mutex_init(&external_strobe_signal_mutex, 0);
}
