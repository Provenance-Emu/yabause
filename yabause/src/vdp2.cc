/*  Copyright 2003-2004 Guillaume Duhamel
    Copyright 2004 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "vdp2.hh"
#include "vdp1.hh"
#include "saturn.hh"
#include "scu.hh"
#include "yui.hh"
#ifndef _arch_dreamcast
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#endif
#include <stdarg.h>

#define COLOR_ADDt(b)		(b>0xFF?0xFF:(b<0?0:b))
#define COLOR_ADDb(b1,b2)	COLOR_ADDt((signed) (b1) + (b2))
#define COLOR_ADD(l,r,g,b)	COLOR_ADDb((l & 0xFF), r) | \
				(COLOR_ADDb((l >> 8 ) & 0xFF, g) << 8) | \
				(COLOR_ADDb((l >> 16 ) & 0xFF, b) << 16) | \
				(l & 0xFF000000)

#if 0
#define drawPixel(s,x,y,c)	if ((x >= 0) && (y >= 0) && (x < 1024) && (y < 512)) s[y * 1024 + x] = c;
#endif
#define drawPixel(s,x,y,c)	if ((x >= 0) && (y >= 0) && (x < width) && (y < height)) s[y * width + x] = c;


/****************************************/
/*					*/
/*		VDP2 Registers		*/
/*					*/
/****************************************/

void Vdp2::setWord(unsigned long addr, unsigned short val) {
  switch(addr) {
    case 0:
      int width, height;

      // Horizontal Resolution
      switch (val & 0x7) {
         case 0: width = 320;
                 break;
         case 1: width = 352;
                 break;
         case 2: width = 640;
                 break;
         case 3: width = 704;
                 break;
         case 4: width = 320;
                 break;
         case 5: width = 352;
                 break;
         case 6: width = 640;
                 break;
         case 7: width = 704;
                 break;
      }

      // Vertical Resolution
      switch ((val >> 4) & 0x3) {
         case 0: height = 224;
                 break;
         case 1: height = 240;
                 break;
         case 2: height = 256;
                 break;
         default: break;
      }

      // Check for interlace
      switch ((val >> 6) & 0x3) {
         case 2: // Single-density Interlace
         case 3: // Double-density Interlace
                 height *= 2;
                 break;
         case 0: // Non-interlace
         default: break;
      }

      setSaturnResolution(width, height);

      Memory::setWord(addr, val);
      break;
    case 0xE:
      Memory::setWord(addr, val);
      updateRam();
      break;
#if VDP2_DEBUG
    case 0x20:
      /*
      if (val & 0x10)
        cerr << "vdp2\t: RBG0 screen needs to be tested" << endl;
      else if (val & 0x20)
        cerr << "vdp2\t: RBG1 screen not implemented" << endl;
	*/
      Memory::setWord(addr, val);
      break;
#endif
    case 0xE0:
      Memory::setWord(addr, val);
#ifdef VDP2_DEBUG
      cerr << "sprite type modified" << endl;
#endif
      sortScreens();
      break;
    case 0xF8:
      Memory::setWord(addr, val);
      sortScreens();
      break;
    case 0xFA:
      Memory::setWord(addr, val);
      sortScreens();
      break;
    case 0xFC:
      Memory::setWord(addr, val);
      sortScreens();
      break;
    default:
      Memory::setWord(addr, val);
      break;
  }
}

/****************************************/
/*					*/
/*		VDP2 Color RAM		*/
/*					*/
/****************************************/

void Vdp2ColorRam::setWord(unsigned long addr, unsigned short val) {
      Memory::setWord(addr, val);

      if (mode == 0)
        Memory::setWord(addr + 0x800, val);
}

unsigned short Vdp2ColorRam::getWord(unsigned long addr) {
      return Memory::getWord(addr);
}

void Vdp2ColorRam::setMode(int v) {
	mode = v;
}

unsigned long Vdp2ColorRam::getColor(unsigned long addr, int alpha, int colorOffset) {
  switch(mode) {
  case 0: {
    addr *= 2; // thanks Runik!
    addr += colorOffset * 0x200;
    unsigned long tmp = readWord(this, addr);
    return SAT2YAB1(alpha, tmp);
  }
  case 1: {
    addr *= 2; // thanks Runik!
    addr += colorOffset * 0x200;
    unsigned long tmp = readWord(this, addr);
    return SAT2YAB1(alpha, tmp);
  }
  case 2: {
    addr *= 4;
    addr += colorOffset * 0x400;
    unsigned long tmp1 = readWord(this, addr);
    unsigned long tmp2 = readWord(this, addr + 2);
    return SAT2YAB2(alpha, tmp1, tmp2);
  }
  default: break;
  }

  return 0;
}

/****************************************/
/*					*/
/*		VDP2 Screen		*/
/*					*/
/****************************************/

Vdp2Screen::Vdp2Screen(Vdp2 *r, Vdp2Ram *v, Vdp2ColorRam *c, unsigned long *s) {
    reg = r;
    vram = v;
    cram = c;
    surface = s;
    //surface = new unsigned long [ 1024 * 512 ];
    disptoggle = true;
    getX = Vdp2Screen_getX;
    getY = Vdp2Screen_getY;

	if (*texture ==0) glGenTextures(1, texture );
	glBindTexture(GL_TEXTURE_2D, texture[0] );
#ifndef _arch_dreamcast
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface);
  
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB4444, 1024, 512, 0, GL_ARGB4444, GL_UNSIGNED_BYTE, surface);
#endif

}

int VdpScreen::comparePriority(const void *arg1, const void *arg2) {
  VdpScreen **screen1 = (VdpScreen **) arg1;
  VdpScreen **screen2 = (VdpScreen **) arg2;
  int s1 = (*screen1)->getPriority();
  int is1 = (*screen1)->getInnerPriority();
  int s2 = (*screen2)->getPriority();
  int is2 = (*screen2)->getInnerPriority();

  if (s1 == s2) return is1 - is2;
  else return s1 - s2;
}

void Vdp2Screen::draw(void) {
        float calcwidthRatio, calcheightRatio;

	init();

        if (!(enable & disptoggle) || (getPriority() == 0)) return;

	if (bitmap) {
		drawCell();
	}
	else {
		drawMap();
	}

        calcwidthRatio = (width * coordIncX) / 1024;
        calcheightRatio = (height * coordIncY) / 512;

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, texture[0] );
	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, surface);
	glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(0, 0);
        glTexCoord2f(calcwidthRatio, 0); glVertex2i(width, 0);
        glTexCoord2f(calcwidthRatio, calcheightRatio); glVertex2i(width, height);
        glTexCoord2f(0, calcheightRatio); glVertex2i(0, height);
	glEnd();
	glDisable( GL_TEXTURE_2D );
}

void Vdp2Screen::drawMap(void) {
	int X, Y;
	X = x;

        //for(int i = 0;i < mapWH;i++) {
		Y = y;
		x = X;
		//for(int j = 0;j < mapWH;j++) {
			y = Y;
			planeAddr(0);
			//planeAddr(mapWH * i + j);
			drawPlane();
		//}
	//}
}

void Vdp2Screen::drawPlane(void) {
	int X, Y;

	X = x;
	for(int i = 0;i < planeH;i++) {
		Y = y;
		x = X;
		for(int j = 0;j < planeW;j++) {
			y = Y;
			drawPage();
		}
	}
}

void Vdp2Screen::drawPage(void) {
	int X, Y;

	X = x;
	for(int i = 0;i < pageWH;i++) {
		Y = y;
		x = X;
		for(int j = 0;j < pageWH;j++) {
			y = Y;
			patternAddr();
			drawPattern();
		}
	}
}

void Vdp2Screen::patternAddr(void) {
	switch(patternDataSize) {
	case 1: {
                unsigned short tmp = readWord(vram, addr);

		addr += 2;
		specialFunction = supplementData & 0x300 >> 8;
    		switch(colorNumber) {
      		case 0: // in 16 colors
			palAddr = ((tmp & 0xF000) >> 12) | ((supplementData & 0xE0) >> 1);
        		break;
      		default: // not in 16 colors
			palAddr = (tmp & 0x7000) >> 8;
			break;
    		}
    		switch(auxMode) {
    		case 0:
      			flipFunction = (tmp & 0xC00) >> 10;
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0x3FF) | ((supplementData & 0x1F) << 10);
				break;
      			case 2:
				charAddr = ((tmp & 0x3FF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x1C) << 10);
				break;
      			}
      			break;
    		case 1:
      			flipFunction = 0;
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0xFFF) | ((supplementData & 0x1C) << 10);
				break;
      			case 4:
				charAddr = ((tmp & 0xFFF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x10) << 10);
				break;
      			}
      			break;
    		}
    		break;
	}
  	case 2: {
                unsigned short tmp1 = readWord(vram, addr);

    		addr += 2;
                unsigned short tmp2 = readWord(vram, addr);
    		addr += 2;
    		charAddr = tmp2 & 0x7FFF;
    		flipFunction = (tmp1 & 0xC000) >> 14;
                palAddr = (tmp1 & 0x7F);
    		specialFunction = (tmp1 & 0x3000) >> 12;
    		break;
	}
	}
        if (!(readWord(reg, 0x6) & 0x8000)) charAddr &= 0x3FFF;

	charAddr *= 0x20; // selon Runik
}

