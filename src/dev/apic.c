#include <dev/apic.h>
#include <cpuid.h>
#include <msr.h>


int 
check_apic_avail (void)
{
    cpuid_ret_t cp;
    struct cpuid_feature_flags * flags;

    cp = cpuid(CPUID_FEATURE_INFO);
    flags = (struct cpuid_feature_flags *)&cp.c;
    return flags->edx.apic;
}


static void 
apic_enable (void) 
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);
    msr_write(IA32_APIC_BASE_MSR, data | APIC_GLOBAL_ENABLE);
}


static addr_t 
apic_get_base_addr (void) 
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);

    // we're assuming PAE is on
    return (addr_t)(data & APIC_BASE_ADDR_MASK);
}


static void
apic_set_base_addr (struct apic_dev * apic, addr_t addr)
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);
    
    msr_write(IA32_APIC_BASE_MSR, (addr & APIC_BASE_ADDR_MASK) | (data & 0xfff));
}


void 
apic_do_eoi (struct apic_dev * apic)
{
    /* TODO: fast eoi */
    apic_write(apic, APIC_REG_EOR, 0);
}


uint32_t
apic_get_id (struct apic_dev * apic)
{
    return apic_read(apic, APIC_REG_ID) >> APIC_ID_SHIFT;
}


void 
apic_ipi (struct apic_dev * apic, 
          uint_t remote_id,
          uint_t vector)
{
    apic_write(apic, APIC_REG_ICR2, remote_id << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, vector | APIC_ICR_LEVEL_ASSERT);
}


void
apic_init (struct apic_dev * apic)
{
    apic->base_addr = apic_get_base_addr();
}

