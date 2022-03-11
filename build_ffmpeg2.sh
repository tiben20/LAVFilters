#!/bin/sh
arch=x64
archdir=x64
cross_prefix=x86_64-w64-mingw32-

delete_files() (
  rm ./libavcodec/dxva2.o
  rm ./libavcodec/dxva2.d
  #rm ./libavcodec/dxva2_vp9.o
  #rm ./libavcodec/dxva2_vp9.d
  #rm ./libavcodec/dxva2_hevc.o
  #rm ./libavcodec/dxva2_av1.o
  #rm ./libavcodec/dxva2_hevc.d
  #rm ./libavcodec/dxva2_av1.d
  #rm ./libavcodec/dxva2_vc1.o
  #rm ./libavcodec/dxva2_mpeg2.o
  rm ./libavcodec/dxva2_h264.o
  #rm ./libavcodec/dxva2_vc1.d
  #rm ./libavcodec/dxva2_mpeg2.d
  rm ./libavcodec/dxva2_h264.d
  rm ./libavcodec/d3d11va.o
  rm ./libavcodec/d3d11va.d
  rm ./libavcodec/d3d12va.o
  rm ./libavcodec/d3d12va.d
  rm ./libavutil/hwcontext_d3d11va.d
  rm ./libavutil/hwcontext_d3d12va.o
  rm ./libavutil/hwcontext_d3d12va.d
  rm ./libavutil/hwcontext_d3d11va.o
  #rm ./libavcodec/decode.o
  #rm ./libavcodec/decode.d
  rm ./libavutil/hwcontext.d
  rm ./libavutil/hwcontext.o
)

copy_libs() (
  # install -s --strip-program=${cross_prefix}strip lib*/*-lav-*.dll ../bin_${archdir}
  cp lib*/*-lav-*.dll ../bin_${archdir}
  ${cross_prefix}strip ../bin_${archdir}/*-lav-*.dll
  cp -u lib*/*.lib ../bin_${archdir}/lib

  cp lib*/*-lav-*.dll ../bin_${archdir}d
  ${cross_prefix}strip ../bin_${archdir}d/*-lav-*.dll
  cp -u lib*/*.lib ../bin_${archdir}d/lib
)

build() (
  make -j12
)

echo
echo rebuild dxva fast
echo

cd ffmpeg

delete_files &&
build &&
copy_libs

cd ..
