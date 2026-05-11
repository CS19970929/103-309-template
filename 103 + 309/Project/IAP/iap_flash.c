#include "iap_flash.h"
#include "iap_config.h"

#include "stm32f10x.h"
#include "stm32f10x_flash.h"

typedef void (*iap_app_entry_t)(void);

static uint16_t s_flag_page_buffer[IAP_FLASH_PAGE_SIZE / 2U];

static __asm void IapFlash_SetMsp(uint32_t stack_pointer)
{
    MSR MSP, r0
    BX lr
}

static uint8_t IapFlash_IsAddressInApp(uint32_t address, uint32_t length)
{
    if (length == 0U)
    {
        return 1U;
    }
    if (address < IAP_APP_BASE)
    {
        return 0U;
    }
    if (address > IAP_APP_MAX_END_EXCLUSIVE)
    {
        return 0U;
    }
    if (length > (IAP_APP_MAX_END_EXCLUSIVE - address))
    {
        return 0U;
    }
    return 1U;
}

static void IapFlash_ClearFlags(void)
{
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

uint16_t IapFlash_ReadHalfWord(uint32_t address)
{
    return *(__IO uint16_t *)address;
}

uint8_t IapFlash_IsImageSizeValid(uint32_t image_size)
{
    if (image_size == 0U)
    {
        return 0U;
    }
    return IapFlash_IsAddressInApp(IAP_APP_BASE, image_size);
}

uint8_t IapFlash_IsAppVectorValid(void)
{
    uint32_t stack_pointer;
    uint32_t reset_handler;

    stack_pointer = *(__IO uint32_t *)IAP_APP_BASE;
    reset_handler = *(__IO uint32_t *)(IAP_APP_BASE + 4U);
    reset_handler &= 0xFFFFFFFEU;

    if ((stack_pointer & 0x2FFE0000U) != 0x20000000U)
    {
        return 0U;
    }
    if (reset_handler < IAP_APP_BASE)
    {
        return 0U;
    }
    if (reset_handler >= IAP_APP_MAX_END_EXCLUSIVE)
    {
        return 0U;
    }
    return 1U;
}

uint8_t IapFlash_BeginUpgrade(uint32_t image_size)
{
    uint32_t erase_addr;
    uint32_t erase_end;
    FLASH_Status status;

    if (IapFlash_IsImageSizeValid(image_size) == 0U)
    {
        return 0U;
    }

    erase_end = IAP_APP_BASE + image_size;
    erase_end = (erase_end + IAP_FLASH_PAGE_SIZE - 1U) & ~(IAP_FLASH_PAGE_SIZE - 1U);
    if (erase_end > IAP_APP_MAX_END_EXCLUSIVE)
    {
        return 0U;
    }

    FLASH_Unlock();
    IapFlash_ClearFlags();

    erase_addr = IAP_APP_BASE;
    while (erase_addr < erase_end)
    {
        if (erase_addr < IAP_BOOT_MAX_END_EXCLUSIVE)
        {
            FLASH_Lock();
            return 0U;
        }
        status = FLASH_ErasePage(erase_addr);
        if (status != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return 0U;
        }
        erase_addr += IAP_FLASH_PAGE_SIZE;
    }

    return 1U;
}

uint8_t IapFlash_WriteBytes(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    uint16_t halfword;
    FLASH_Status status;

    if (IapFlash_IsAddressInApp(address, length) == 0U)
    {
        return 0U;
    }
    if ((address & 0x00000001U) != 0U)
    {
        return 0U;
    }

    offset = 0U;
    while (offset < length)
    {
        halfword = data[offset];
        if ((offset + 1U) < length)
        {
            halfword |= (uint16_t)((uint16_t)data[offset + 1U] << 8U);
        }
        else
        {
            halfword |= 0xFF00U;
        }

        IapFlash_ClearFlags();
        status = FLASH_ProgramHalfWord(address + offset, halfword);
        if (status != FLASH_COMPLETE)
        {
            return 0U;
        }
        if (*(__IO uint16_t *)(address + offset) != halfword)
        {
            return 0U;
        }
        offset += 2U;
    }

    return 1U;
}

void IapFlash_EndUpgrade(void)
{
    FLASH_Lock();
}

uint8_t IapFlash_ClearUpdateFlagPreservePage(void)
{
    uint32_t page_addr;
    uint32_t flag_offset;
    uint32_t word_count;
    uint32_t index;
    FLASH_Status status;

    if (IapFlash_ReadHalfWord(IAP_UPDATE_FLAG_ADDR) == IAP_FLASH_TO_APP_VALUE)
    {
        return 1U;
    }

    page_addr = IAP_UPDATE_FLAG_ADDR & ~(IAP_FLASH_PAGE_SIZE - 1U);
    flag_offset = (IAP_UPDATE_FLAG_ADDR - page_addr) / 2U;
    word_count = IAP_FLASH_PAGE_SIZE / 2U;

    for (index = 0U; index < word_count; index++)
    {
        s_flag_page_buffer[index] = *(__IO uint16_t *)(page_addr + (index * 2U));
    }
    s_flag_page_buffer[flag_offset] = IAP_FLASH_TO_APP_VALUE;

    FLASH_Unlock();
    IapFlash_ClearFlags();
    status = FLASH_ErasePage(page_addr);
    if (status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return 0U;
    }

    for (index = 0U; index < word_count; index++)
    {
        if (s_flag_page_buffer[index] != 0xFFFFU)
        {
            IapFlash_ClearFlags();
            status = FLASH_ProgramHalfWord(page_addr + (index * 2U), s_flag_page_buffer[index]);
            if (status != FLASH_COMPLETE)
            {
                FLASH_Lock();
                return 0U;
            }
        }
    }
    FLASH_Lock();

    return (IapFlash_ReadHalfWord(IAP_UPDATE_FLAG_ADDR) == IAP_FLASH_TO_APP_VALUE) ? 1U : 0U;
}

void IapFlash_JumpToApp(void)
{
    uint32_t stack_pointer;
    uint32_t jump_address;
    iap_app_entry_t app_entry;

    if (IapFlash_IsAppVectorValid() == 0U)
    {
        return;
    }

    stack_pointer = *(__IO uint32_t *)IAP_APP_BASE;
    jump_address = *(__IO uint32_t *)(IAP_APP_BASE + 4U);
    app_entry = (iap_app_entry_t)jump_address;

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->VTOR = IAP_APP_BASE;
    IapFlash_SetMsp(stack_pointer);
    __enable_irq();
    app_entry();
}
