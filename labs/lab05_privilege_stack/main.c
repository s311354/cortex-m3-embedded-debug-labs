#include <stdint.h>
#include "device.h"

/*
 * MPU (Memory Protection Unit) access permissions
 *
 * AP[2:0]:
 *
 *   001 = Privileged RW, Unprivileged No Access
 *   011 = Privileged RW, Unprivileged RW
 *   110 = Privileged RO, Unprivileged RO
 */
#define MPU_AP_PRIV_RW_USER_NONE (1UL << MPU_RASR_AP_Pos)
#define MPU_AP_FULL_ACCESS       (3UL << MPU_RASR_AP_Pos)
#define MPU_AP_READ_ONLY         (6UL << MPU_RASR_AP_Pos)

/*
 * MPU SIZE field encoding
 *
 * 32B    -> SIZE = 4
 * 128B   -> SIZE = 16
 * 256B   -> SIZE = 17
 */
#define MPU_SIZE_32KB             (4UL  << MPU_RASR_SIZE_Pos)
#define MPU_SIZE_128KB            (16UL << MPU_RASR_SIZE_Pos)
#define MPU_SIZE_256KB            (17UL << MPU_RASR_SIZE_Pos)

/*
 * Use normal memory attributes
 * 
 * Cortex-M3 itself does not have an L1 data cache,
 * but the MPU still uses these fields to classify memory type
 */
#define MPU_NORMAL_MEMORY        (MPU_RASR_C_Msk | MPU_RASR_B_Msk)

/*
 * MPU debug state
 */
volatile uint32_t mpu_type;
volatile uint32_t mpu_ctrl_after_enable;

volatile uint32_t privileged_readback;
volatile uint32_t unprivileged_normal_readback;

volatile uint32_t fault_cfsr;
volatile uint32_t fault_mmfar;
volatile uint32_t fault_exc_return;
volatile uint32_t fault_stacked_pc;
volatile uint32_t fault_stacked_xpsr;
volatile uint32_t fault_control;

volatile uint32_t test_stage;

static volatile uint32_t normal_ram_word;

/*
 * Protected MPU test region
 *
 * Cortex MPU minimum region size is 32 bytes
 */
static volatile uint32_t protected_words[8] __attribute((aligned(32)));

/*
 * MPU
 */
static void mpu_configure_region(uint32_t region, uint32_t base, uint32_t attributes) {
    /*
     * Select MPU region
     */
    MPU->RNR = region;

    /*
     * Base address must satisfy region-size alignment
     */
    MPU->RBAR = base;

    /*
     * Permissions + memory type + size + enable
     */
    MPU->RASR = attributes | MPU_RASR_ENABLE_Msk;
}

static void MPU_Init(void) {
    /*
     * Read MPU capability
     */
    mpu_type = MPU->TYPE;

    /*
     * Disable MPU while changing its region configuration
     */
    MPU->CTRL = 0U;

    /*
     * Region 0: FLASH
     *
     * 0x00000000 - 0x0003FFFF (256 KB)
     *
     * Privileged   : Read-only + executable
     * Unprivileged : Read-only + executable
     */
    mpu_configure_region(
		    0U,
		    CM3DS_MPS2_FLASH_BASE,
		    MPU_AP_READ_ONLY | MPU_NORMAL_MEMORY | MPU_SIZE_256KB);

    /*
     * Region 1: SRAM
     * 
     * 0x20000000 - 0x2001FFFF (128 KB)
     *
     * Privileged   : RW
     * Unprivileged : RW
     * Execute      : Never
     */
    mpu_configure_region(
		    1U,
		    CM3DS_MPS2_SRAM_BASE,
		    MPU_RASR_XN_Msk | MPU_AP_FULL_ACCESS | MPU_NORMAL_MEMORY | MPU_SIZE_128KB);

    /*
     * Region 2: protected_words[]
     *
     * Higher-numbered MPU regions have priority when regions overlap
     *
     * Privileged   : RW
     * Unprivileged : No Access
     * Execute      : Never
     */
    mpu_configure_region(
		    2U,
		    (uint32_t) protected_words,
		    MPU_RASR_XN_Msk | MPU_AP_PRIV_RW_USER_NONE | MPU_NORMAL_MEMORY | MPU_SIZE_32KB);

    /*
     * Enbale MemManage faults (System Control Block)
     */
    SCB->SHCSR = SCB_SHCSR_MEMFAULTENA_Msk;

    /*
     * Enable MPU
     */
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;

    /*
     * MPU configuration is an architectural-state transition
     */
    __DSB(); // complete preceding explicit memory effects before continuing
    __ISB(); // flush/refetch the instruction stream

    mpu_ctrl_after_enable = MPU->CTRL;
}


