/*
  Copyright notice
  ================
  
  Copyright (C) 2010
      Lorenzo  Martignoni <martignlo@gmail.com>
      Roberto  Paleari    <roberto.paleari@gmail.com>
      Aristide Fattori    <joystick@security.dico.unimi.it>
  
  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.
  
  HyperDbg is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License along with
  this program. If not, see <http://www.gnu.org/licenses/>.
  
*/

#ifdef GUEST_LINUX
#include <linux/kernel.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/io.h> // <------- virt_to_phys
#endif

#include "common.h"
#include "debug.h"
#include "mmu.h"
#include "x86.h"
#include "vmmstring.h"

/* ################ */
/* #### MACROS #### */
/* ################ */

#define MmuPrint(fmt, ...) //GuestLog(fmt, ## __VA_ARGS__)

/* This is the virtual base address for the page directory of the current
   process */
#ifdef ENABLE_PAE
#define VIRTUAL_PD_BASE     0xC0600000
#else
#define VIRTUAL_PD_BASE     0xC0300000 
#endif

/* Common to both PAE and non-PAE systems */
#define VIRTUAL_PT_BASE     0xC0000000 

static void *hostpt = NULL;

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static hvm_status MmuFindUnusedPTE(hvm_address* pdwLogical);
static hvm_status MmuGetPageEntry(hvm_address cr3, hvm_address va, PPTE ppte, hvm_bool* pisLargePage);
static hvm_bool   MmuGetCr0WP(void);

#if 0
static void       MmuPrintPDEntry(PPTE pde);
static void       MmuPrintPTEntry(PPTE pte);
static hvm_status MmuFindUnusedPDE(hvm_address* pdwLogical);
#endif
  
/* ################ */
/* #### BODIES #### */
/* ################ */

/* Initialize the MMU. Allocate the page table for the host, and define the CR3
   value that will be used when the CPU is executing in root mode */
hvm_status MmuInit(hvm_address *pcr3)
{
  hvm_status r;
  hvm_phy_address phy;
  hvm_address cr3;
#ifdef GUEST_LINUX
  mempool_t* pool;
#endif

  cr3 = RegGetCr3();

#ifdef GUEST_WINDOWS
  hostpt = ExAllocatePoolWithTag(NonPagedPool, MMU_PAGE_SIZE, 'gbdh');
#elif defined GUEST_LINUX
  pool = mempool_create_kmalloc_pool(1,MMU_PAGE_SIZE); 
  hostpt = mempool_alloc(pool, GFP_KERNEL);
#endif
  
  if (!hostpt) {
    GuestLog("[mmu-init] Failed to allocate non-paged pool for page table");
    return HVM_STATUS_UNSUCCESSFUL;
  }
  
  r = MmuGetPhysicalAddress(cr3, (hvm_address) hostpt, &phy);
  
  if(!HVM_SUCCESS(r)) {
    GuestLog("[mmu-init] Failed to get physical address for page table at 0x%.8x (CR3: 0x%.8x))", (hvm_address) hostpt, cr3);
    goto error;
  }

  *pcr3 = (hvm_address) phy;
  
  /* Initialize the page table */
#ifdef ENABLE_PAE
  /* In PAE mode, CR3 points to a set of 4 64 bit PDPTE's */
  r = MmuReadPhysicalRegion(CR3_ALIGN(cr3), hostpt, 4 * 16);
#else
  r = MmuReadPhysicalRegion(CR3_ALIGN(cr3), hostpt, MMU_PAGE_SIZE);
#endif /* ENABLE_PAE */

  if (!HVM_SUCCESS(r)) {
    GuestLog("[mmu-init] Failed to read page table at 0x%.8x (CR3: 0x%.8x)", (hvm_address) hostpt, cr3);
    goto error;
  }
  
  return HVM_STATUS_SUCCESS;
  
 error:
  if (hostpt) {

#ifdef GUEST_WINDOWS
    ExFreePoolWithTag(hostpt, 'gbdh');
#elif defined GUEST_LINUX
    mempool_destroy(pool);
#endif

    hostpt = NULL;
  }
  return HVM_STATUS_UNSUCCESSFUL;
}



