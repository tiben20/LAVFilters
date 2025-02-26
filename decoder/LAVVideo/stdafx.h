/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// pre-compiled header

#pragma once

// Support for Version 6.0 styles
#pragma comment( \
    linker,      \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include "common_defines.h"

// include headers
#include <Windows.h>
#include <Commctrl.h>

#include <atlbase.h>
#include <atlconv.h>

#include <d3d9.h>
#include <dxva2api.h>
#include <dvdmedia.h>

#pragma warning(push)
#pragma warning(disable : 4244)
extern "C"
{
#define __STDC_CONSTANT_MACROS
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/cpu.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
}
#pragma warning(pop)

#include "streams.h"

#include "DShowUtil.h"
#include "growarray.h"

#include "SubRenderIntf.h"

#define REF_SECOND_MULT 10000000LL