/*
 * Existing Lab05 debug state
 */
volatile uint32_t dump[8];
volatile uint32_t exc_return;

volatile uint32_t control_before;
volatile uint32_t control_after;

volatile uint32_t msp_before;
volatile uint32_t psp_before;

volatile uint32_t msp_after;
volatile uint32_t psp_after;

volatile uint32_t debug_control;
volatile uint32_t debug_psp;
volatile uint32_t debug_msp;
volatile uint32_t debug_ipsr;

static void DumpSpecialRegisters(void) {
    __asm volatile(
        "mrs %0, control"
        : "=r"(debug_control)	
    );

    __asm volatile(
        "mrs %0, psp"
        : "=r"(debug_psp)	
    );

    __asm volatile(
        "mrs %0, msp"
        : "=r"(debug_msp)	
    );

    __asm volatile(
        "mrs %0, ipsr"
	: "=r"(debug_ipsr)
    );
}

/* Process Stack */
static uint32_t process_stack[64];

static void Trigger_SVC(void) {
    register uint32_t r0 asm("r0") = 0x11111111;
    register uint32_t r1 asm("r1") = 0x22222222;
    register uint32_t r2 asm("r2") = 0x33333333;
    register uint32_t r3 asm("r3") = 0x44444444;

    asm volatile("" : : "r"(r0), "r"(r1), "r"(r2), "r"(r3));

    __asm volatile ("svc #0");
}


/*
 * MemManage fault
 */
void MemManage_Handler_C(uint32_t *stack, uint32_t lr) __attribute__((noreturn));

__attribute__((naked))
void MemManage_Handler(void) {
    /*
     * EXC_RETURN bit 2
     */
    __asm volatile (
		    "tst   lr, #4        \n"
		    "ite   eq            \n"
		    "mrseq r0, msp       \n"
		    "mrsne r0, psp       \n"
		    "mov   r1, lr        \n"
		    "b     MemManage_Handler_C \n"
		    );
}

void MemManage_Handler_C(uint32_t *stack, uint32_t lr) {
    /*
     * Snapshot fault information for GDB
     */
    fault_exc_return = lr;

    fault_cfsr = SCB->CFSR;
    fault_mmfar = SCB->MMFAR;

    fault_stacked_pc = stack[6];
    fault_stacked_xpsr = stack[7];

    fault_control = __get_CONTROL();

    test_stage = 0xDEADU;

    while (1) {
        __NOP();
    }
}

__attribute__((naked))
void SVC_Handler(void) {
    __asm volatile(
        "tst lr,#4            \n"
	"ite eq               \n"
	"mrseq r0,msp         \n"
	"mrsne r0,psp         \n"
	"mov r1, lr           \n"
	"b SVC_Handler_C     \n"
	"bx lr                \n"
    );
}

void SVC_Handler_C(uint32_t *stack, uint32_t lr) {

    exc_return = lr;

    for (int i = 0; i < 8; ++i)
        dump[i] = stack[i];
}

int main(void) {
    uint32_t control;

    test_stage = 1U;

    /* ---- Initial State ----*/
    control_before = __get_CONTROL();
    msp_before = __get_MSP();
    psp_before = __get_PSP();

    DumpSpecialRegisters();

    /* ---- Configure PSP ----*/
    __set_PSP((uint32_t)&process_stack[64]);


    /*
     * Configure + enable MPU while privileged
     */
    MPU_Init();

    test_stage = 2U;

    protected_words[0] = 0x12345678U;
    privileged_readback = protected_words[0];
    
    /*
     * Normal RAM test
     */
    normal_ram_word = 0xA5A55A5AU;


    control = __get_CONTROL();

    /* SPSEL = 1 (MSP -> PSP)
     * nPRIV = 1 (Privileged -> Unprivileged)
     * */
    control |= 0x3;
    __set_CONTROL(control);
    __ISB();

    test_stage = 3U;

    control_after = __get_CONTROL();
    msp_after = __get_MSP();
    psp_after = __get_PSP();

    DumpSpecialRegisters();

    /*
     * Test #1: Unprivileged access to ordinary Region-1 SRAM
     */
    unprivileged_normal_readback = normal_ram_word;

    test_stage = 4U;

    /*
     * Test #2: SVC from unprivileged Thread mode
     *
     * Expection stack frame: saved on PSP
     */
    Trigger_SVC();

    test_stage = 5U;

    /*
     * Test #3: Deliberate MPU violation
     *
     * Expected: protected_words[0] write -> MPU permission violation -> MemManage exception
     */
    protected_words[0] = 0xBAD0BAD0U;

    test_stage = 6U;

    while (1) {
    }
}