/* Finalize the MMU. Deallocates the page table used by the CPU when running in
   root mode. */
hvm_status MmuFini(void)
{
  if (hostpt) {
#ifdef GUEST_WINDOWS
    ExFreePoolWithTag(hostpt, 'gbdh');
#elif defined GUEST_LINUX
    mempool_destroy(hostpt);    
#endif
    hostpt = NULL;
  }
  
  return HVM_STATUS_SUCCESS;
}

hvm_bool MmuIsAddressWritable(hvm_address cr3, hvm_address va)
{
  hvm_status r;
  PTE p;
  hvm_bool isLarge;

  if (!MmuIsAddressValid(CR3_ALIGN(cr3), va)) {
    return FALSE;
  }

  /* Data may be written to any linear address with a valid translation */
  if(!MmuGetCr0WP()) { 
    MmuPrint("[MMU] CR0.WP == 0\n");
    return TRUE;
  }

  r = MmuGetPageEntry(CR3_ALIGN(cr3), va, &p, &isLarge);
  if (r != HVM_STATUS_SUCCESS) {
    return FALSE;
  }

  return (p.Writable != 0);
}

hvm_bool MmuIsAddressValid(hvm_address cr3, hvm_address va)
{
  hvm_status r;
  hvm_bool isLarge;

  r = MmuGetPageEntry(CR3_ALIGN(cr3), va, NULL, &isLarge);

  return (r == HVM_STATUS_SUCCESS);
}

hvm_status MmuGetPhysicalAddress(hvm_address cr3, hvm_address va, hvm_phy_address* pphy)
{
  hvm_status r;
  PTE pte;
  hvm_bool isLarge;

  r = MmuGetPageEntry(CR3_ALIGN(cr3), va, &pte, &isLarge); 
    
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  if (isLarge) {
    *pphy = LARGEFRAME_TO_PHY(pte.PageBaseAddr) + LARGEPAGE_OFFSET(va);
    MmuPrint("[MMU] MmuGetPhysicalAddress(LARGE) cr3: %.8x frame: %.8x va: %.8x phy: %.8x\n", 
	     CR3_ALIGN(cr3), pte.PageBaseAddr, va, *pphy);
    return HVM_STATUS_SUCCESS;
  }

  *pphy = FRAME_TO_PHY(pte.PageBaseAddr) + MMU_PAGE_OFFSET(va);
  
  return HVM_STATUS_SUCCESS;
}

hvm_status MmuMapPhysicalPage(hvm_phy_address phy, hvm_address* pva, PPTE pentryOriginal)
{
  hvm_status r;
  hvm_address dwEntryAddress, dwLogicalAddress;
  PTE *pentry;
  
  /* Get unused PTE address in the current process */
  MmuPrint("[MMU] MmuMapPhysicalPage() Searching for unused PTE...\n");
  r = MmuFindUnusedPTE(&dwLogicalAddress);
  MmuPrint("[MMU] MmuMapPhysicalPage() Unused PTE found at %.8x\n", dwLogicalAddress);
  
  if (r != HVM_STATUS_SUCCESS)
    return HVM_STATUS_UNSUCCESSFUL;
  
#ifdef GUEST_WINDOWS
  dwEntryAddress = VIRTUAL_PT_BASE + (VA_TO_PTE(dwLogicalAddress) * sizeof(PTE));
#elif defined GUEST_LINUX 
  /* DIRTY HACK: we call this again to avoid refactoring of FindUnusedPTE */
  MmuPrint("dwLogicalAddress: %08x", dwLogicalAddress);
  MmuVirtToPTE(dwLogicalAddress, &dwEntryAddress);
  MmuPrint("PTE@%08x", dwEntryAddress);
#endif

  pentry = (PPTE) dwEntryAddress;

  /* Save original PT entry */
  *pentryOriginal = *pentry;

  /* Replace PT entry */
  pentry->Present         = 1;
  pentry->Writable        = 1;
  pentry->Owner           = 1;
  pentry->WriteThrough    = 0;
  pentry->CacheDisable    = 0;
  pentry->Accessed        = 0;
  pentry->Dirty           = 0;
  pentry->LargePage       = 0;
  pentry->Global          = 0;
  pentry->ForUse1         = 0;
  pentry->ForUse2         = 0;
  pentry->ForUse3         = 0;
  pentry->PageBaseAddr    = PHY_TO_FRAME(phy);

  hvm_x86_ops.mmu_tlb_flush();

  *pva = dwLogicalAddress;

  return HVM_STATUS_SUCCESS;
}

