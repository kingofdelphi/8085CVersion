#include "8255ppi.h"
pthread_t ppi_threads[MAX_PPI_CONNECTIONS];
pthread_mutex_t cpu_access_mutex;
extern int program_loaded;
extern pthread_cond_t cpu_started;
extern pthread_mutex_t broadcast_mutex;
int current_thread_count = 0;
int cpu_mutex_created = 0;
void reset_thread_count() {
    current_thread_count = 0;
}
void connect_ppi(ppi_8255 * ppi, Sim8085 * simulator, int base_address) {
    ppi->mpu = simulator;
    ppi->base_address = base_address;
    ppi->control_register_latch = ppi->mpu->IOPORTS[base_address + 3]; //ppi has not been used
    ppi->thread_id = current_thread_count++;
    if (!cpu_mutex_created) {
        cpu_mutex_created = 1;
        initialize_cpu_mutex();
    }
    pthread_create(&ppi_threads[current_thread_count], 0, ppi_thread_handler, (void*)ppi);
    current_thread_count++;
}

int get_ppi_port_value(ppi_8255 * ppi, int port_no) {
    return ppi->mpu->IOPORTS[ppi->base_address + port_no];
}
int is_bsr(int mode_info) {
    return (mode_info >> 7) == 0;
}
void* ppi_thread_handler(void * t_info) { //each ppi device has its own thread
    ppi_8255 * pinfo = (ppi_8255*)t_info;
    while (1) {
        pthread_mutex_lock(&broadcast_mutex);
        //printf("cpu not started\n");
        pthread_cond_wait(&cpu_started, &broadcast_mutex);
        pthread_mutex_unlock(&broadcast_mutex);
        //printf("cpu started\n");
        pinfo->control_register_latch = pinfo->mpu->IOPORTS[pinfo->base_address + 3]; //ppi has not been used
        //todo: donot try to access cpu without a mutex
        while (program_loaded && !hasHalted(pinfo->mpu)) {//currently this is inefficient
            ppi_logic(pinfo);
        }
    }
    pthread_exit((void*)0);
}
void ppi_logic(ppi_8255 * ppi) {
    int crval = get_ppi_port_value(ppi, 3);
    //check if control register has changed
    //if changed, it means this ppi has to update
    if (crval != ppi->control_register_latch) {//newer information was written on control register address
        if (is_bsr(crval)) {
            pthread_mutex_lock(&cpu_access_mutex);
            int * portc = ppi->mpu->IOPORTS + ppi->base_address + 2;
            int bitpos = (crval >> 1) & 0x7;
            *portc = (*portc) & (~(1 << bitpos)); 
            *portc = (*portc) | ((crval & 1) << bitpos);
            pthread_mutex_unlock(&cpu_access_mutex);
        }
        ppi->control_register_latch = crval;
    }
}
void initialize_cpu_mutex() {
    pthread_mutex_init(&cpu_access_mutex, 0);
}
