#include <stdint.h>

#define PAGE_SIZE 4096
#define IMAGE_BASE UINT64_C(0x40200000)
#define TABLE_ENTRIES 512U
#define BLOCK_SIZE (UINT64_C(512) * PAGE_SIZE)

/* Stage-1 page descriptors: AP[2:1] in bits [7:6] */
#define MMU_AP_PRIV_RW_USER_NONE  (UINT64_C(0) << 6)
#define MMU_AP_FULL_ACCESS        (UINT64_C(1) << 6)
#define MMU_AP_READ_ONLY          (UINT64_C(3) << 6)

#define DESC_PAGE                 UINT64_C(3)
#define DESC_AF                   (UINT64_C(1) << 10)
#define DESC_SH_INNER             (UINT64_C(3) << 8)
#define DESC_PXN                  (UINT64_C(1) << 53)
#define DESC_UXN                  (UINT64_C(1) << 54)

#define SCTLR_M                   UINT64_C(1)
#define SCTLR_WXN                 (UINT64_C(1) << 19)

#define ESR_EC(v)                 (((v) >> 26) & 0x3fU)

#define READ_SYSREG(reg)          ({            \
		uint64_t v;                     \
		__asm__ volatile ("mrs %0,"     \
				   #reg         \
				   : "=r"(v));  \
		v;                              \
		})
#define WRITE_SYSREG(reg, v)     __asm__ volatile("msr " #reg ", %0" :: "r"((uint64_t)(v)) : "memory")      

#define ISB() __asm__ volatile("isb" ::: "memory")

#define ADDR(p) ((uint64_t)(uintptr_t)(p))

extern char __exception_vectors[], el0_code_start[], el0_code_end[];
extern char __stack_top[], svc_resume[], fault_trigger[];
extern void enter_el0(void (*entry)(void), uint64_t stack_top) __attribute__((noreturn));

/* EL0 data, process stack and protected region each occupy one page */
union user_page {
    struct {
        uint32_t test_stage;
	uint32_t normal_ram_word;
	uint32_t unprivileged_normal_readback;
	uint64_t user_sp_before, user_sp_after;
    };
    uint8_t page[PAGE_SIZE];
};

