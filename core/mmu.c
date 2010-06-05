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

#include "common.h"
#include "debug.h"
#include "mmu.h"
#include "x86.h"

#ifdef ENABLE_PAE
#error PAE support still not available
#endif

/* ################ */
/* #### MACROS #### */
/* ################ */

#define MmuPrint(fmt, ...)

/* This is the virtual base address for the page directory of the current
   process */
#define VIRTUAL_PD_BASE     0xC0300000 
#define VIRTUAL_PD_BASE_PAE 0xC0600000 /* Use this when PAE is enabled */
#define VIRTUAL_PT_BASE     0xC0000000 

/* ########################## */
/* #### LOCAL PROTOTYPES #### */
/* ########################## */

static hvm_status MmuFindUnusedPTE(hvm_address* pdwLogical);
static hvm_status MmuFindUnusedPDE(hvm_address* pdwLogical);
static hvm_status MmuGetPageEntry(hvm_address cr3, hvm_address va, PPTE ppte, hvm_bool* pisLargePage);
static hvm_bool   MmuGetCr0WP(void);
static void       MmuPrintPDEntry(PPTE pde);
static void       MmuPrintPTEntry(PPTE pte);

/* ################ */
/* #### BODIES #### */
/* ################ */

hvm_bool MmuIsAddressWritable(hvm_address cr3, hvm_address va)
{
  hvm_status r;
  PTE p;
  hvm_bool isLarge;

  if (!MmuIsAddressValid(CR3_TO_PDBASE(cr3), va)) {
    return FALSE;
  }

  /* Data may be written to any linear address with a valid translation */
  if(!MmuGetCr0WP()) { 
    MmuPrint("[MMU] CR0.WP == 0\n");
    return TRUE;
  }

  r = MmuGetPageEntry(CR3_TO_PDBASE(cr3), va, &p, &isLarge);
  if (r != HVM_STATUS_SUCCESS) {
    return FALSE;
  }

  return (p.Writable != 0);
}

hvm_bool MmuIsAddressValid(hvm_address cr3, hvm_address va)
{
  hvm_status r;
  hvm_bool isLarge;

  r = MmuGetPageEntry(CR3_TO_PDBASE(cr3), va, NULL, &isLarge);

  return (r == HVM_STATUS_SUCCESS);
}