void Vdp2Screen::drawPattern(void) {
	int X, Y;
	int xEnd, yEnd;

	if(flipFunction & 0x1) { // vertical flip
		x += patternWH * 8 - 1;
    		xEnd = patternWH * 8 + 1;
  	}
 	else {
    		xEnd = 0;
  	}

  	if(flipFunction & 0x2) { // horizontal flip
    		y += patternWH * 8 - 1;
    		yEnd = patternWH * 8 + 1;
  	}
  	else {
    		yEnd = 0;
  	}

	X = x;
	for(int i = 0;i < patternWH;i++) {
		Y = y;
		x = X;
		for(int j = 0;j < patternWH;j++) {
			y = Y;
			drawCell();
		}
	}
	x += xEnd;
	y += yEnd;
}

void Vdp2Screen::drawCell(void) {
  unsigned long color;
  int X;
  int xInc, yInc;

  if(flipFunction & 0x1) { // vertical flip
    xInc = -1;
  }
  else {
    xInc = 1;
  }

  if(flipFunction & 0x2) { // horizontal flip
    yInc = -1;
  }
  else {
    yInc = 1;
  }

  switch(colorNumber) {
    case 0:
      // 4-bit Mode(16 colors)
      X = x;
      for(int i = 0;i < cellH;i++) {
	x = X;
	for(int j = 0;j < cellW;j+=4) {
          unsigned short dot = readWord(vram, charAddr);

	  charAddr += 2;
	  if (!(dot & 0xF000) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | ((dot & 0xF000) >> 12), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	  if (!(dot & 0xF00) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | ((dot & 0xF00) >> 8), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	  if (!(dot & 0xF0) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | ((dot & 0xF0) >> 4), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	  if (!(dot & 0xF) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | (dot & 0xF), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	}
	y += yInc;
      }
      break;
    case 1:
      // 8-bit Mode(256 colors)
      X = x;
      for(int i = 0;i < cellH;i++) {
	x = X;
	for(int j = 0;j < cellW;j+=2) {
          unsigned short dot = readWord(vram, charAddr);

	  charAddr += 2;
	  if (!(dot & 0xFF00) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | ((dot & 0xFF00) >> 8), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	  if (!(dot & 0xFF) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor((palAddr << 4) | (dot & 0xFF), alpha, colorOffset);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	}
	y += yInc;
      }
      break;
    case 2:
      // 16-bit Mode(2048 colors)
      X = x;
      for(int i = 0;i < cellH;i++) {
	x = X;
	for(int j = 0;j < cellW;j++) {
          unsigned short dot = readWord(vram, charAddr);
	  if ((dot == 0) && transparencyEnable) color = 0x00000000;
          else color = cram->getColor(dot, alpha, colorOffset);
	  charAddr += 2;
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	}
	y += yInc;
      }
      break;
    case 3:
      // 16-bit Mode(32,786 colors)
      X = x;
      for(int i = 0;i < cellH;i++) {
	x = X;
	for(int j = 0;j < cellW;j++) {
          unsigned short dot = readWord(vram, charAddr);
	  charAddr += 2;
          if (!(dot & 0x8000) && transparencyEnable) color = 0x00000000;
	  else color = SAT2YAB1(0xFF, dot);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	}
	y += yInc;
      }
      break;
    case 4:
      // 32-bit Mode(16,770,000 colors)
      X = x;
      for(int i = 0;i < cellH;i++) {
	x = X;
	for(int j = 0;j < cellW;j++) {
          unsigned short dot1 = readWord(vram, charAddr);
	  charAddr += 2;
          unsigned short dot2 = readWord(vram, charAddr);
	  charAddr += 2;
          if (!(dot1 & 0x8000) && transparencyEnable) color = 0x00000000;
	  else color = SAT2YAB2(alpha, dot1, dot2);
	  drawPixel(surface, getX(this, x,y), getY(this, x,y), COLOR_ADD(color,cor,cog,cob));
	  x += xInc;
	}
	y += yInc;
      }
      break;
  }
}

/*
void Vdp2Screen::drawPixel(unsigned long *surface, Sint16 x, Sint16 y, Uint32 tmpcolor) {
        if ((x >= 0) && (y >= 0) && (x < 1024) && (y < 512)) {
                surface[y * 1024 + x] = tmpcolor;
	}
}
*/

void Vdp2Screen::toggleDisplay(void) {
   disptoggle ^= true;
}

void Vdp2Screen::readRotationTable(unsigned long addr) {
	long i;

        i = vram->getLong(addr);
	Xst = (float) (signed) ((i & 0x1FFFFFC0) | (i & 0x10000000 ? 0xF0000000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	Yst = (float) (signed) ((i & 0x1FFFFFC0) | (i & 0x10000000 ? 0xF0000000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	Zst = (float) (signed) ((i & 0x1FFFFFC0) | (i & 0x10000000 ? 0xF0000000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	deltaXst = (float) (signed) ((i & 0x0007FFC0) | (i & 0x00040000 ? 0xFFFC0000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	deltaYst = (float) (signed) ((i & 0x0007FFC0) | (i & 0x00040000 ? 0xFFFC0000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	deltaX = (float) (signed) ((i & 0x0007FFC0) | (i & 0x00040000 ? 0xFFFC0000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	deltaY = (float) (signed) ((i & 0x0007FFC0) | (i & 0x00040000 ? 0xFFFC0000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	A = (float) (signed) ((i & 0x000FFFC0) | (i & 0x00080000 ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	B = (float) (signed) ((i & 0x000FFFC0) | ((i & 0x00080000) ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	C = (float) (signed) ((i & 0x000FFFC0) | (i & 0x00080000 ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	D = (float) (signed) ((i & 0x000FFFC0) | (i & 0x00080000 ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	E = (float) (signed) ((i & 0x000FFFC0) | (i & 0x00080000 ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	F = (float) (signed) ((i & 0x000FFFC0) | (i & 0x00080000 ? 0xFFF80000 : 0x00000000)) / 65536;
	addr += 4;

        i = readWord(vram, addr);

	Px = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 2;

        i = readWord(vram, addr);
	Py = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 2;

        i = readWord(vram, addr);
	Pz = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 4;

        i = readWord(vram, addr);
	Cx = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 2;

        i = readWord(vram, addr);
	Cy = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 2;

        i = readWord(vram, addr);
	Cz = ((i & 0x3FFF) | (i & 0x2000 ? 0xE000 : 0x0000));
	addr += 4;

	i = vram->getLong(addr);
	Mx = (float) (signed) ((i & 0x3FFFFFC0) | (i & 0x20000000 ? 0xE0000000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	My = (float) (signed) ((i & 0x3FFFFFC0) | (i & 0x20000000 ? 0xE0000000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	kx = (float) (signed) ((i & 0x00FFFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) / 65536;
	addr += 4;

	i = vram->getLong(addr);
	ky = (float) (signed) ((i & 0x00FFFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) / 65536;
	addr += 4;
}

int Vdp2Screen_getX(Vdp2Screen * screen, int Hcnt, int Vcnt) {
	return Hcnt;
}

int Vdp2Screen_getY(Vdp2Screen * screen, int Hcnt, int Vcnt) {
	return Vcnt;
}

void Vdp2Screen::setTextureRatio(unsigned long widthR, unsigned long heightR) {
   width = widthR;
   height = heightR;
}

RBG0::RBG0(Vdp2 *reg, Vdp2Ram *vram, Vdp2ColorRam *cram, unsigned long *s) : Vdp2Screen(reg, vram, cram, s) {
	getX = RBG0_getX;
	getY = RBG0_getY;
}

int RBG0_getX(Vdp2Screen * tmp, int Hcnt, int Vcnt) {
	RBG0 * screen = (RBG0 *) tmp;
	float ret;
	float Xsp = screen->A * ((screen->Xst + screen->deltaXst * Vcnt) - screen->Px) + screen->B * ((screen->Yst + screen->deltaYst * Vcnt) - screen->Py) + screen->C * (screen->Zst - screen->Pz);
	float Xp = screen->A * (screen->Px - screen->Cx) + screen->B * (screen->Py - screen->Cy) + screen->C * (screen->Pz - screen->Cz) /*+ Cx + Mx*/;
	float dX = screen->A * screen->deltaX + screen->B * screen->deltaY;
	ret = (screen->kx * (Xsp + dX * Hcnt) + Xp);
	return (int) ret;
}

int RBG0_getY(Vdp2Screen * tmp, int Hcnt, int Vcnt) {
	RBG0 * screen = (RBG0 *) tmp;
	float ret;
	float Ysp = screen->D * ((screen->Xst + screen->deltaXst * Vcnt) - screen->Px) + screen->E * ((screen->Yst + screen->deltaYst * Vcnt) - screen->Py) + screen->F * (screen->Zst - screen->Pz);
	float Yp = screen->D * (screen->Px - screen->Cx) + screen->E * (screen->Py - screen->Cy) + screen->F * (screen->Pz - screen->Cz) /*+ Cy + My*/;
	float dY = screen->D * screen->deltaX + screen->E * screen->deltaY;
	ret = (screen->ky * (Ysp + dY * Hcnt) + Yp);
	return (int) ret;
}

void RBG0::init(void) {
        // For now, let's treat it like a regular scroll screen
        unsigned short patternNameReg = readWord(reg, 0x38);
        unsigned short patternReg = readWord(reg, 0x2A);
	unsigned long rotTableAddressA;
	unsigned long rotTableAddressB;
	rotTableAddressA = rotTableAddressB = reg->getLong(0xBC) << 1;
	rotTableAddressA &= 0x000FFF7C;
	rotTableAddressB = (rotTableAddressB & 0x000FFFFC) | 0x00000080;

	readRotationTable(rotTableAddressA);

        enable = readWord(reg, 0x20) & 0x10;
        transparencyEnable = !(readWord(reg, 0x20) & 0x1000);

        // Figure out which Rotation parameter to use here(or use both)
        unsigned long rotParaModeReg = reg->getLong(0xB0) & 0x3;

        x = 0; // this is obviously wrong
        y = 0; // this is obviously wrong

        colorNumber = (patternReg & 0x7000) >> 12;
        if(bitmap = patternReg & 0x200) {
                switch((patternReg & 0x400) >> 10) {
                        case 0: cellW = 512;
                                cellH = 256;
                                break;
                        case 1: cellW = 512;
                                cellH = 512;
                                break;
                }

                charAddr = (readWord(reg, 0x3E) & 0x7) * 0x20000;
                palAddr = (readWord(reg, 0x2E) & 0x7) << 4;
                flipFunction = 0;
                specialFunction = 0;
        }
        else {
		mapWH = 4;
		unsigned char planeSize;
		switch(rotParaModeReg) {
			case 0:
                                planeSize = (readWord(reg, 0x3A) & 0x300) >> 8;
				break;
			case 1:
                                planeSize = (readWord(reg, 0x3A) & 0x3000) >> 12;
				break;
			default:
#if VDP2_DEBUG
				cerr << "RGB0\t: don't know what to do with plane size" << endl;
#endif
                                planeSize = 0;
				break;
		}
  		switch(planeSize) {
  			case 0: planeW = planeH = 1; break;
  			case 1: planeW = 2; planeH = 1; break;
  			case 2: planeW = planeH = 2; break;
  		}
  		if(patternNameReg & 0x8000) patternDataSize = 1;
  		else patternDataSize = 2;
  		if(patternReg & 0x1) patternWH = 2;
  		else patternWH = 1;
  		pageWH = 64/patternWH;
  		cellW = cellH = 8;
  		supplementData = patternNameReg & 0x3FF;
  		auxMode = (patternNameReg & 0x4000) >> 14;
        }
        unsigned short colorCalc = readWord(reg, 0xEC);
	if (colorCalc & 0x1000) {
                alpha = ((~readWord(reg, 0x108) & 0x1F) << 3) + 0x7;
	}
	else {
		alpha = 0xFF;
	}

        colorOffset = readWord(reg, 0xE6) & 0x7;
        if (readWord(reg, 0x110) & 0x10) { // color offset enable
                if (readWord(reg, 0x112) & 0x10) { // color offset B
                        cor = readWord(reg, 0x11A) & 0xFF;
                        if (readWord(reg, 0x11A) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x11C) & 0xFF;
                        if (readWord(reg, 0x11C) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x11E) & 0xFF;
                        if (readWord(reg, 0x11E) & 0x100) cob = cob | 0xFFFFFF00;
		}
		else { // color offset A
                        cor = readWord(reg, 0x114) & 0xFF;
                        if (readWord(reg, 0x114) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x116) & 0xFF;
                        if (readWord(reg, 0x116) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x118) & 0xFF;
                        if (readWord(reg, 0x118) & 0x100) cob = cob | 0xFFFFFF00;
		}
	}
	else { // color offset disable
		cor = cog = cob = 0;
	}
 
        coordIncX = coordIncY = 1;
}

void RBG0::planeAddr(int i) {
	// works only for parameter A for time being
        unsigned long offset = (readWord(reg, 0x3E) & 0x7) << 6;
        unsigned long tmp=0;
  	switch(i) {
    		case 0: tmp = offset | reg->getByte(0x51); break;
    		case 1: tmp = offset | reg->getByte(0x50); break;
    		case 2: tmp = offset | reg->getByte(0x53); break;
   		case 3: tmp = offset | reg->getByte(0x52); break;
    		case 4: tmp = offset | reg->getByte(0x55); break;
    		case 5: tmp = offset | reg->getByte(0x54); break;
    		case 6: tmp = offset | reg->getByte(0x57); break;
   		case 7: tmp = offset | reg->getByte(0x56); break;
    		case 8: tmp = offset | reg->getByte(0x59); break;
    		case 9: tmp = offset | reg->getByte(0x58); break;
    		case 10: tmp = offset | reg->getByte(0x6B); break;
   		case 11: tmp = offset | reg->getByte(0x6A); break;
    		case 12: tmp = offset | reg->getByte(0x6D); break;
    		case 13: tmp = offset | reg->getByte(0x6C); break;
    		case 14: tmp = offset | reg->getByte(0x6F); break;
   		case 15: tmp = offset | reg->getByte(0x6E); break;
  	}
  	int deca = planeH + planeW - 2;
  	int multi = planeH * planeW;

  	if (patternDataSize == 1) {
		if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
	  	else addr = (tmp >> deca) * (multi * 0x800);
  	}
  	else {
		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
	  	else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
  	}
}

int RBG0::getPriority(void) {
        return (readWord(reg, 0xFC) & 0x7);
}

int RBG0::getInnerPriority(void) {
	return 4;
}

void RBG0::debugStats(char *outstring, bool *isenabled) {
}

void NBG0::init(void) {
        unsigned short patternNameReg = readWord(reg, 0x30);
        unsigned short patternReg = readWord(reg, 0x28);
	/* FIXME should start by checking if it's a normal
	 * or rotate scroll screen
	*/
        enable = readWord(reg, 0x20) & 0x1;
        transparencyEnable = !(readWord(reg, 0x20) & 0x100);
        x = - readWord(reg, 0x70);
        y = - readWord(reg, 0x74);

  	colorNumber = (patternReg & 0x70) >> 4;
	if(bitmap = patternReg & 0x2) {
		switch((patternReg & 0xC) >> 2) {
			case 0: cellW = 512;
				cellH = 256;
				break;
			case 1: cellW = 512;
				cellH = 512;
				break;
			case 2: cellW = 1024;
				cellH = 256;
				break;
			case 3: cellW = 1024;
				cellH = 512;
                                break;                                                           
		}
                charAddr = (readWord(reg, 0x3C) & 0x7) * 0x20000;
                palAddr = (readWord(reg, 0x2C) & 0x7) << 4;
		flipFunction = 0;
		specialFunction = 0;
  	}
  	else {
		mapWH = 2;
                switch(readWord(reg, 0x3A) & 0x3) {
  			case 0: planeW = planeH = 1; break;
  			case 1: planeW = 2; planeH = 1; break;
  			case 2: planeW = planeH = 2; break;
  		}
  		if(patternNameReg & 0x8000) patternDataSize = 1;
  		else patternDataSize = 2;
  		if(patternReg & 0x1) patternWH = 2;
  		else patternWH = 1;
  		pageWH = 64/patternWH;
  		cellW = cellH = 8;
  		supplementData = patternNameReg & 0x3FF;
  		auxMode = (patternNameReg & 0x4000) >> 14;
  	}
        unsigned short colorCalc = readWord(reg, 0xEC);
	if (colorCalc & 0x1) {
                alpha = ((~readWord(reg, 0x108) & 0x1F) << 3) + 0x7;
	}
	else {
		alpha = 0xFF;
	}

        colorOffset = readWord(reg, 0xE4) & 0x7;
        if (readWord(reg, 0x110) & 0x1) { // color offset enable
                if (readWord(reg, 0x112) & 0x1) { // color offset B
                        cor = readWord(reg, 0x11A) & 0xFF;
                        if (readWord(reg, 0x11A) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x11C) & 0xFF;
                        if (readWord(reg, 0x11C) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x11E) & 0xFF;
                        if (readWord(reg, 0x11E) & 0x100) cob = cob | 0xFFFFFF00;
		}
		else { // color offset A
                        cor = readWord(reg, 0x114) & 0xFF;
                        if (readWord(reg, 0x114) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x116) & 0xFF;
                        if (readWord(reg, 0x116) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x118) & 0xFF;
                        if (readWord(reg, 0x118) & 0x100) cob = cob | 0xFFFFFF00;
		}
	}
	else { // color offset disable
		cor = cog = cob = 0;
	}

        coordIncX = (float)(reg->getLong(0x78) & 0x7FF00) / 65536;
        coordIncY = (float)(reg->getLong(0x7C) & 0x7FF00) / 65536;
}

void NBG0::planeAddr(int i) {
        unsigned long offset = (readWord(reg, 0x3C) & 0x7) << 6;
        unsigned long tmp=0;
  	switch(i) {
    		case 0: tmp = offset | reg->getByte(0x41); break;
    		case 1: tmp = offset | reg->getByte(0x40); break;
    		case 2: tmp = offset | reg->getByte(0x43); break;
   		case 3: tmp = offset | reg->getByte(0x42); break;
  	}
  	int deca = planeH + planeW - 2;
  	int multi = planeH * planeW;
        //if (readWord(reg, 0x6) & 0x8000) {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
	  		else addr = (tmp >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
  		}
	/*}
	else {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x2000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0xF) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x3F) >> deca) * (multi * 0x1000);
  		}
	}*/
}

int NBG0::getPriority(void) {
  return reg->getByte(0xF9) & 0x7;
}

int NBG0::getInnerPriority(void) {
  return 3;
}

void NBG0::debugStats(char *outstring, bool *isenabled) {
  unsigned short screenDisplayReg = readWord(reg, 0x20);
  unsigned short mosaicReg = readWord(reg, 0x22);
  unsigned short lineVerticalScrollReg = readWord(reg, 0x9A) & 0x3F;
  unsigned long tmp=0;

  init();

  if (screenDisplayReg & 0x1 || screenDisplayReg & 0x20) {
     // enabled
     *isenabled = true;

     // Generate specific Info for NBG0/RBG1
     if (screenDisplayReg & 0x20)
     {
         sprintf(outstring, "RBG1 mode\r\n");
         outstring += strlen(outstring);
     }
     else
     {
         sprintf(outstring, "NBG0 mode\r\n");
         outstring += strlen(outstring);
     }

     // Mosaic
     if (mosaicReg & 0x1)
     {
        sprintf(outstring, "Mosaic Size = width %d height %d\r\n", (mosaicReg >> 8) & 0xf + 1, (mosaicReg >> 12) + 1);
        outstring += strlen(outstring);        
     }

     switch (colorNumber) {
        case 0:
                sprintf(outstring, "4-bit(16 colors)\r\n");
                break;
        case 1:
                sprintf(outstring, "8-bit(256 colors)\r\n");
                break;
        case 2:
                sprintf(outstring, "16-bit(2048 colors)\r\n");
                break;
        case 3:
                sprintf(outstring, "16-bit(32,768 colors)\r\n");
                break;
        case 4:
                sprintf(outstring, "32-bit(16.7 mil colors)\r\n");
                break;
        default:
                sprintf(outstring, "Unsupported BPP\r\n");
                break;
     }
     outstring += strlen(outstring);

     // Bitmap or Tile mode?(RBG1 can only do Tile mode)
     
     if (bitmap && !(screenDisplayReg & 0x20))
     {
        unsigned short bmpPalNumberReg = readWord(reg, 0x2C);

        // Bitmap
        sprintf(outstring, "Bitmap(%dx%d)\r\n", cellW, cellH);
        outstring += strlen(outstring);

        if (bmpPalNumberReg & 0x20)
        {
           sprintf(outstring, "Bitmap Special Priority enabled\r\n");
           outstring += strlen(outstring);
        }

        if (bmpPalNumberReg & 0x10)
        {
           sprintf(outstring, "Bitmap Special Color Calculation enabled\r\n");
           outstring += strlen(outstring);
        }

        sprintf(outstring, "Bitmap Address = %X\r\n", charAddr);
        outstring += strlen(outstring);

        sprintf(outstring, "Bitmap Palette Address = %X\r\n", palAddr);
        outstring += strlen(outstring);
     }
     else
     {
        // Tile
        sprintf(outstring, "Tile(%dH x %dV)\r\n", patternWH, patternWH);
        outstring += strlen(outstring);

        // Pattern Name Control stuff
        if (patternDataSize == 2) 
        {
           sprintf(outstring, "Pattern Name data size = 2 words\r\n");
           outstring += strlen(outstring);
        }
        else
        {
           sprintf(outstring, "Pattern Name data size = 1 word\r\n");
           outstring += strlen(outstring);
           sprintf(outstring, "Character Number Supplement bit = %d\r\n", (supplementData >> 16));
           outstring += strlen(outstring);
           sprintf(outstring, "Special Priority bit = %d\r\n", (supplementData >> 9) & 0x1);
           outstring += strlen(outstring);
           sprintf(outstring, "Special Color Calculation bit = %d\r\n", (supplementData >> 8) & 0x1);
           outstring += strlen(outstring);
           sprintf(outstring, "Supplementary Palette number = %d\r\n", (supplementData >> 5) & 0x7);
           outstring += strlen(outstring);
           sprintf(outstring, "Supplementary Color number = %d\r\n", supplementData & 0x1f);
           outstring += strlen(outstring);
        }

        // Figure out Cell start address
        switch(patternDataSize) {
           case 1: {
                   tmp = readWord(vram, addr);

                   switch(auxMode) {
                      case 0:
                         switch(patternWH) {
                            case 1:
                                charAddr = (tmp & 0x3FF) | ((supplementData & 0x1F) << 10);
				break;
                            case 2:
				charAddr = ((tmp & 0x3FF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x1C) << 10);
				break;
                         }
                         break;
                      case 1:
                         switch(patternWH) {
                         case 1:
				charAddr = (tmp & 0xFFF) | ((supplementData & 0x1C) << 10);
				break;
                         case 4:
                                charAddr = ((tmp & 0xFFF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x10) << 10);
                                break;
                         }
                         break;
                   }
                   break;
           }
           case 2: {
                   unsigned short tmp1 = readWord(vram, addr);
                   unsigned short tmp2 = readWord(vram, addr+2);

                   charAddr = tmp2 & 0x7FFF;
                   break;
           }
        }
        if (!(readWord(reg, 0x6) & 0x8000)) charAddr &= 0x3FFF;

        charAddr *= 0x20; // selon Runik
  
        sprintf(outstring, "Cell Data Address = %X\r\n", charAddr);
        outstring += strlen(outstring);
     }
     
     sprintf(outstring, "Plane Size = %dH x %dV\r\n", planeW, planeH);
     outstring += strlen(outstring);

     if (screenDisplayReg & 0x20)
     {
//        unsigned long mapOffsetReg=(readWord(reg, 0x3E) & 0x70) << 2;
        unsigned short rotParaControlReg=readWord(reg, 0xB2);

        // RBG1

        // Map Planes A-P here

        // Rotation Parameter Read Control
        if (rotParaControlReg & 0x400)
           sprintf(outstring, "Read KAst Parameter = TRUE\r\n");
        else
           sprintf(outstring, "Read KAst Parameter = FALSE\r\n");
        outstring += strlen(outstring);

        if (rotParaControlReg & 0x200)
           sprintf(outstring, "Read Yst Parameter = TRUE\r\n");
        else
           sprintf(outstring, "Read Yst Parameter = FALSE\r\n");
        outstring += strlen(outstring);

        if (rotParaControlReg & 0x100)
           sprintf(outstring, "Read Xst Parameter = TRUE\r\n");
        else
           sprintf(outstring, "Read Xst Parameter = FALSE\r\n");
        outstring += strlen(outstring);

        // Coefficient Table Control

        // Coefficient Table Address Offset

        // Screen Over Pattern Name(should this be moved?)

        // Rotation Parameter Table Address
     }
     else
     {
        // NBG0
        unsigned long mapOffsetReg=(readWord(reg, 0x3C) & 0x7) << 6;
        int deca = planeH + planeW - 2;
        int multi = planeH * planeW;

        if (!bitmap) {
           // Map Planes A-D
           for (int i=0; i < 4; i++) {
              tmp = mapOffsetReg | reg->getByte(0x40 + (i ^ 1));

              if (patternDataSize == 1) {
                 if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
                 else addr = (tmp >> deca) * (multi * 0x800);
              }
              else {
                 if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
                 else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
              }
  
              sprintf(outstring, "Plane %C Address = %08X\r\n", 0x41+i, addr);
              outstring += strlen(outstring);
           }
        }

        // Screen scroll values
        sprintf(outstring, "Screen Scroll x = %f, y = %f\r\n", (float)(reg->getLong(0x70) & 0x7FFFF00) / 65536, (float)(reg->getLong(0x74) & 0x7FFFF00) / 65536);
        outstring += strlen(outstring);

        // Coordinate Increments
        sprintf(outstring, "Coordinate Increments x = %f, y = %f\r\n", coordIncX, coordIncY);
        outstring += strlen(outstring);

        // Reduction Enable
        switch (readWord(reg, 0x98) & 3)
        {
           case 1:
                   sprintf(outstring, "Horizontal Reduction = 1/2\r\n");
                   outstring += strlen(outstring);
                   break;
           case 2:
           case 3:
                   sprintf(outstring, "Horizontal Reduction = 1/4\r\n");
                   outstring += strlen(outstring);
                   break;
           default: break;
        }

        switch (lineVerticalScrollReg >> 4)
        {
           case 0:
                   sprintf(outstring, "Line Scroll Interval = Each Line\r\n");
                   break;
           case 1:
                   sprintf(outstring, "Line Scroll Interval = Every 2 Lines\r\n");
                   break;
           case 2:
                   sprintf(outstring, "Line Scroll Interval = Every 4 Lines\r\n");
                   break;
           case 3:
                   sprintf(outstring, "Line Scroll Interval = Every 8 Lines\r\n");
                   break;
        }

        outstring += strlen(outstring);
   
        if (lineVerticalScrollReg & 0x8)
        {
           sprintf(outstring, "Line Zoom enabled\r\n");
           outstring += strlen(outstring);
        }

        if (lineVerticalScrollReg & 0x4)
        {
           sprintf(outstring, "Line Scroll Vertical enabled\r\n");
           outstring += strlen(outstring);
        }
   
        if (lineVerticalScrollReg & 0x2)
        {
           sprintf(outstring, "Line Scroll Horizontal enabled\r\n");
           outstring += strlen(outstring);
        }

        if (lineVerticalScrollReg & 0x6) 
        {
           sprintf(outstring, "Line Scroll Enabled\r\n");
           outstring += strlen(outstring);
           sprintf(outstring, "Line Scroll Table Address = %08X\r\n", 0x05E00000 + ((((readWord(reg, 0xA0) & 0x7) << 16) | (readWord(reg, 0xA2) & 0xFFFE)) << 1));
           outstring += strlen(outstring);
        }

        if (lineVerticalScrollReg & 0x1)
        {
           sprintf(outstring, "Vertical Cell Scroll enabled\r\n");
           outstring += strlen(outstring);
           sprintf(outstring, "Vertical Cell Scroll Table Address = %08X\r\n", 0x05E00000 + ((((readWord(reg, 0x9C) & 0x7) << 16) | (readWord(reg, 0x9E) & 0xFFFE)) << 1));
           outstring += strlen(outstring);
        }
     }

     // Window Control here

     // Shadow Control here

     // Color Ram Address Offset here

     // Special Priority Mode here

     // Color Calculation Control here

     // Special Color Calculation Mode here

     // Priority Number
     sprintf(outstring, "Priority = %d\r\n", getPriority());
     outstring += strlen(outstring);

     // Color Calculation here

     // Color Offset Enable here

     // Color Offset Select here
  }
  else {
     // disabled
     *isenabled = false;
  }
}

void NBG1::init(void) {
        unsigned short patternNameReg = readWord(reg, 0x32);
        unsigned short patternReg = readWord(reg, 0x28);

        enable = readWord(reg, 0x20) & 0x2;
        transparencyEnable = !(readWord(reg, 0x20) & 0x200);
        x = - readWord(reg, 0x80);
        y = - readWord(reg, 0x84);
	
  	colorNumber = (patternReg & 0x3000) >> 12;
  	if(bitmap = patternReg & 0x200) {
		switch((patternReg & 0xC00) >> 10) {
			case 0: cellW = 512;
				cellH = 256;
				break;
			case 1: cellW = 512;
				cellH = 512;
				break;
			case 2: cellW = 1024;
				cellH = 256;
				break;
			case 3: cellW = 1024;
				cellH = 512;
				break;
		}
                charAddr = ((readWord(reg, 0x3C) & 0x70) >> 4) * 0x20000;
                palAddr = (readWord(reg, 0x2C) & 0x700) >> 4;
		flipFunction = 0;
		specialFunction = 0;
  	}
  	else {
  		mapWH = 2;
                switch((readWord(reg, 0x3A) & 0xC) >> 2) {
  			case 0: planeW = planeH = 1; break;
  			case 1: planeW = 2; planeH = 1; break;
  			case 2: planeW = planeH = 2; break;
  		}
  		if(patternNameReg & 0x8000) patternDataSize = 1;
  		else patternDataSize = 2;
  		if(patternReg & 0x100) patternWH = 2;
  		else patternWH = 1;
  		pageWH = 64/patternWH;
  		cellW = cellH = 8;
  		supplementData = patternNameReg & 0x3FF;
  		auxMode = (patternNameReg & 0x4000) >> 14;
	}

        unsigned short colorCalc = readWord(reg, 0xEC);
	if (colorCalc & 0x2) {
                alpha = ((~readWord(reg, 0x108) & 0x1F00) >> 5) + 0x7;
	}
	else {
		alpha = 0xFF;
	}

        colorOffset = (readWord(reg, 0xE4) & 0x70) >> 4;
        if (readWord(reg, 0x110) & 0x2) { // color offset enable
                if (readWord(reg, 0x112) & 0x2) { // color offset B
                        cor = readWord(reg, 0x11A) & 0xFF;
                        if (readWord(reg, 0x11A) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x11C) & 0xFF;
                        if (readWord(reg, 0x11C) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x11E) & 0xFF;
                        if (readWord(reg, 0x11E) & 0x100) cob = cob | 0xFFFFFF00;
		}
		else { // color offset A
                        cor = readWord(reg, 0x114) & 0xFF;
                        if (readWord(reg, 0x114) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x116) & 0xFF;
                        if (readWord(reg, 0x116) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x118) & 0xFF;
                        if (readWord(reg, 0x118) & 0x100) cob = cob | 0xFFFFFF00;
		}
	}
	else { // color offset disable
		cor = cog = cob = 0;
	}

        coordIncX = (float)(reg->getLong(0x88) & 0x7FF00) / 65536;
        coordIncY = (float)(reg->getLong(0x8C) & 0x7FF00) / 65536;
}

void NBG1::planeAddr(int i) {
        unsigned long offset = (readWord(reg, 0x3C) & 0x70) << 2;
        unsigned long tmp=0;
  	switch(i) {
    		case 0: tmp = offset | reg->getByte(0x45); break;
    		case 1: tmp = offset | reg->getByte(0x44); break;
    		case 2: tmp = offset | reg->getByte(0x47); break;
    		case 3: tmp = offset | reg->getByte(0x46); break;
  	}
  	int deca = planeH + planeW - 2;
  	int multi = planeH * planeW;
        //if (readWord(reg, 0x6) & 0x8000) {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
	  		else addr = (tmp >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
  		}
	/*}
	else {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x2000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0xF) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x3F) >> deca) * (multi * 0x1000);
  		}
	}*/
}

int NBG1::getPriority(void) {
  return reg->getByte(0xF8) & 0x7;
}

int NBG1::getInnerPriority(void) {
  return 2;
}

void NBG1::debugStats(char *outstring, bool *isenabled) {
  unsigned short mosaicReg = readWord(reg, 0x22);
  unsigned short lineVerticalScrollReg = (readWord(reg, 0x9A) >> 8) & 0x3F;
  unsigned long mapOffsetReg=(readWord(reg, 0x3C) & 0x70) << 2;
  unsigned long tmp=0;
  int deca;
  int multi;

  init();

  if (enable) {
     // enabled
     *isenabled = true;

     // Mosaic
     if (mosaicReg & 0x2)
     {
        sprintf(outstring, "Mosaic Size = width %d height %d\r\n", (mosaicReg >> 8) & 0xf + 1, (mosaicReg >> 12) + 1);
        outstring += strlen(outstring);        
     }

     switch (colorNumber) {
        case 0:
                sprintf(outstring, "4-bit(16 colors)\r\n");
                break;
        case 1:
                sprintf(outstring, "8-bit(256 colors)\r\n");
                break;
        case 2:
                sprintf(outstring, "16-bit(2048 colors)\r\n");
                break;
        case 3:
                sprintf(outstring, "16-bit(32,768 colors)\r\n");
                break;
        case 4:
                sprintf(outstring, "32-bit(16.7 mil colors)\r\n");
                break;
        default:
                sprintf(outstring, "Unsupported BPP\r\n");
                break;
     }
     outstring += strlen(outstring);

     // Bitmap or Tile mode?     
     if (bitmap)
     {
        unsigned short bmpPalNumberReg = readWord(reg, 0x2C) >> 8;

        // Bitmap
        sprintf(outstring, "Bitmap(%dx%d)\r\n", cellW, cellH);
        outstring += strlen(outstring);

        if (bmpPalNumberReg & 0x20)
        {
           sprintf(outstring, "Bitmap Special Priority enabled\r\n");
           outstring += strlen(outstring);
        }

        if (bmpPalNumberReg & 0x10)
        {
           sprintf(outstring, "Bitmap Special Color Calculation enabled\r\n");
           outstring += strlen(outstring);
        }

        sprintf(outstring, "Bitmap Address = %X\r\n", charAddr);
        outstring += strlen(outstring);

        sprintf(outstring, "Bitmap Palette Address = %X\r\n", palAddr);
        outstring += strlen(outstring);
     }
     else
     {
        // Tile
        sprintf(outstring, "Tile(%dH x %dV)\r\n", patternWH, patternWH);
        outstring += strlen(outstring);

        // Pattern Name Control stuff
        if (patternDataSize == 2) 
        {
           sprintf(outstring, "Pattern Name data size = 2 words\r\n");
           outstring += strlen(outstring);
        }
        else
        {
           sprintf(outstring, "Pattern Name data size = 1 word\r\n");
           outstring += strlen(outstring);
           sprintf(outstring, "Character Number Supplement bit = %d\r\n", (supplementData >> 16));
           outstring += strlen(outstring);
           sprintf(outstring, "Special Priority bit = %d\r\n", (supplementData >> 9) & 0x1);
           outstring += strlen(outstring);
           sprintf(outstring, "Special Color Calculation bit = %d\r\n", (supplementData >> 8) & 0x1);
           outstring += strlen(outstring);
           sprintf(outstring, "Supplementary Palette number = %d\r\n", (supplementData >> 5) & 0x7);
           outstring += strlen(outstring);
           sprintf(outstring, "Supplementary Color number = %d\r\n", supplementData & 0x1f);
           outstring += strlen(outstring);
        }

        deca = planeH + planeW - 2;
        multi = planeH * planeW;

        // Map Planes A-D
        for (int i=0; i < 4; i++) {
           tmp = mapOffsetReg | reg->getByte(0x44 + (i ^ 1));

           if (patternDataSize == 1) {
              if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
              else addr = (tmp >> deca) * (multi * 0x800);
           }
           else {
              if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
              else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
           }

           sprintf(outstring, "Plane %C Address = %08X\r\n", 0x41+i, addr);
           outstring += strlen(outstring);
        }

        // Figure out Cell start address
        switch(patternDataSize) {
           case 1: {
                   tmp = readWord(vram, addr);

                   switch(auxMode) {
                      case 0:
                         switch(patternWH) {
                            case 1:
                                charAddr = (tmp & 0x3FF) | ((supplementData & 0x1F) << 10);
				break;
                            case 2:
				charAddr = ((tmp & 0x3FF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x1C) << 10);
				break;
                         }
                         break;
                      case 1:
                         switch(patternWH) {
                         case 1:
				charAddr = (tmp & 0xFFF) | ((supplementData & 0x1C) << 10);
				break;
                         case 4:
                                charAddr = ((tmp & 0xFFF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x10) << 10);
                                break;
                         }
                         break;
                   }
                   break;
           }
           case 2: {
                   unsigned short tmp1 = readWord(vram, addr);
                   unsigned short tmp2 = readWord(vram, addr+2);

                   charAddr = tmp2 & 0x7FFF;
                   break;
           }
        }
        if (!(readWord(reg, 0x6) & 0x8000)) charAddr &= 0x3FFF;

        charAddr *= 0x20; // selon Runik
  
        sprintf(outstring, "Cell Data Address = %X\r\n", charAddr);
        outstring += strlen(outstring);
     }
     
     sprintf(outstring, "Plane Size = %dH x %dV\r\n", planeW, planeH);
     outstring += strlen(outstring);

     // Screen scroll values
     sprintf(outstring, "Screen Scroll x = %f, y = %f\r\n", (float)(reg->getLong(0x80) & 0x7FFFF00) / 65536, (float)(reg->getLong(0x84) & 0x7FFFF00) / 65536);
     outstring += strlen(outstring);

     // Coordinate Increments
     sprintf(outstring, "Coordinate Increments x = %f, y = %f\r\n", coordIncX, coordIncY);
     outstring += strlen(outstring);

     // Reduction Enable
     switch ((readWord(reg, 0x98) >> 8) & 3)
     {
        case 1:
                sprintf(outstring, "Horizontal Reduction = 1/2\r\n");
                outstring += strlen(outstring);
                break;
        case 2:
        case 3:
                sprintf(outstring, "Horizontal Reduction = 1/4\r\n");
                outstring += strlen(outstring);
                break;
        default: break;
     }

     switch (lineVerticalScrollReg >> 4)
     {
        case 0:
                sprintf(outstring, "Line Scroll Interval = Each Line\r\n");
                break;
        case 1:
                sprintf(outstring, "Line Scroll Interval = Every 2 Lines\r\n");
                break;
        case 2:
                sprintf(outstring, "Line Scroll Interval = Every 4 Lines\r\n");
                break;
        case 3:
                sprintf(outstring, "Line Scroll Interval = Every 8 Lines\r\n");
                break;
     }

     outstring += strlen(outstring);

     if (lineVerticalScrollReg & 0x8)
     {
        sprintf(outstring, "Line Zoom X enabled\r\n");
        outstring += strlen(outstring);
     }

     if (lineVerticalScrollReg & 0x4)
     {
        sprintf(outstring, "Line Scroll Vertical enabled\r\n");
        outstring += strlen(outstring);
     }
   
     if (lineVerticalScrollReg & 0x2)
     {
        sprintf(outstring, "Line Scroll Horizontal enabled\r\n");
        outstring += strlen(outstring);
     }

     if (lineVerticalScrollReg & 0x6) 
     {
        sprintf(outstring, "Line Scroll Enabled\r\n");
        outstring += strlen(outstring);
        sprintf(outstring, "Line Scroll Table Address = %08X\r\n", 0x05E00000 + ((((readWord(reg, 0xA4) & 0x7) << 16) | (readWord(reg, 0xA6) & 0xFFFE)) << 1));
        outstring += strlen(outstring);
     }

     if (lineVerticalScrollReg & 0x1)
     {
        sprintf(outstring, "Vertical Cell Scroll enabled\r\n");
        outstring += strlen(outstring);
        sprintf(outstring, "Vertical Cell Scroll Table Address = %08X\r\n", 0x05E00000 + ((((readWord(reg, 0x9C) & 0x7) << 16) | (readWord(reg, 0x9E) & 0xFFFE)) << 1));
        outstring += strlen(outstring);
     }

     // Window Control here

     // Shadow Control here

     // Color Ram Address Offset here

     // Special Priority Mode here

     // Color Calculation Control here

     // Special Color Calculation Mode here

     // Priority Number
     sprintf(outstring, "Priority = %d\r\n", getPriority());
     outstring += strlen(outstring);

     // Color Calculation here

     // Color Offset Enable here

     // Color Offset Select here
  }
  else {
     // disabled
     *isenabled = false;
  }
}

void NBG2::init(void) {
        unsigned short patternNameReg = readWord(reg, 0x34);
        unsigned short patternReg = readWord(reg, 0x2A);

        enable = readWord(reg, 0x20) & 0x4;
        transparencyEnable = !(readWord(reg, 0x20) & 0x400);
        x = - readWord(reg, 0x90);
        y = - readWord(reg, 0x92);

  	colorNumber = (patternReg & 0x2) >> 1;
	bitmap = false; // NBG2 can only use cell mode
	
	mapWH = 2;
        switch((readWord(reg, 0x3A) & 0x30) >> 4) {
  		case 0: planeW = planeH = 1; break;
  		case 1: planeW = 2; planeH = 1; break;
  		case 2: planeW = planeH = 2; break;
  	}
  	if(patternNameReg & 0x8000) patternDataSize = 1;
  	else patternDataSize = 2;
  	if(patternReg & 0x1) patternWH = 2;
  	else patternWH = 1;
  	pageWH = 64/patternWH;
  	cellW = cellH = 8;
  	supplementData = patternNameReg & 0x3FF;
  	auxMode = (patternNameReg & 0x4000) >> 14;
        unsigned short colorCalc = readWord(reg, 0xEC);
	if (colorCalc & 0x4) {
                alpha = ((~readWord(reg, 0x10A) & 0x1F) << 3) + 0x7;
	}
	else {
		alpha = 0xFF;
	}

        colorOffset = (readWord(reg, 0xE4) & 0x700) >> 8;
        if (readWord(reg, 0x110) & 0x4) { // color offset enable
                if (readWord(reg, 0x112) & 0x4) { // color offset B
                        cor = readWord(reg, 0x11A) & 0xFF;
                        if (readWord(reg, 0x11A) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x11C) & 0xFF;
                        if (readWord(reg, 0x11C) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x11E) & 0xFF;
                        if (readWord(reg, 0x11E) & 0x100) cob = cob | 0xFFFFFF00;
		}
		else { // color offset A
                        cor = readWord(reg, 0x114) & 0xFF;
                        if (readWord(reg, 0x114) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x116) & 0xFF;
                        if (readWord(reg, 0x116) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x118) & 0xFF;
                        if (readWord(reg, 0x118) & 0x100) cob = cob | 0xFFFFFF00;
		}
	}
	else { // color offset disable
		cor = cog = cob = 0;
	}

        coordIncX = coordIncY = 1;
}

void NBG2::planeAddr(int i) {
        unsigned long offset = (readWord(reg, 0x3C) & 0x700) >> 2;
        unsigned long tmp=0;
  	switch(i) {
    		case 0: tmp = offset | reg->getByte(0x49); break;
    		case 1: tmp = offset | reg->getByte(0x48); break;
    		case 2: tmp = offset | reg->getByte(0x4B); break;
    		case 3: tmp = offset | reg->getByte(0x4A); break;
  	}
  	int deca = planeH + planeW - 2;
  	int multi = planeH * planeW;

        //if (readWord(reg, 0x6) & 0x8000) {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
	  		else addr = (tmp >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
  		}
	/*}
	else {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) {
				cerr << "ici 1" << endl;
				addr = ((tmp & 0x1F) >> deca) * (multi * 0x2000);
			}
	  		else {
				cerr << "ici 2" << endl;
				addr = ((tmp & 0x7F) >> deca) * (multi * 0x800);
			}
  		}
  		else {
	  		if (patternWH == 1) {
				cerr << "ici 3" << endl;
				addr = ((tmp & 0xF) >> deca) * (multi * 0x4000);
			}
	  		else {
				cerr << "ici 4" << endl;
				addr = ((tmp & 0x3F) >> deca) * (multi * 0x1000);
			}
  		}
	}*/
}

int NBG2::getPriority(void) {
  return reg->getByte(0xFB) & 0x7;
}

int NBG2::getInnerPriority(void) {
  return 1;
}

void NBG2::debugStats(char *outstring, bool *isenabled) {
  unsigned short mosaicReg = readWord(reg, 0x22);
  unsigned long mapOffsetReg=(readWord(reg, 0x3C) & 0x700) >> 2;
  unsigned long tmp=0;
  int deca;
  int multi;

  init();

  if (enable) {
     // enabled
     *isenabled = true;

     // Mosaic
     if (mosaicReg & 0x4)
     {
        sprintf(outstring, "Mosaic Size = width %d height %d\r\n", (mosaicReg >> 8) & 0xf + 1, (mosaicReg >> 12) + 1);
        outstring += strlen(outstring);        
     }

     switch (colorNumber) {
        case 0:
                sprintf(outstring, "4-bit(16 colors)\r\n");
                break;
        case 1:
                sprintf(outstring, "8-bit(256 colors)\r\n");
                break;
        case 2:
                sprintf(outstring, "16-bit(2048 colors)\r\n");
                break;
        case 3:
                sprintf(outstring, "16-bit(32,768 colors)\r\n");
                break;
        case 4:
                sprintf(outstring, "32-bit(16.7 mil colors)\r\n");
                break;
        default:
                sprintf(outstring, "Unsupported BPP\r\n");
                break;
     }
     outstring += strlen(outstring);

     sprintf(outstring, "Tile(%dH x %dV)\r\n", patternWH, patternWH);
     outstring += strlen(outstring);

     // Pattern Name Control stuff
     if (patternDataSize == 2) 
     {
        sprintf(outstring, "Pattern Name data size = 2 words\r\n");
        outstring += strlen(outstring);
     }
     else
     {
        sprintf(outstring, "Pattern Name data size = 1 word\r\n");
        outstring += strlen(outstring);
        sprintf(outstring, "Character Number Supplement bit = %d\r\n", (supplementData >> 16));
        outstring += strlen(outstring);
        sprintf(outstring, "Special Priority bit = %d\r\n", (supplementData >> 9) & 0x1);
        outstring += strlen(outstring);
        sprintf(outstring, "Special Color Calculation bit = %d\r\n", (supplementData >> 8) & 0x1);
        outstring += strlen(outstring);
        sprintf(outstring, "Supplementary Palette number = %d\r\n", (supplementData >> 5) & 0x7);
        outstring += strlen(outstring);
        sprintf(outstring, "Supplementary Color number = %d\r\n", supplementData & 0x1f);
        outstring += strlen(outstring);
     }
     
     sprintf(outstring, "Plane Size = %dH x %dV\r\n", planeW, planeH);
     outstring += strlen(outstring);

     deca = planeH + planeW - 2;
     multi = planeH * planeW;

     // Map Planes A-D
     for (int i=0; i < 4; i++) {
        tmp = mapOffsetReg | reg->getByte(0x48 + (i ^ 1));

        if (patternDataSize == 1) {
           if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
           else addr = (tmp >> deca) * (multi * 0x800);
        }
        else {
           if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
           else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
        }

        sprintf(outstring, "Plane %C Address = %08X\r\n", 0x41+i, addr);
        outstring += strlen(outstring);
     }

     // Figure out Cell start address
     switch(patternDataSize) {
        case 1: {
                tmp = readWord(vram, addr);

    		switch(auxMode) {
    		case 0:
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0x3FF) | ((supplementData & 0x1F) << 10);
				break;
      			case 2:
				charAddr = ((tmp & 0x3FF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x1C) << 10);
				break;
      			}
      			break;
    		case 1:
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0xFFF) | ((supplementData & 0x1C) << 10);
				break;
      			case 4:
				charAddr = ((tmp & 0xFFF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x10) << 10);
				break;
      			}
      			break;
    		}
    		break;
	}
  	case 2: {
                unsigned short tmp1 = readWord(vram, addr);
                unsigned short tmp2 = readWord(vram, addr+2);

    		charAddr = tmp2 & 0x7FFF;
    		break;
	}
     }
     if (!(readWord(reg, 0x6) & 0x8000)) charAddr &= 0x3FFF;

     charAddr *= 0x20; // selon Runik

     sprintf(outstring, "Cell Data Address = %X\r\n", charAddr);
     outstring += strlen(outstring);

     // Screen scroll values
     sprintf(outstring, "Screen Scroll x = %d, y = %d\r\n", readWord(reg, 0x90) & 0x7FF, readWord(reg, 0x92) & 0x7FF);
     outstring += strlen(outstring);
 
     // Window Control here

     // Shadow Control here

     // Color Ram Address Offset here

     // Special Priority Mode here

     // Color Calculation Control here

     // Special Color Calculation Mode here

     // Priority Number
     sprintf(outstring, "Priority = %d\r\n", getPriority());
     outstring += strlen(outstring);

     // Color Calculation here

     // Color Offset Enable here

     // Color Offset Select here
  }
  else {
     // disabled
     *isenabled = false;
  }
}

void NBG3::init(void) {
        unsigned short patternNameReg = readWord(reg, 0x36);
        unsigned short patternReg = readWord(reg, 0x2A);

        enable = readWord(reg, 0x20) & 0x8;
        transparencyEnable = !(readWord(reg, 0x20) & 0x800);
        x = - readWord(reg, 0x94);
        y = - readWord(reg, 0x96);

  	colorNumber = (patternReg & 0x20) >> 5;
	bitmap = false; // NBG2 can only use cell mode
	
	mapWH = 2;
        switch((readWord(reg, 0x3A) & 0xC0) >> 6) {
  		case 0: planeW = planeH = 1; break;
  		case 1: planeW = 2; planeH = 1; break;
  		case 2: planeW = planeH = 2; break;
  	}
  	if(patternNameReg & 0x8000) patternDataSize = 1;
  	else patternDataSize = 2;
  	if(patternReg & 0x10) patternWH = 2;
  	else patternWH = 1;
  	pageWH = 64/patternWH;
  	cellW = cellH = 8;
  	supplementData = patternNameReg & 0x3FF;
  	auxMode = (patternNameReg & 0x4000) >> 14;
        unsigned short colorCalc = readWord(reg, 0xEC);
	if (colorCalc & 0x8) {
                alpha = ((~readWord(reg, 0x10A) & 0x1F00) >> 5) + 0x7;
	}
	else {
		alpha = 0xFF;
	}

        colorOffset = (readWord(reg, 0xE4) & 0x7000) >> 12;
        if (readWord(reg, 0x110) & 0x8) { // color offset enable
                if (readWord(reg, 0x112) & 0x8) { // color offset B
                        cor = readWord(reg, 0x11A) & 0xFF;
                        if (readWord(reg, 0x11A) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x11C) & 0xFF;
                        if (readWord(reg, 0x11C) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x11E) & 0xFF;
                        if (readWord(reg, 0x11E) & 0x100) cob = cob | 0xFFFFFF00;
		}
		else { // color offset A
                        cor = readWord(reg, 0x114) & 0xFF;
                        if (readWord(reg, 0x114) & 0x100) cor = cor | 0xFFFFFF00;
                        cog = readWord(reg, 0x116) & 0xFF;
                        if (readWord(reg, 0x116) & 0x100) cog = cog | 0xFFFFFF00;
                        cob = readWord(reg, 0x118) & 0xFF;
                        if (readWord(reg, 0x118) & 0x100) cob = cob | 0xFFFFFF00;
		}
	}
	else { // color offset disable
		cor = cog = cob = 0;
	}

        coordIncX = coordIncY = 1;
}

void NBG3::planeAddr(int i) {
        unsigned long offset = (readWord(reg, 0x3C) & 0x7000) >> 6;
        unsigned long tmp=0;
  	switch(i) {
    		case 0: tmp = offset | reg->getByte(0x4D); break;
    		case 1: tmp = offset | reg->getByte(0x4C); break;
    		case 2: tmp = offset | reg->getByte(0x4F); break;
    		case 3: tmp = offset | reg->getByte(0x4E); break;
  	}
  	int deca = planeH + planeW - 2;
  	int multi = planeH * planeW;

        //if (readWord(reg, 0x6) & 0x8000) {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
	  		else addr = (tmp >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
  		}
	/*}
	else {
  		if (patternDataSize == 1) {
	  		if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x2000);
	  		else addr = ((tmp & 0x7F) >> deca) * (multi * 0x800);
  		}
  		else {
	  		if (patternWH == 1) addr = ((tmp & 0xF) >> deca) * (multi * 0x4000);
	  		else addr = ((tmp & 0x3F) >> deca) * (multi * 0x1000);
  		}
	}*/
}

int NBG3::getPriority(void) {
	return (reg->getByte(0xFA) & 0x7);
}

int NBG3::getInnerPriority(void) {
	return 0;
}

void NBG3::debugStats(char *outstring, bool *isenabled) {
  unsigned short mosaicReg = readWord(reg, 0x22);
  unsigned long mapOffsetReg=(readWord(reg, 0x3C) & 0x7000) >> 6;
  unsigned long tmp=0;
  int deca;
  int multi;

  init();

  if (enable) {
     // enabled
     *isenabled = true;

     // Mosaic
     if (mosaicReg & 0x8)
     {
        sprintf(outstring, "Mosaic Size = width %d height %d\r\n", (mosaicReg >> 8) & 0xf + 1, (mosaicReg >> 12) + 1);
        outstring += strlen(outstring);        
     }

     switch (colorNumber) {
        case 0:
                sprintf(outstring, "4-bit(16 colors)\r\n");
                break;
        case 1:
                sprintf(outstring, "8-bit(256 colors)\r\n");
                break;
        case 2:
                sprintf(outstring, "16-bit(2048 colors)\r\n");
                break;
        case 3:
                sprintf(outstring, "16-bit(32,768 colors)\r\n");
                break;
        case 4:
                sprintf(outstring, "32-bit(16.7 mil colors)\r\n");
                break;
        default:
                sprintf(outstring, "Unsupported BPP\r\n");
                break;
     }
     outstring += strlen(outstring);

     sprintf(outstring, "Tile(%dH x %dV)\r\n", patternWH, patternWH);
     outstring += strlen(outstring);

     // Pattern Name Control stuff
     if (patternDataSize == 2) 
     {
        sprintf(outstring, "Pattern Name data size = 2 words\r\n");
        outstring += strlen(outstring);
     }
     else
     {
        sprintf(outstring, "Pattern Name data size = 1 word\r\n");
        outstring += strlen(outstring);
        sprintf(outstring, "Character Number Supplement bit = %d\r\n", (supplementData >> 16));
        outstring += strlen(outstring);
        sprintf(outstring, "Special Priority bit = %d\r\n", (supplementData >> 9) & 0x1);
        outstring += strlen(outstring);
        sprintf(outstring, "Special Color Calculation bit = %d\r\n", (supplementData >> 8) & 0x1);
        outstring += strlen(outstring);
        sprintf(outstring, "Supplementary Palette number = %d\r\n", (supplementData >> 5) & 0x7);
        outstring += strlen(outstring);
        sprintf(outstring, "Supplementary Color number = %d\r\n", supplementData & 0x1f);
        outstring += strlen(outstring);
     }
     
     sprintf(outstring, "Plane Size = %dH x %dV\r\n", planeW, planeH);
     outstring += strlen(outstring);

     deca = planeH + planeW - 2;
     multi = planeH * planeW;

     // Map Planes A-D
     for (int i=0; i < 4; i++) {
        tmp = mapOffsetReg | reg->getByte(0x4C + (i ^ 1));

        if (patternDataSize == 1) {
           if (patternWH == 1) addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
           else addr = (tmp >> deca) * (multi * 0x800);
        }
        else {
           if (patternWH == 1) addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
           else addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
        }

        sprintf(outstring, "Plane %C Address = %08X\r\n", 0x41+i, addr);
        outstring += strlen(outstring);
     }

     // Figure out Cell start address
     switch(patternDataSize) {
        case 1: {
                tmp = readWord(vram, addr);

    		switch(auxMode) {
    		case 0:
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0x3FF) | ((supplementData & 0x1F) << 10);
				break;
      			case 2:
				charAddr = ((tmp & 0x3FF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x1C) << 10);
				break;
      			}
      			break;
    		case 1:
      			switch(patternWH) {
      			case 1:
				charAddr = (tmp & 0xFFF) | ((supplementData & 0x1C) << 10);
				break;
      			case 4:
				charAddr = ((tmp & 0xFFF) << 2) |  (supplementData & 0x3) | ((supplementData & 0x10) << 10);
				break;
      			}
      			break;
    		}
    		break;
	}
  	case 2: {
                unsigned short tmp1 = readWord(vram, addr);
                unsigned short tmp2 = readWord(vram, addr+2);

    		charAddr = tmp2 & 0x7FFF;
    		break;
	}
     }
     if (!(readWord(reg, 0x6) & 0x8000)) charAddr &= 0x3FFF;

     charAddr *= 0x20; // selon Runik

     sprintf(outstring, "Cell Data Address = %X\r\n", charAddr);
     outstring += strlen(outstring);

     // Screen scroll values
     sprintf(outstring, "Screen Scroll x = %d, y = %d\r\n", readWord(reg, 0x94) & 0x7FF, readWord(reg, 0x96) & 0x7FF);
     outstring += strlen(outstring);
 
     // Window Control here

     // Shadow Control here

     // Color Ram Address Offset here

     // Special Priority Mode here

     // Color Calculation Control here

     // Special Color Calculation Mode here

     // Priority Number
     sprintf(outstring, "Priority = %d\r\n", getPriority());
     outstring += strlen(outstring);

     // Color Calculation here

     // Color Offset Enable here

     // Color Offset Select here
  }
  else {
     // disabled
     *isenabled = false;
  }
}

/****************************************/
/*					*/
/*		VDP2 			*/
/*					*/
/****************************************/

Vdp2::Vdp2(SaturnMemory *v) : Memory(0x1FF, 0x200) {
  satmem = v;
  vram = new Vdp2Ram;
  cram = new Vdp2ColorRam;

  satmem->vdp1_2->setVdp2Ram(this, cram);

  SDL_InitSubSystem(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 4 );
  SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 4 );
  SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 4 );
  SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 4 );
  SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
  SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

  SDL_SetVideoMode(320,224,32, SDL_OPENGL);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //glOrtho(-1, 1, -1, 1, -10, 10);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 320, 224, 0, 1, 0);
#ifndef _arch_dreamcast
  surface = new unsigned long [1024 * 512];
#else
  surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 1024, 512, 16, 0x000f, 0x00f0, 0x0f00, 0xf000);
  free(surface->pixels);
  surface->pixels = memalign(32, 1024 * 512 * 2);
#endif
  screens[5] = satmem->vdp1_2;
  screens[4] = rbg0 = new RBG0(this, vram, cram, surface);
  screens[3] = nbg0 = new NBG0(this, vram, cram, surface);
  screens[2] = nbg1 = new NBG1(this, vram, cram, surface);
  screens[1] = nbg2 = new NBG2(this, vram, cram, surface);
  screens[0] = nbg3 = new NBG3(this, vram, cram, surface);

  setSaturnResolution(320, 224);

  reset();

  fps = 0;
  frameCount = 0;
  ticks = 0;
  fpstoggle = false;
}

Vdp2::~Vdp2(void) {
    for(int i = 0;i < 6;i++) {
	    if (screens[i] != satmem->vdp1_2)
	    	delete screens[i];
    }
    delete [] surface;
    delete vram;
    delete cram;
}

void Vdp2::reset(void) {
  setWord(0x0, 0);
  setWord(0x4, 0); //setWord(0x4, 0x302);
  setWord(0x6, 0);
  setWord(0xE, 0);
  setWord(0x20, 0);

  // clear Vram here
}

Vdp2ColorRam *Vdp2::getCRam(void) {
	return cram;
}

Vdp2Ram *Vdp2::getVRam(void) {
	return vram;
}

void Vdp2::VBlankIN(void) {
        setWord(0x4, getWord(0x4) | 0x0008);
	satmem->scu->sendVBlankIN();

        if (satmem->sshRunning)
           ((SuperH *) satmem->getSlaveSH())->send(Interrupt(0x6, 0x43));
}

void Vdp2::HBlankIN(void) {
        setWord(0x4, getWord(0x4) | 0x0004);
        satmem->scu->sendHBlankIN();

        if (satmem->sshRunning)
           ((SuperH *) satmem->getSlaveSH())->send(Interrupt(0x2, 0x41));
}

void Vdp2::HBlankOUT(void) {
        setWord(0x4, getWord(0x4) & 0xFFFB);
}

void Vdp2::VBlankOUT(void) {
  setWord(0x4, getWord(0x4) & 0xFFF7 | 0x0002);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (getWord(0) & 0x8000) {
    drawBackScreen();
    screens[0]->draw();
    screens[1]->draw();
    screens[2]->draw();
    screens[3]->draw();
    screens[4]->draw();
    screens[5]->draw();
  }

  if (fpstoggle) {
     //onScreenDebugMessage(-0.9, -0.85, "%02d/60 FPS", fps);
     onScreenDebugMessage(10, 214, "%02d/60 FPS", fps);

     frameCount++;
     if(SDL_GetTicks() >= ticks + 1000) {
        fps = frameCount;
        frameCount = 0;
        ticks = SDL_GetTicks();
     }
  }

#ifdef _arch_dreamcast
  glKosBeginFrame();
#endif

  //((Vdp1 *) satmem->getVdp1())->execute(0);
  glFlush();
#ifndef _arch_dreamcast
  SDL_GL_SwapBuffers();
#else
  glKosFinishFrame();
#endif
  //colorOffset();
  satmem->scu->sendVBlankOUT();
}

VdpScreen *Vdp2::getScreen(int i) {
  return screens[i];
}

void Vdp2::sortScreens(void) {
  qsort(screens, 6, sizeof(VdpScreen *), &VdpScreen::comparePriority);
}

void Vdp2::updateRam(void) {
  cram->setMode((getWord(0xE) >> 12) & 0x3);
}

void Vdp2::drawBackScreen(void) {
	unsigned long BKTAU = getWord(0xAC);
	unsigned long BKTAL = getWord(0xAE);
	unsigned long scrAddr;

	if (getWord(0x6) & 0x8000)
		scrAddr = (((BKTAU & 0x7) << 16) | BKTAL) * 2;
	else
		scrAddr = (((BKTAU & 0x3) << 16) | BKTAL) * 2;

	unsigned short dot;
	if (BKTAU & 0x8000) {
		int y;
		glBegin(GL_LINES);
		for(y = -112;y < 112;y++) {
			dot = vram->getWord(scrAddr);
			scrAddr += 2;
			glColor3ub(((dot & 0x1F) << 3), ((dot & 0x3E0) >> 2), ((dot & 0x7C00) >> 7));
			glVertex2f(0, y);
			glVertex2f(320, y);
		}
		glEnd();
		glColor3ub(0xFF, 0xFF, 0xFF);
	}
	else {
		dot = vram->getWord(scrAddr);
		glColor3ub(((dot & 0x1F) << 3), ((dot & 0x3E0) >> 2), ((dot & 0x7C00) >> 7));
		glBegin(GL_QUADS);
		glVertex2i(0, 0);
		glVertex2i(320, 0);
		glVertex2i(320, 224);
		glVertex2i(0, 224);
		glEnd();
		glColor3ub(0xFF, 0xFF, 0xFF);
	}
}

void Vdp2::priorityFunction(void) {
}

/*
void Vdp2::colorOffset(void) {
  SDL_Surface *tmp1, *tmp2;

  tmp1 = SDL_CreateRGBSurface(SDL_HWSURFACE, 400, 400, 16, 0, 0, 0, 0);
  tmp2 = SDL_CreateRGBSurface(SDL_HWSURFACE, 400, 400, 16, 0, 0, 0, 0);

  SDL_FillRect(tmp2, NULL, SDL_MapRGB(tmp2->format, getWord(0x114),
			  			    getWord(0x116),
						    getWord(0x118)));
  SDL_BlitSurface(surface, NULL, tmp1, NULL);

  SDL_imageFilterAdd((unsigned char *) tmp1->pixels,
		     (unsigned char *) tmp2->pixels,
		     (unsigned char *) surface->pixels, surface->pitch);

  SDL_FreeSurface(tmp1);
  SDL_FreeSurface(tmp2);
}
*/

VdpScreen *Vdp2::getRBG0(void) {
   return rbg0;
}

VdpScreen *Vdp2::getNBG0(void) {
   return nbg0;
}

VdpScreen *Vdp2::getNBG1(void) {
   return nbg1;
}

VdpScreen *Vdp2::getNBG2(void) {
   return nbg2;
}            

VdpScreen *Vdp2::getNBG3(void) {
   return nbg3;
}

void Vdp2::setSaturnResolution(int width, int height) {
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0, width, height, 0, 1, 0);
   ((RBG0 *)rbg0)->setTextureRatio(width, height);
   ((NBG0 *)nbg0)->setTextureRatio(width, height);
   ((NBG1 *)nbg1)->setTextureRatio(width, height);
   ((NBG2 *)nbg2)->setTextureRatio(width, height);
   ((NBG3 *)nbg3)->setTextureRatio(width, height);
   satmem->vdp1_2->setTextureSize(width, height);
}

void Vdp2::setActualResolution(int width, int height) {
}

void Vdp2::onScreenDebugMessage(float x, float y, char *string, ...) {
  va_list arglist;
  char tempstr[512];

  va_start(arglist, string);
  vsprintf(tempstr, string, arglist);
  va_end(arglist);

#ifndef _arch_dreamcast
  glColor3f(0.01f, 0.01f, 0.01f);
  glRasterPos2f(x+0.012, y-0.0145);
  for (int i=0; i < strlen(tempstr); i++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, tempstr[i]);
  }

  glColor3f(1.0f, 0.0f, 0.0f);
  glRasterPos2f(x, y);
  for (int i=0; i < strlen(tempstr); i++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, tempstr[i]);
  }

  glColor3f(1.0f, 1.0f, 1.0f);
#endif
}

void Vdp2::toggleFPS(void) {
   fpstoggle ^= true;
}
