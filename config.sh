../configure \
--disable-asm \
--disable-optimizations \
--enable-avcodec \
--enable-avdevice \
--enable-avfilter \
--enable-avformat \
--enable-decoder='aac,aac_latm,ac3,flac,h264,libopus,mp3,pcm_s*,rawvideo,libfluoride_sbc,libfluoride_sbc_packed,vorbis,vp8' \
--enable-demuxer='aac,ac3,flac,hls,mp3,mp4,mpegts*,mov,ogg,pcm_s*,sbc,wav,w64' \
--enable-encoder='libopus,mpeg4,pcm_s*,rawvideo,libfluoride_sbc,wavpack' \
--enable-indev='fbdev,nuttx,v4l2,bluelet' \
--enable-filter='acopy,adevsrc,adevsink,afade,afifo,aformat,amix,amovie_async,amoviesink_async,aresample,asplit,astreamselect,scale,volume' \
--enable-libopus \
--enable-muxer='mp4,pcm_s*,ogg,rawvideo,sbc,wav,opus' \
--enable-outdev='fbdev,nuttx,bluelet' \
--enable-parser='aac,ac3,flac,gif,opus,png,vorbis,webp,sbc' \
--enable-protocol='cache,data,file,ftp,hls,http*,pipe,rtp,tcp,tls,unix' \
--enable-bsf='a2dp_rechunk' \
--enable-swresample \
--enable-decoder=hwh264 \
--enable-swscale \
--enable-libx264 \
--enable-libopenh264 \
--enable-gpl \
--enable-zlib

# link static lib
# --extra-ldflags=-L/home/hui/vela/ffmpeg-5.1.1/ffmpeg/lc3 --extra-libs=-llc3 \
# --disable-decoder="hwh264" \