hvm_status MmuGetPhysicalAddress(hvm_address cr3, hvm_address va, hvm_phy_address* pphy)
{
  hvm_status r;
  PTE pte;
  hvm_bool isLarge;

  r = MmuGetPageEntry(CR3_TO_PDBASE(cr3), va, &pte, &isLarge);
  if (r != HVM_STATUS_SUCCESS) {
    return HVM_STATUS_UNSUCCESSFUL;
  }

  if (isLarge) {
    *pphy = LARGEFRAME_TO_PHY(pte.PageBaseAddr>>10) + LARGEPAGE_OFFSET(va);
    MmuPrint("[MMU] MmuGetPhysicalAddress(LARGE) cr3: %.8x frame: %.8x va: %.8x phy: %.8x\n", 
	     CR3_TO_PDBASE(cr3), pte.PageBaseAddr, va, *pphy);
    return HVM_STATUS_SUCCESS;
  }

  *pphy = FRAME_TO_PHY(pte.PageBaseAddr) + PAGE_OFFSET(va);
  
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
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  dwEntryAddress = VIRTUAL_PT_BASE + (VA_TO_PTE(dwLogicalAddress) * sizeof(PTE));
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
  pentry = (PPTE) (VIRTUAL_PT_BASE + (VA_TO_PTE(va) * sizeof(PTE)));
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
  if (r != HVM_STATUS_SUCCESS) return HVM_STATUS_UNSUCCESSFUL;

  dwLogicalAddress += PAGE_OFFSET(phy);

  if (!isWrite) {
    /* Read memory page */
    MmuPrint("[MMU] MmuReadWritePhysicalRegion() Going to read %d from va: %.8x\n", size, dwLogicalAddress);

    memcpy(buffer, (Bit8u*) dwLogicalAddress, size);
  } else {
    /* Write to memory page */
    memcpy((Bit8u*) dwLogicalAddress, buffer, size);
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
	   CR3_TO_PDBASE(cr3), va, size, isWrite);

  while (va+i < va+size) {
    n = MIN(size-i, PAGE_SIZE-PAGE_OFFSET(va+i));
    r = MmuGetPhysicalAddress(CR3_TO_PDBASE(cr3), va+i, &phy);
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
static hvm_status MmuGetPageEntry(hvm_address cr3, hvm_address va, PPTE ppte, 
				  hvm_bool* pisLargePage)
{
  hvm_status r;
  hvm_phy_address addr;
  PTE p;

  MmuPrint("[MMU] MmuGetPageEntry() cr3: %.8x va: %.8x\n", CR3_TO_PDBASE(cr3), va);

  /* Read PDE */
  addr = CR3_TO_PDBASE(cr3) + (VA_TO_PDE(va)*sizeof(PTE));
  MmuPrint("[MMU] MmuGetPageEntry() Reading phy %.8x (NOT large)\n", addr);
  r = MmuReadPhysicalRegion(addr, &p, sizeof(PTE));
  if (r != HVM_STATUS_SUCCESS) {
    MmuPrint("[MMU] MmuGetPageEntry() cannot read PDE from %.8x\n", addr);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  MmuPrint("[MMU] MmuGetPageEntry() PDE read. Present? %d Large? %d\n", p.Present, p.LargePage);

/*   MmuPrintPDEntry(&p); */

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

/*   MmuPrintPTEntry(&p); */

  if (!p.Present)
    return HVM_STATUS_UNSUCCESSFUL;

  if (ppte) *ppte = p;
  *pisLargePage = FALSE;

  return HVM_STATUS_SUCCESS;
}

static hvm_status MmuFindUnusedPTE(hvm_address* pdwLogical)
{
  hvm_address dwCurrentAddress, dwPTEAddr, dwPDEAddr;
  Bit32u dwPDE, dwPTE;

  for (dwCurrentAddress=PAGE_SIZE; dwCurrentAddress < 0x80000000; dwCurrentAddress += PAGE_SIZE) {
    /* Check if memory page at logical address 'dwCurrentAddress' is free */
    dwPDEAddr = VIRTUAL_PD_BASE + (VA_TO_PDE(dwCurrentAddress) * sizeof(PTE));

    dwPDE = *(Bit32u*) dwPDEAddr;
    if (!PDE_TO_VALID(dwPDE))
      continue;

    dwPTEAddr = VIRTUAL_PT_BASE + (VA_TO_PTE(dwCurrentAddress) * sizeof(PTE));

    dwPTE = *(Bit32u*) dwPTEAddr;
    if (PDE_TO_VALID(dwPTE)) {
      /* Skip *valid* PTEs */
      continue;
    }

    /* All done!*/
    *pdwLogical = dwCurrentAddress;
    return HVM_STATUS_SUCCESS;
  }

  return HVM_STATUS_UNSUCCESSFUL;
}

static hvm_status MmuFindUnusedPDE(hvm_address* pdwLogical)
{
  hvm_address dwCurrentAddress, dwPDEAddr;
  Bit32u dwPDE;

  for (dwCurrentAddress=PAGE_SIZE; dwCurrentAddress < 0x80000000; dwCurrentAddress += PAGE_SIZE) {
    /* Check if memory page at logical address 'dwCurrentAddress' is free */
    dwPDEAddr = VIRTUAL_PD_BASE + (VA_TO_PDE(dwCurrentAddress) * sizeof(PTE));

    dwPDE = *(Bit32u*) dwPDEAddr;
    if (PDE_TO_VALID(dwPDE)) {
      /* Skip *valid* PDEs */
      continue;
    }

    /* All done!*/
    *pdwLogical = dwCurrentAddress;
    return HVM_STATUS_SUCCESS;
  }

  return HVM_STATUS_UNSUCCESSFUL;
}

static hvm_bool MmuGetCr0WP(void)
{
  hvm_bool WPFlag;

  WPFlag = ((RegGetCr0() >> 16) & 0x1) != 0;

  return WPFlag;
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
