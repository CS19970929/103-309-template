"""Exercise the production journal and engineering test controller on modeled Flash."""
import ast, os, re
from pathlib import Path
import run_flash_fit_host_test as base
base.OUT=Path(os.environ['LOCALAPPDATA'])/'CodexTemp/afe-reference-align/flash-endurance/host'
base.OUT.mkdir(parents=True,exist_ok=True)
node=next(n for n in ast.parse(Path(base.__file__).read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='flash_checks')
model=node.body[0].value.value
h=base.read('Flash.h');src=base.read('Flash.c')
code=model+'\n#define FLASH_ENDURANCE_TEST_ENABLE 1\n'
for name in ['FLASH_ADDR_STORAGE_START','FLASH_ADDR_STORAGE_END','FLASH_ADDR_DEVICE_END','FLASH_ADDR_STORAGE_SOC_SLOT_A','FLASH_ADDR_STORAGE_SOC_SLOT_B','FLASH_STORAGE_SOC_DATA_VERSION_CURRENT']:
    code+=base.macro(h,name)
code+=base.read('FlashEndurance.h')
code+=re.search(r'typedef struct\s*\{[^}]*\} STORAGE_FLASH_SOC_DATA;',h).group()+'\n'
code+='static UINT32 s_testEraseAttempt[8],s_testEraseSuccess[8],s_testProgramFailure,tick;\n'
code+='static UINT32 SysTime_Get10msTickCount(void){return tick;}\n#define FLASH_STORAGE_MAGIC_SOC 0x534F4331U\n'
code+=base.clean(src[src.index('typedef struct'):src.index('FLASH_Status FlashWriteOneHalfWord')])
for name in ['StorageFlash_LoadSocDataReal','StorageFlash_LoadSocData','StorageFlash_SaveSocDataReal','StorageFlash_SaveSocData']:
    code+=base.function(src,name)
code+=base.read('FlashEndurance.inc')
code+=r'''
int main(void) {
    unsigned i; UINT8 *flash; UINT8 status[192]; STORAGE_FLASH_SOC_DATA original,loaded;
    flash=VirtualAlloc((void*)0x08000000,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(flash==(UINT8*)0x08000000);memset(flash,255,65536);
    memset(&original,0,sizeof(original)); original.u16FormatVersion=3;original.u16SocNow=50;
    original.u32CapFull=100000;original.u32CapNow=50000;
    assert(sizeof(original)==40 && StorageFlash_SaveSocData(&original));
    assert(!FlashTest_Command(0x2700,123));
    assert(FlashTest_Command(0x2700,FLASH_TEST_START_POLICY));
    assert(!StorageFlash_SaveSocData(&original));
    for(i=0;i<1000;i++)assert(FlashTest_Command(0x2701,50));
    assert(s_testSkipped==1000 && s_testSaved==0);
    for(i=0;i<3000;i++)assert(FlashTest_Command(0x2701,i%101));
    assert(s_testSaved==3000 && s_testSubmitted==4000);
    assert(s_testEraseAttempt[4]>50 && s_testEraseAttempt[5]>50);
    assert(s_testEraseAttempt[4]==s_testEraseSuccess[4] && s_testEraseAttempt[5]==s_testEraseSuccess[5]);
    assert(!StorageFlash_LoadSocData(&loaded)); /* synthetic image cannot enter runtime */
    FlashTest_Read(status);assert(status[0]==0x46 && status[1]==0x54);
    assert(FlashTest_Command(0x2700,0));assert(StorageFlash_LoadSocData(&loaded));assert(!memcmp(&loaded,&original,40));
    assert(FlashTest_Command(0x2700,FLASH_TEST_START_FORCE));
    for(i=0;i<500;i++)assert(FlashTest_Command(0x2701,50));
    assert(s_testSaved==500 && s_testSkipped==0);
    tick=1001;FlashTest_Service();assert(!FlashTest_Active());assert(StorageFlash_LoadSocData(&loaded));assert(!memcmp(&loaded,&original,40));
    assert(FlashTest_Command(0x2700,FLASH_TEST_START_FORCE));
    fail_at=steps+1;assert(!FlashTest_Command(0x2701,1));assert(s_testFault && s_testFailed==1);
    assert(!FlashTest_Command(0x2701,2));fail_at=0;assert(FlashTest_Command(0x2700,0));
    assert(FlashTest_Restore((UINT16*)&original));
    original.u16SocNow=101;assert(!FlashTest_Restore((UINT16*)&original));original.u16SocNow=50;
    for(i=0;i<65536;i++)if(i<0xF000 || i>=0xF800)assert(flash[i]==255);
    puts("PASS: real SOC journal, 3000 changed/1000 repeated/500 forced saves; per-page erases, backup restore, timeout, fault latch, synthetic isolation, address boundaries");
    return 0;
}
'''
print(base.run('flash_endurance',code).decode())
