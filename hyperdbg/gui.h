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

#ifndef _GUI_H
#define _GUI_H

extern UCHAR out_matrix[OUT_SIZE_Y][OUT_SIZE_X];

VOID VideoUpdateShell(PUCHAR buffer);
VOID VideoInitShell(VOID);
VOID VideoDrawFrame(VOID);
VOID VideoPrintHeader(VOID);
VOID VideoResetOutMatrix(VOID);
VOID VideoResetOutMatrixCache(VOID);
VOID VideoRefreshOutArea(unsigned int);

#endif	/* _GUI_H */