hvm_status MmuUnmapPhysicalPage(hvm_address va, PTE entryOriginal)
{
  PPTE pentry;

  /* Restore original PTE */
#ifdef GUEST_WINDOWS
  // FIXME: check if this also works on Linux
  pentry = (PPTE) (VIRTUAL_PT_BASE + (VA_TO_PTE(va) * sizeof(PTE)));
#elif defined GUEST_LINUX
  MmuVirtToPTE(va, (hvm_address*) &pentry);
#endif

  *pentry = entryOriginal;

  hvm_x86_ops.mmu_tlb_flush();
  
  return HVM_STATUS_SUCCESS;
}

hvm_status MmuReadWritePhysicalRegion(hvm_phy_address phy, void* buffer, Bit32u size, hvm_bool isWrite)
{
  hvm_status r;
  hvm_address dwLogicalAddress;
  PTE entryOriginal;

  /* Check that the memory region to read does not cross multiple frames */
  if (PHY_TO_FRAME(phy) != PHY_TO_FRAME(phy+size-1)) {
      MmuPrint("[MMU] Error: physical region %.8x-%.8x crosses multiple frames\n", phy, phy+size-1);
      return HVM_STATUS_UNSUCCESSFUL;
  }
  
  r = MmuMapPhysicalPage(phy, &dwLogicalAddress, &entryOriginal);
  if (r != HVM_STATUS_SUCCESS)
    return HVM_STATUS_UNSUCCESSFUL;
  dwLogicalAddress += MMU_PAGE_OFFSET(phy);
  
  if (!isWrite) {
    /* Read memory page */
    MmuPrint("[MMU] MmuReadWritePhysicalRegion() Going to read %d from va: %.8x\n", size, dwLogicalAddress);
    vmm_memcpy(buffer, (Bit8u*) dwLogicalAddress, size);
    
  } else {
    /* Write to memory page */
    vmm_memcpy((Bit8u*) dwLogicalAddress, buffer, size);
  }
  
  MmuPrint("[MMU] MmuReadWritePhysicalRegion() All done!\n");
  
  MmuUnmapPhysicalPage(dwLogicalAddress, entryOriginal);
  
  return HVM_STATUS_SUCCESS;
}

hvm_status MmuReadWriteVirtualRegion(hvm_address cr3, hvm_address va, void* buffer, 
				     Bit32u size, hvm_bool isWrite)
{
  hvm_status r;
  hvm_phy_address phy;
  Bit32u i, n;

  i = 0;
  MmuPrint("[MMU] MmuReadWriteVirtualRegion() cr3: %.8x va: %.8x size: %.8x isWrite? %d\n",
	   CR3_ALIGN(cr3), va, size, isWrite);

  while (va+i < va+size) {
    n = MIN(size-i, MMU_PAGE_SIZE - MMU_PAGE_OFFSET(va+i));
    r = MmuGetPhysicalAddress(CR3_ALIGN(cr3), va+i, &phy);
    if (r != HVM_STATUS_SUCCESS)
      return HVM_STATUS_UNSUCCESSFUL;

    MmuPrint("[MMU] MmuReadWriteVirtualRegion() Reading phy %.8x (write? %d)\n",
	     phy, isWrite);
    r = MmuReadWritePhysicalRegion(phy, (void*) ((hvm_address)buffer+i), n, isWrite);    
    MmuPrint("[MMU] MmuReadWriteVirtualRegion() Read! Success? %d\n", (r == HVM_STATUS_SUCCESS));

    if (r != HVM_STATUS_SUCCESS)
      return HVM_STATUS_UNSUCCESSFUL;

    i += n;
  }

  MmuPrint("[MMU] MmuReadWriteVirtualRegion() done!\n");

  return HVM_STATUS_SUCCESS;;
}

