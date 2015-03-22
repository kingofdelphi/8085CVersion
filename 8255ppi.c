#include "8255ppi.h"
pthread_t ppi_threads[MAX_PPI_CONNECTIONS];
pthread_mutex_t cpu_access_mutex;
extern int program_loaded;
extern pthread_cond_t cpu_started;
extern pthread_mutex_t broadcast_mutex;
extern pthread_mutex_t io_signal_mutex;
int current_thread_count = 0;
int cpu_mutex_created = 0;
void reset_thread_count() {
    current_thread_count = 0;
}
void connect_ppi(ppi_8255 * ppi, Sim8085 * simulator, int chip_select) {
    ppi->mpu = simulator;
    ppi->chip_select = chip_select;
    ppi->control_register = 0;
    ppi->thread_id = current_thread_count++;
    if (!cpu_mutex_created) {
        cpu_mutex_created = 1;
        initialize_cpu_mutex();
    }
    pthread_create(&ppi_threads[current_thread_count], 0, ppi_thread_handler, (void*)ppi);
    current_thread_count++;
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
int group_a_mode(ppi_8255 * ppi) {
    return (ppi->control_register) >> 5;
}
int group_b_mode(ppi_8255 * ppi) {
    return (ppi->control_register >> 2) & 1;
}
void* ppi_thread_handler(void * t_info) { //each ppi device has its own thread
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
                        pinfo->mpu->data_latch = pinfo->portc;
                    }
                } else {//iow signal
                    if (addr == 0) { //read from port a
                        int mode = group_a_mode(pinfo);
                        if (mode == 0) pinfo->porta = pinfo->mpu->data_latch;
                    } else if (addr == 1) {
                        int mode = group_b_mode(pinfo);
                        if (mode == 0) pinfo->portb = pinfo->mpu->data_latch;
                    } else if (addr == 2) {
                        pinfo->portc = pinfo->mpu->data_latch;
                    }
                }
            }
            pthread_mutex_unlock(&io_signal_mutex);
        }
    }
    pthread_exit((void*)0);
}
void initialize_cpu_mutex() {
    pthread_mutex_init(&cpu_access_mutex, 0);
}
