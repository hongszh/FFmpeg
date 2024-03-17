

为了快速熟悉openH264解码，写了个小程序，使用FFmpeg API读取文件，将读取的packet送给openH264解码器，使用openh264 API解码后，生成yuv文件，最后可以用ffplay播放生成的yuv文件，已确定解码是正确的。

## 编译安装libopenh264

```bash
git clone https://github.com/cisco/openh264

cd openh264
meson builddir
cd builddir
ninja
sudo ninja install
```

> `make OS=linux`编译install后，编译代码没问题，但是decoder不能工作，所以用ffmpeg命令验证了下，libopenh264确实有问题，meson+ninja编译后安装的就没有问题，可能是没指定arch参数，这个没有进一步验证。

## ffmpeg

ffmpeg使用libopenh264解码命令

```bash

ffmpeg -vcodec libopenh264 -i 256x144.mp4 -f rawvideo out.yuv -y -v 56

```

## ffmpeg生成h264文件

> 不加h264_mp4toannexb参数也可以，生成的都是annexb格式

```bash
ffmpeg -i 256x144.mp4 -c:v copy -bsf:v h264_mp4toannexb -f h264 256x144.h264
```


## 编译本例程序

```bash

# 查找include和ld路径
$ pkg-config --libs --cflags openh264
-I/usr/local/include -L/usr/lib/x86_64-linux-gnu -lopenh264

$ pkg-config --libs --cflags libpng16
-I/usr/local/include/libpng16 -L/usr/local/lib/x86_64-linux-gnu -lpng16 -lz

# 设置环境变量
export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# 编译
> -lpng16不是依赖项，我的环境比较复杂依赖这个
gcc -O0 -g -o openh264dec openh264dec.c -lavformat -lavcodec -lavutil -lopenh264 -lpng16

# 运行
./openh264dec 256x144.h264 256x144-yuv420p.yuv

```

这里如果输入256x144.mp4文件会报错，原因是openh264需要annexb格式，而从mp4中读取的packet没有经过bitstream filter处理，所以解码会报错：

```bash
$ ./openh264dec 256x144.mp4 256x144-yuv420p.yuv
find video index 0
openH264 version 2.3
[OpenH264] this = 0x0x558200c33740, Info:CWelsDecoder::SetOption():DECODER_OPTION_TRACE_CALLBACK callback = 0x5581ffca83d7.
[OpenH264] this = 0x0x558200c33740, Info:CWelsDecoder::init_decoder(), openh264 codec version = openh264 default: 1.4, ParseOnly = 0
[OpenH264] this = 0x0x558200c33740, Info:CWelsDecoder::init_decoder(), openh264 codec version = openh264 default: 1.4, ParseOnly = 0
[OpenH264] this = 0x0x558200c33740, Info:eVideoType: 1
pakcet is key frame!
[OpenH264] this = 0x0x558200c33740, Info:decode failed, failure type:4
```

所以，换成annexb格式的h264流文件解码正常：
```bash
./openh264dec 256x144.h264 256x144-yuv420p.yuv
```

## 播放yuv文件

```bash
ffplay -video_size 256x144 -pixel_format yuv420p 256x144-yuv420p.yuv
```

需要注意的是，openH264中，SBufferInfo返回的stride信息比较特殊，
```c
typedef struct TagSysMemBuffer {
  int iWidth;                    ///< width of decoded pic for display
  int iHeight;                   ///< height of decoded pic for display
  int iFormat;                   ///< type is "EVideoFormatType"
  int iStride[2];                ///< stride of 2 component
} SSysMEMBuffer;
```

因为输出默认是I420，对于Y分量，对应`iStride[0]`，对于UV分量都是`iStride[1]`。

写yuv数据到文件：

```c
  for (int plane = 0; plane < 3; plane++) {
    int width  = frame_info.UsrData.sSystemBuffer.iWidth;
    int height = frame_info.UsrData.sSystemBuffer.iHeight;
    int stride = frame_info.UsrData.sSystemBuffer.iStride[0];

    if (plane > 0) {
      /* UV plane stride is iStride[1] */
      width  = width / 2;
      height = height / 2;
      stride = frame_info.UsrData.sSystemBuffer.iStride[1];
    }

    uint8_t* addr = ptrs[plane];
    for (int row = 0; row < height; row++) {
      fwrite(addr, width, 1, out_fp);
      addr += stride;
    }
  } /* write yuv data */
```

或者分开写：

```c
  int width   = frame_info.UsrData.sSystemBuffer.iWidth;
  int height  = frame_info.UsrData.sSystemBuffer.iHeight;
  int strideY = frame_info.UsrData.sSystemBuffer.iStride[0];
  int strideU = frame_info.UsrData.sSystemBuffer.iStride[1];
  int strideV = frame_info.UsrData.sSystemBuffer.iStride[1];

  uint8_t *y = ptrs[0];
  uint8_t *u = ptrs[1];
  uint8_t *v = ptrs[2];

  for (int i = 0; i < height; i++) {
    fwrite(y, 1, width, out_fp);
    y += strideY;
  }

  for (int i = 0; i < height / 2; i++) {
    fwrite(u, 1, width / 2, out_fp);
    u += strideU;
  }

  for (int i = 0; i < height / 2; i++) {
    fwrite(v, 1, width / 2, out_fp);
    v += strideV;
  }
```

