## Copyright (C) 2012-2020 Philip Nienhuis
## 
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <http://www.gnu.org/licenses/>.

## __JXL_spsh_close__ - internal function: close a spreadsheet file using JXL

## Author: Philip Nienhuis <prnienhuis@users.sf.net>
## Created: 2012-10-12

function [ xls ] = __JXL_spsh_close__ (xls)

    if (xls.changed > 0 && xls.changed < 3)
      try
        xls.workbook.write ();
        xls.workbook.close ();
        if (xls.changed == 3)
          ## Upon entering write mode, JExcelAPI always resets disk file.
          ## Incomplete new files (no data added) had better be deleted.
          xls.workbook.close ();
          delete (xls.filename); 
        endif
        xls.changed = 0;
      catch
      end_try_catch
    endif

endfunction
