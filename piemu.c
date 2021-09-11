
#include "app.h"
#include "pfi.h"

extern char pfi_location[256];

int SetEmuParameters(struct tagPIEMU_CONTEXT* context, EMU* pEmuInfo, void* pUser)
{
  FILE* fp;
  PFIHEADER pfi;

  fp = fopen(pfi_location, "rb");
  if(!fp) DIE();
  if(!fread(&pfi, sizeof(PFIHEADER), 1, fp))
    DIE();
  if(pfi.dwSignature != 'PFI1') DIE();
  fclose(fp);

  pEmuInfo->sysinfo = pfi.siSysInfo;

  return 1;
}

#define FLASH_TOP 0x0c00000
#define READ_UNIT 0x1000        // 4096

int LoadFlashImage(struct tagPIEMU_CONTEXT* context, FLASH* pFlashInfo, void* pUser)
{
  FILE* fp;
  PFIHEADER pfi;
  unsigned int nOffset = 0;

  fp = fopen(pfi_location, "rb");
  if(!fp) DIE();
  if(!fread(&pfi, sizeof(PFIHEADER), 1, fp)) DIE();
  if(pfi.dwSignature != 'PFI1') DIE();
  if(!pfi.dwOffsetToFlash) DIE();

  fseek(fp, pfi.dwOffsetToFlash, SEEK_SET);

  for(nOffset = 0; nOffset < (unsigned)pFlashInfo->mem_size; nOffset += READ_UNIT)
  {
    fread(pFlashInfo->mem + nOffset, 1, READ_UNIT, fp);
  }

  fclose(fp);
  return 1;
}

int UpdateScreen(PIEMU_CONTEXT* context, void* pUser)
{
  SDL_Surface* surface;
  #ifdef _16BPP
  static const unsigned short palette[] = { 0xffff, 0xad55, 0x52aa, 0x0 };
  unsigned short* px;
  unsigned short* py;
  char* pp;
  #else
  static const unsigned int palette[] = { 0x00ffffff, 0x00aaaaaa, 0x00555555, 0x00000000 };
  unsigned int* px;
  unsigned int* py;
  char* pp;
  #endif

  surface = context->screen;

  if(SDL_MUSTLOCK(surface))
    SDL_LockSurface(surface);
#ifndef PSP
  pp = *context->vbuff;
  #ifdef _16BPP
  for(py = (unsigned short*)surface->pixels; py != (unsigned short*)surface->pixels + DISP_X * DISP_Y;
      py += DISP_X)
   #else
  for(py = (unsigned int*)surface->pixels; py != (unsigned int*)surface->pixels + DISP_X * DISP_Y;
      py += DISP_X)
   #endif
  {
    for(px = py; px != py + DISP_X; px++)
    {
      *px = palette[*pp++];
    }
  }
#else
  pp = *context->vbuff;
  for(py = (unsigned int*)surface->pixels; py != (unsigned int*)surface->pixels + (context->screen->w * 4) * DISP_Y;
      py += context->screen->w * 4)
  {
    for(px = py; px != py + DISP_X; px++)
    {
      *px = palette[*pp++];
    }
  }
#endif // !PSP
  if(SDL_MUSTLOCK(surface))
    SDL_UnlockSurface(surface);

//  SDL_BlitSurface(context->buffer, NULL, context->screen, NULL);
	SDL_Flip(context->screen);
//  SDL_UpdateRects(context->screen, 1, &rctDest);
  return 1;
}