/* 
   Given a virtual address and a cr3 value, get the PTE (or PDE, for 4MB pages)
   that maps the specified address in the specified process.
   Note: 'ppte' is OPTIONAL (i.e., it can be NULL).
 */
static hvm_status MmuGetPageEntry (hvm_address cr3, hvm_address va, PPTE ppte, hvm_bool* pisLargePage)
{
  hvm_status r;
  hvm_phy_address addr;
  PTE p;

  MmuPrint("[MMU] MmuGetPageEntry() cr3: %.8x va: %.8x\n", CR3_ALIGN(cr3), va);

#ifdef ENABLE_PAE
  /* Read PDPTE */
  addr = CR3_ALIGN(cr3) + (VA_TO_PDPTE(va)*sizeof(PTE));
  r = MmuReadPhysicalRegion(addr, &p, sizeof(PTE));
  if (r != HVM_STATUS_SUCCESS) {
    MmuPrint("[MMU] MmuGetPageEntry() cannot read PDPTE from %.8x\n", addr);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  if (!p.Present)
    return HVM_STATUS_UNSUCCESSFUL;
  
  /* Read PDE */
  addr = FRAME_TO_PHY(p.PageBaseAddr) + (VA_TO_PDE(va)*sizeof(PTE));
#else
  /* Read PDE */
  addr = CR3_ALIGN(cr3) + (VA_TO_PDE(va)*sizeof(PTE));
#endif
  
  MmuPrint("[MMU] MmuGetPageEntry() Reading phy %.8x%.8x (NOT large)\n", GET32H(addr), GET32L(addr));
  r = MmuReadPhysicalRegion(addr, &p, sizeof(PTE));
  
  if (r != HVM_STATUS_SUCCESS) {
    MmuPrint("[MMU] MmuGetPageEntry() cannot read PDE from %.8x\n", addr);
    return HVM_STATUS_UNSUCCESSFUL;
  }
  
  MmuPrint("[MMU] MmuGetPageEntry() PDE read. Present? %d Large? %d\n", p.Present, p.LargePage);
  
  if (!p.Present)
    return HVM_STATUS_UNSUCCESSFUL;
  
  /* If it's present and it's a 4MB page, then this is a hit */
  if(p.LargePage) {
    if (ppte) *ppte = p;
    *pisLargePage = TRUE;
    return HVM_STATUS_SUCCESS;
  }

  /* Read PTE */
  addr = FRAME_TO_PHY(p.PageBaseAddr) + (VA_TO_PTE(va)*sizeof(PTE));
  r = MmuReadPhysicalRegion(addr, &p, sizeof(PTE));

  if (r != HVM_STATUS_SUCCESS) {
    MmuPrint("[MMU] MmuGetPageEntry() cannot read PTE from %.8x\n", addr);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  MmuPrint("[MMU] MmuGetPageEntry() PTE read. Present? %d\n", p.Present);

  if (!p.Present)
    return HVM_STATUS_UNSUCCESSFUL;
  
  if (ppte) *ppte = p;
  *pisLargePage = FALSE;
  
  return HVM_STATUS_SUCCESS;
}

#ifdef GUEST_LINUX

/* Inspired from http://lxr.linux.no/#linux+v2.6.26.2/arch/x86/mm/fault.c */
void MmuVirtToPTE(hvm_address addr, hvm_address *pPTEAddr)
{
  PPTE ppde, ppte;
  *pPTEAddr = 0;
  
  ppde = (PPTE) phys_to_virt(CR3_ALIGN(RegGetCr3()) + (VA_TO_PDE(addr)*sizeof(PTE)));

  if(ppde->Present && !ppde->LargePage) {
    ppte = (PPTE) phys_to_virt(FRAME_TO_PHY(ppde->PageBaseAddr) + (VA_TO_PTE(addr)*sizeof(PTE)));
    *pPTEAddr = (hvm_address)ppte;
  }
}

#endif

static hvm_status MmuFindUnusedPTE(hvm_address* pdwLogical)
{
  hvm_address dwCurrentAddress, dwPTEAddr;
  
#ifdef GUEST_LINUX
  Bit32u dwPTE;

  for (dwCurrentAddress=PAGE_OFFSET; dwCurrentAddress < 0xfffff000; dwCurrentAddress += MMU_PAGE_SIZE) {
    MmuVirtToPTE(dwCurrentAddress, &dwPTEAddr);
    if (dwPTEAddr == 0)
      continue;

    dwPTE = READ_PTE(dwPTEAddr);
    if (PDE_TO_VALID(dwPTE))
      continue;    
    *pdwLogical= dwCurrentAddress;
    return HVM_STATUS_SUCCESS;
  }
  return HVM_STATUS_UNSUCCESSFUL;
  
#elif defined GUEST_WINDOWS
  hvm_address dwPDEAddr;
  
  for (dwCurrentAddress=MMU_PAGE_SIZE; dwCurrentAddress < 0x80000000; dwCurrentAddress += MMU_PAGE_SIZE) {
    /* Check if memory page at logical address 'dwCurrentAddress' is free */
#ifdef ENABLE_PAE
    Bit64u dwPDE, dwPTE;
    dwPDEAddr = VIRTUAL_PD_BASE +					\
      ((VA_TO_PDE(dwCurrentAddress) | (VA_TO_PDPTE(dwCurrentAddress) << 9)) * sizeof(PTE));
#else
    Bit32u dwPDE, dwPTE;
    dwPDEAddr = VIRTUAL_PD_BASE + (VA_TO_PDE(dwCurrentAddress) * sizeof(PTE));
#endif
    
    dwPDE = READ_PTE(dwPDEAddr);
    if (!PDE_TO_VALID(dwPDE))
      continue;
    
    dwPTEAddr = VIRTUAL_PT_BASE + (VA_TO_PTE(dwCurrentAddress) * sizeof(PTE));
    dwPTE = READ_PTE(dwPTEAddr);

    if (PDE_TO_VALID(dwPTE)) {
      /* Skip *valid* PTEs */
      continue;
    }
    
    /* All done!*/
    *pdwLogical = dwCurrentAddress;
    return HVM_STATUS_SUCCESS;
  }

  return HVM_STATUS_UNSUCCESSFUL;
#endif

}

static hvm_bool MmuGetCr0WP(void)
{
  CR0_REG cr0_reg;

  CR0_TO_ULONG(cr0_reg) = RegGetCr0();

  return (cr0_reg.WP != 0);
}

/* FIXME: please refactor */
static hvm_status MmuFindUnusedPTEs(hvm_address* pdwLogical, int goal)
{
  hvm_address dwCurrentAddress, dwPTEAddr, start, end;
  int r = 0;
#ifdef GUEST_WINDOWS
  hvm_address dwPDEAddr;
  start = MMU_PAGE_SIZE; 
  end   = 0x80000000;
#elif defined GUEST_LINUX
  start = PAGE_OFFSET;
  end   = 0xfffff000;
#endif

  for (dwCurrentAddress=start; (dwCurrentAddress < end)&&(r<goal); dwCurrentAddress += MMU_PAGE_SIZE){
    /* Check if memory page at logical address 'dwCurrentAddress' is free */
#ifdef GUEST_LINUX
    Bit32u dwPTE;
    MmuVirtToPTE(dwCurrentAddress, &dwPTEAddr);
    if (dwPTEAddr == 0){
      r = 0;
      *pdwLogical = 0;
      continue;
    }
#elif defined GUEST_WINDOWS
#ifdef ENABLE_PAE
    Bit64u dwPDE, dwPTE;
    dwPDEAddr = VIRTUAL_PD_BASE + ((VA_TO_PDE(dwCurrentAddress) | (VA_TO_PDPTE(dwCurrentAddress) << 9)) * sizeof(PTE));
#else
    Bit32u dwPDE, dwPTE;
    dwPDEAddr = VIRTUAL_PD_BASE + (VA_TO_PDE(dwCurrentAddress) * sizeof(PTE));
#endif
      
    dwPDE = READ_PTE(dwPDEAddr);
    if (!PDE_TO_VALID(dwPDE)){
      r = 0;
      *pdwLogical= (hvm_address) NULL;
      continue;
    }  
    dwPTEAddr = VIRTUAL_PT_BASE + (VA_TO_PTE(dwCurrentAddress) * sizeof(PTE));
#endif

    dwPTE = READ_PTE(dwPTEAddr);
    if (PDE_TO_VALID(dwPTE)) {
      /* Skip *valid* PTEs */
      r= 0;
      *pdwLogical = 0;
      continue;
      
    }
    
    /* Found!*/
    /* Take the address of the every first free PTE */
    if (r==0)
      *pdwLogical = dwCurrentAddress;
    r++;
  } /* for(;;) */

  if ( (r == goal) || (pdwLogical!=NULL) )
    return HVM_STATUS_SUCCESS;
  else
    return HVM_STATUS_UNSUCCESSFUL;
}

hvm_status MmuUnmapPhysicalSpace(hvm_address va, Bit32u memSize){
  int i, numOfPages;
  hvm_address dwEntryAddress;
  PPTE pentry;

  numOfPages= memSize/MMU_PAGE_SIZE;
  if(memSize % MMU_PAGE_SIZE != 0)
    numOfPages++;

  for ( i=0; i< numOfPages; i++ ){
#ifdef GUEST_WINDOWS
    dwEntryAddress = VIRTUAL_PT_BASE + (VA_TO_PTE(va + i*(MMU_PAGE_SIZE))*sizeof(PTE));
#elif defined GUEST_LINUX
    MmuVirtToPTE(va + i*(MMU_PAGE_SIZE), &dwEntryAddress);
#endif
    
    /* make each PTE non-valid by setting the Present Bit*/
    pentry= (PPTE) dwEntryAddress;
    if ((i==0) || (i== numOfPages-1)){
      Log("[&PTE:%.8x] Unmapping from phy_address %.8x", dwEntryAddress, pentry->PageBaseAddr);
    }
    pentry->Present= 0;
  }
  
  hvm_x86_ops.mmu_tlb_flush();
  
  return HVM_STATUS_SUCCESS;
}

hvm_status MmuMapPhysicalSpace(hvm_address phy, Bit32u memSize, hvm_address* pva){ 
  int numOfPages, i;
  hvm_address dwEntryAddress, dwLogicalAddress = 0;
  PPTE pentry;
  
  numOfPages = memSize/MMU_PAGE_SIZE;
  if(memSize % MMU_PAGE_SIZE != 0)
    numOfPages++;

  /* Seek for enough unused PTEs in the current process */
  if (!HVM_SUCCESS(MmuFindUnusedPTEs(&dwLogicalAddress, numOfPages))){
    Log("[MMU] MmuMapPhysicalSpace() mapping of %iB from %.8x failed\n", memSize, phy);  
    return HVM_STATUS_UNSUCCESSFUL;
  }
   
  /* Map physical space to obtained logical addresses */ 
  for(i = 0; i < numOfPages; i++){
#ifdef GUEST_WINDOWS
    dwEntryAddress = VIRTUAL_PT_BASE + (VA_TO_PTE(dwLogicalAddress + i*(MMU_PAGE_SIZE))*sizeof(PTE));
#elif defined GUEST_LINUX    
    MmuVirtToPTE(dwLogicalAddress + i*MMU_PAGE_SIZE, &dwEntryAddress);
#endif

#if 0
    if (i ==0) Log("[MMU] [MmuMapPhysicalSpace] first usable PTE found at %.8x; &PTE:%.8x", dwLogicalAddress, dwEntryAddress);   
    if (i == numOfPages-1) Log("[MMU] [MmuMapPhysicalSpace] last usable PTE found at %.8x; &PTE:%.8x", dwLogicalAddress, dwEntryAddress);   
#endif
    
    pentry = (PPTE) dwEntryAddress;
  
    /* Replace PT entry */
    pentry->Present         = 1;
    pentry->Writable        = 1;
    pentry->Owner           = 1;
    pentry->WriteThrough    = 0;
    pentry->CacheDisable    = 0;
    pentry->Accessed        = 0;
    pentry->Dirty           = 0;
    pentry->LargePage       = 0;
    pentry->Global          = 0;
    pentry->ForUse1         = 0;
    pentry->ForUse2         = 0;
    pentry->ForUse3         = 0;
    pentry->PageBaseAddr    = PHY_TO_FRAME(phy + i*(MMU_PAGE_SIZE));
  }
  
  hvm_x86_ops.mmu_tlb_flush();

  *pva = dwLogicalAddress;
  return HVM_STATUS_SUCCESS;
}

#if 0
static hvm_status MmuFindUnusedPDE(hvm_address* pdwLogical)
{
  hvm_address dwCurrentAddress, dwPDEAddr;

#ifdef GUEST_WINDOWS
  for (dwCurrentAddress=MMU_PAGE_SIZE; dwCurrentAddress < 0x80000000; dwCurrentAddress += MMU_PAGE_SIZE) {
    /* Check if memory page at logical address 'dwCurrentAddress' is free */
#ifdef ENABLE_PAE
    Bit64u dwPDE;
    dwPDEAddr = VIRTUAL_PD_BASE + \
      ((VA_TO_PDE(dwCurrentAddress) | (VA_TO_PDPTE(dwCurrentAddress) << 9)) * sizeof(PTE));
#else
    Bit32u dwPDE;
    dwPDEAddr = VIRTUAL_PD_BASE + (VA_TO_PDE(dwCurrentAddress) * sizeof(PTE));
#endif

    dwPDE = READ_PTE(dwPDEAddr);
    if (PDE_TO_VALID(dwPDE)) {
      /* Skip *valid* PDEs */
      continue;
    }

    /* All done!*/
    *pdwLogical = dwCurrentAddress;
    return HVM_STATUS_SUCCESS;
  }

  return HVM_STATUS_UNSUCCESSFUL;
#elif defined GUEST_LINUX
  ;
#endif
}

static void MmuPrintPDEntry(PPTE pde)
{
  ComPrint("======= PDE @ 0x%.8x =======\n", pde);
  ComPrint("Present [0x%.8x]\n", pde->Present);
  ComPrint("Writable [0x%.8x]\n", pde->Writable);
  ComPrint("Owner [0x%.8x]\n", pde->Owner);
  ComPrint("WriteThrough [0x%.8x]\n", pde->WriteThrough);
  ComPrint("CacheDisable [0x%.8x]\n", pde->CacheDisable);
  ComPrint("Accessed [0x%.8x]\n", pde->Accessed);
  ComPrint("Reserved [0x%.8x]\n", pde->Dirty);
  ComPrint("PageSize [0x%.8x]\n", pde->LargePage);
  ComPrint("Global [0x%.8x]\n", pde->Global);
  ComPrint("PTAddress [0x%.8x]\n", pde->PageBaseAddr);
}

static void MmuPrintPTEntry(PPTE pte)
{
  ComPrint("======= PTE @ 0x%.8x ======\n", pte);
  ComPrint("Present [0x%.8x]\n", pte->Present);
  ComPrint("Writable [0x%.8x]\n", pte->Writable);
  ComPrint("Owner [0x%.8x]\n", pte->Owner);
  ComPrint("WriteThrough [0x%.8x]\n", pte->WriteThrough);
  ComPrint("CacheDisable [0x%.8x]\n", pte->CacheDisable);
  ComPrint("Accessed [0x%.8x]\n", pte->Accessed);
  ComPrint("Dirty [0x%.8x]\n", pte->Dirty);
  ComPrint("PAT [0x%.8x]\n", pte->LargePage);
  ComPrint("Global [0x%.8x]\n", pte->Global);
  ComPrint("PageBaseAddress [0x%.8x]\n", pte->PageBaseAddr);
}

static void MmuPrintPageEntry(PPTE Entry)
{  
  ComPrint("%08x:%20x|%1x|%1x|%1x|%1x|%1x|%1x|%1x|%1x|%1x|%1x|%1x|%1x\n", \
	   Entry,							\
	   Entry->PageBaseAddr,						\
	   Entry->ForUse3,						\
	   Entry->ForUse2,						\
	   Entry->ForUse1,						\
	   Entry->Global,						\
	   Entry->LargePage,						\
	   Entry->Dirty,						\
	   Entry->Accessed,						\
	   Entry->CacheDisable,						\
	   Entry->WriteThrough,						\
	   Entry->Owner,						\
	   Entry->Writable,						\
	   Entry->Present						\
	   );
}
#endif