volatile union user_page user_state __attribute__((aligned(PAGE_SIZE)));
static uint8_t process_stack[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
volatile uint32_t protected_words[PAGE_SIZE / sizeof(uint32_t)] __attribute((aligned(PAGE_SIZE)));

static uint64_t l1[TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static uint64_t l2[TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static uint64_t l3[TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

/* Debug state for GDB */
volatile uint32_t g_result, g_svc_count, privileged_readback;
volatile uint64_t g_current_el, g_spsel, g_sctlr_el1;
volatile uint64_t el1_sp_before, svc_el1_sp, svc_el0_sp;
volatile uint64_t fault_esr_el1, fault_far_el1, fault_elr_el1, fault_spsr_el1;
volatile uint64_t exc_return, g_esr_el1, g_spsr_el1;
volatile uint32_t dump[4];

__attribute__((noinline, noreturn))
void lab_halt(void) {
    while (1) {
        __asm__ volatile ("wfe");
    }
}

static void fail(void) __attribute__((noreturn));
static void fail(void) {
    g_result = 2U;
    lab_halt();
}

static void mmu_configure_page(uint64_t address, uint64_t permissions) {
    if (address < IMAGE_BASE || address >= IMAGE_BASE + BLOCK_SIZE || 
	(address & (PAGE_SIZE - 1U)) != 0U) {
        fail();
    }

    l3[(address - IMAGE_BASE) / PAGE_SIZE] = address | DESC_PAGE | DESC_AF | DESC_SH_INNER | permissions;
}

static void MMU_Init(void) {
    /* Identity-map only the 2MiB image region */
    if (ADDR(__stack_top) > IMAGE_BASE + BLOCK_SIZE || 
	ADDR(el0_code_end) - ADDR(el0_code_start) != PAGE_SIZE ||
        (READ_SYSREG(SCTLR_EL1) & SCTLR_M) != 0U) {
        fail();
    }

    for (uint32_t i = 0; i < TABLE_ENTRIES; ++i) {
        l1[i] = 0U;
	l2[i] = 0U;
	mmu_configure_page(IMAGE_BASE + (uint64_t)i * PAGE_SIZE, MMU_AP_PRIV_RW_USER_NONE | DESC_UXN);
    }

    l1[1] = ADDR(l2) | DESC_PAGE;
    l2[1] = ADDR(l3) | DESC_PAGE;

    /* Region 0: EL0 code, read-only and executable */
    mmu_configure_page(ADDR(el0_code_start), MMU_AP_READ_ONLY | DESC_PXN);

    /* Region 1: ordinary RAM and process stack, EL0 RW, execute-never */
    mmu_configure_page(ADDR(&user_state), MMU_AP_FULL_ACCESS | DESC_PXN | DESC_UXN);
    mmu_configure_page(ADDR(process_stack), MMU_AP_FULL_ACCESS | DESC_PXN | DESC_UXN);

    /* Region 2: protected words, EL1 RW, EL0 no access */
    mmu_configure_page(ADDR(protected_words), MMU_AP_PRIV_RW_USER_NONE | DESC_PXN | DESC_UXN);

    /* Normal WB memory */
    WRITE_SYSREG(MAIR_EL1, 0xffU);
    WRITE_SYSREG(TCR_EL1, (UINT64_C(25) | (UINT64_C(1) << 8) |
		          (UINT64_C(1) << 10) | (UINT64_C(3) << 12)) |
	                  (UINT64_C(25) << 16 | (UINT64_C(1) << 23) |
			  (UINT64_C(2) << 30)));
    WRITE_SYSREG(TTBR0_EL1, ADDR(l1));
    WRITE_SYSREG(TTBR1_EL1, 0U);

    __asm__ volatile("dsb sy\n\tisb\n\ttlbi vmalle1\n\tdsb sy\n\tisb" ::: "memory");

    WRITE_SYSREG(SCTLR_EL1, (READ_SYSREG(SCTLR_EL1) & ~SCTLR_WXN) | SCTLR_M);

    ISB();

    g_sctlr_el1 = READ_SYSREG(SCTLR_EL1);
}

static void DumpSpecialRegisters(void) {
    g_current_el = (READ_SYSREG(CurrentEL) >> 2) & 3U;
    g_spsel = READ_SYSREG(SPSel) & 1U;
}

static inline __attribute__((always_inline)) void Trigger_SVC(void) {
    __asm__ volatile(
		"mov x0, #0x1111\nmovk x0, #0x1111, lsl #16\n"
		"mov x1, #0x2222\nmovk x1, #0x2222, lsl #16\n"
		"mov x2, #0x3333\nmovk x2, #0x3333, lsl #16\n"
		"mov x3, #0x4444\nmovk x3, #0x4444, lsl #16\n"
		".global svc_trigger\nsvc_trigger: svc #0\n"
		".global svc_resume\nsvc_resume:\n"
		::: "x0", "x1", "x2", "x3", "memory", "cc"
		    );
}

/* Called with the software-saved frame on SP_EL1 */
void SVC_Handler(const uint64_t *stack) {
    exc_return = READ_SYSREG(ELR_EL1);
    g_esr_el1 = READ_SYSREG(ESR_EL1);
    g_spsr_el1 = READ_SYSREG(SPSR_EL1);
    svc_el0_sp = READ_SYSREG(SP_EL0);
    svc_el1_sp = ADDR(stack) + 256U;

    if (user_state.test_stage != 4U || (g_esr_el1 & 0xffffU) != 0U) {
        fail();
    }

    for (uint32_t i = 0; i < 4U; ++i) {
        dump[i] = stack[i];
    }

    ++g_svc_count;
}

/* Expected terminal fault */
__attribute__((noreturn))
void Sync_Fault_Handler(const uint64_t *stack) {
    fault_esr_el1 = READ_SYSREG(ESR_EL1);
    fault_far_el1 = READ_SYSREG(FAR_EL1);
    fault_elr_el1 = READ_SYSREG(ELR_EL1);
    fault_spsr_el1 = READ_SYSREG(SPSR_EL1);

    g_result = (user_state.test_stage == 5U && g_svc_count == 1U &&
		    privileged_readback == 0x12345678U && protected_words[0] == 0x12345678U &&
		    ESR_EC(g_esr_el1) == 0x15U && (g_spsr_el1 & 0xfU) == 0U &&
		    exc_return == ADDR(svc_resume) &&
		    svc_el0_sp == user_state.user_sp_before &&
		    user_state.user_sp_before == user_state.user_sp_after &&
		    svc_el1_sp == el1_sp_before && ADDR(stack) + 256U == el1_sp_before &&
		    dump[0] == 0x11111111U && dump[1] == 0x22222222U &&
		    dump[2] == 0x33333333U && dump[3] == 0x44444444U &&
		    ESR_EC(fault_esr_el1) == 0x24U &&
		    (fault_esr_el1 & 0x47fU) == 0x4fU &&
		    fault_far_el1 == ADDR(protected_words) &&
		    fault_elr_el1 == ADDR(fault_trigger) &&
		    (fault_spsr_el1 & 0xfU) == 0U) ? 1U : 2U;

    user_state.test_stage = 0xDEADU;
    lab_halt();
}

__attribute__((noreturn))
void Default_Handler(void) {
    fail();
}

/* Continue on SP_EL0 after ERET */
__attribute__((section(".el0_text"), noinline, noreturn))
void User_Main(void) {
    user_state.test_stage = 3U;

    /* Test 1. oridinary RAM access */
    user_state.unprivileged_normal_readback = user_state.normal_ram_word;
    user_state.test_stage = 4U;

    /* Test 2. SVC from EL0 */
    uint64_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    user_state.user_sp_before = sp;

    Trigger_SVC();

    __asm__ volatile("mov %0, sp" : "=r"(sp));
    user_state.user_sp_after = sp;
    user_state.test_stage = 5U;

    /* Test 3. deliberate permission fault */
    __asm__ volatile(".global fault_trigger\nfault_trigger: str %w1, [%0]"
		    :: "r"(&protected_words[0]), "r"(UINT32_C(0xBAD0BAD0)) : "memory");
    user_state.test_stage = 6U;

    __asm__ volatile("svc #1" ::: "memory");
    while (1) {
    }
}

int main(void) {
    g_result = 0U;
    g_svc_count = 0U;
    user_state.test_stage = 1U;

    /* Initial state */
    DumpSpecialRegisters();

    if (g_current_el != 1U || g_spsel != 1U) {
        fail();
    }

    __asm__ volatile("msr daifset, #0xf" ::: "memory");
    WRITE_SYSREG(VBAR_EL1, ADDR(__exception_vectors));
    ISB();

    /* Configure process stack and MMU */
    uint64_t stack_top = ADDR(process_stack) + PAGE_SIZE;
    WRITE_SYSREG(SP_EL0, stack_top);
    MMU_Init();

    user_state.test_stage = 2U;

    protected_words[0] = 0x12345678U;
    privileged_readback = protected_words[0];
    user_state.normal_ram_word = 0xA5A55A5AU;

    /* EL1h -> EL0t */
    uint64_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    el1_sp_before = sp;
    enter_el0(User_Main, stack_top);
}	
