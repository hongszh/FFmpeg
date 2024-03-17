#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <wels/codec_api.h>
#include <wels/codec_app_def.h>

#define OPENH264_VER_AT_LEAST(maj, min) \
  ((OPENH264_MAJOR > (maj)) ||          \
      (OPENH264_MAJOR == (maj) && OPENH264_MINOR >= (min)))

void openh264_trace_callback(void *ctx, int level, const char *msg);

void openh264_trace_callback(void *ctx, int level, const char *msg) {
  fprintf(stderr, "%s\n", msg);
}

int main(int argc, char *argv[]) {
  int opt;
  int ret;
  uint8_t* addr;
  uint8_t *pouts[4] = {NULL};
  char *input_file;
  char *output_file;
  AVPacket *avpkt;
  AVFormatContext *format_ctx;
  int video_stream_index;
  FILE *out_fp;
  OpenH264Version libver;
  ISVCDecoder *decoder;
  SDecodingParam decParam;
  int log_level = WELS_LOG_RESV;
  int frame_count = 0;
  WelsTraceCallback callback_function;

  while ((opt = getopt(argc, argv, ":")) != -1) {
    switch (opt) {
      default:
        fprintf(stderr, "Usage: %s input.mp4 output.yuv\n", argv[0]);
        return 0;
    }
  }

  if (optind + 2 != argc) {
    fprintf(stderr, "Usage: %s input.mp4 output.yuv\n", argv[0]);
    return 0;
  }

  input_file  = argv[optind];
  output_file = argv[optind + 1];

  av_log_set_level(AV_LOG_TRACE);

  avpkt = av_packet_alloc();
  if (!avpkt) {
    fprintf(stderr, "Could not allocate packet\n");
    exit(1);
  }

  /* Init FFmpeg context */

  format_ctx = NULL;
  format_ctx = avformat_alloc_context();
  ret = avformat_open_input(&format_ctx, input_file, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error opening input file: %s\n", av_err2str(ret));
    return 1;
  }

  ret = avformat_find_stream_info(format_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error finding stream information: %s\n", av_err2str(ret));
    return 1;
  }

  /* Find the first video stream */
  video_stream_index = -1;
  for (int i = 0; i < format_ctx->nb_streams; i++) {
    if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      fprintf(stderr, "find video index %d\n", video_stream_index);
      break;
    }
  }

  if (video_stream_index == -1) {
      fprintf(stderr, "Could not find video stream\n");
      exit(1);
  }

  /* Open output file  */

  out_fp = fopen(output_file, "wb");
  if (!out_fp) {
    fprintf(stderr, "Failed to open output file: %s\n", output_file);
    return 0;
  }

   /* Init openh264 codec context  */

  libver = WelsGetCodecVersion();
  fprintf(stderr, "openH264 version %d.%d\n", libver.uMajor, libver.uMinor);

  memset(&decParam, 0, sizeof(SDecodingParam));
  decParam.eEcActiveIdc = ERROR_CON_DISABLE;
  decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

  WelsCreateDecoder(&decoder);
  if (!decoder) {
    fprintf(stderr, "Failed to create decoder\n");
    fclose(out_fp);
    return 0;
  }

  callback_function = openh264_trace_callback;
  (*decoder)->SetOption(decoder, DECODER_OPTION_TRACE_LEVEL, &log_level);
  (*decoder)->SetOption(decoder, DECODER_OPTION_TRACE_CALLBACK, (void *)&callback_function);

  if ((*decoder)->Initialize(decoder, &decParam) != cmResultSuccess) {
    fprintf(stderr, "Failed to initialize decoder\n");
    WelsDestroyDecoder(decoder);
    fclose(out_fp);
    return 0;
  }

  /* Read packets from the input file and decode them */

  while (av_read_frame(format_ctx, avpkt) >= 0) {
    if (avpkt->stream_index == video_stream_index) {
      SBufferInfo frame_info = {0};
      DECODING_STATE state;

      if (avpkt->flags & AV_PKT_FLAG_KEY) {
        av_log(NULL, AV_LOG_INFO, "pakcet is key frame!\n");
      }

      if (!avpkt->size) {
#if OPENH264_VER_AT_LEAST(1, 9)
        int end_of_stream = 1;
        (*decoder)->SetOption(decoder, DECODER_OPTION_END_OF_STREAM, &end_of_stream);
        state = (*decoder)->FlushFrame(decoder, pouts, &info);
#endif
      } else {

        /* Decode the packet */

        frame_info.uiInBsTimeStamp = avpkt->pts;
#if OPENH264_VER_AT_LEAST(1, 4)
        state = (*decoder)->DecodeFrameNoDelay(decoder, avpkt->data, avpkt->size, pouts, &frame_info);
#else
        state = (*decoder)->DecodeFrame2(decoder, avpkt->data, avpkt->size, pouts, &frame_info);
#endif
        if (state != dsErrorFree) {
          fprintf(stderr, "decode failed 0x%04x\n", state);
          exit(1);
        }

        if (frame_info.iBufferStatus != 1) {
          fprintf(stderr, "Packet size %d, No frame produced\n", avpkt->size);
        }

        if (frame_info.iBufferStatus == 1) {
          /* frame_info.UsrData.sSystemBuffer.iFormat: videoFormatI420 */
          if (frame_info.UsrData.sSystemBuffer.iFormat != videoFormatI420) {
            fprintf(stderr, "output format is not videoFormatI420\n");
            exit(1);
          }

#if 1
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

            addr = pouts[plane];
            for (int row = 0; row < height; row++) {
              fwrite(addr, width, 1, out_fp);
              addr += stride;
            }
          } /* write yuv data */
#else
          int width   = frame_info.UsrData.sSystemBuffer.iWidth;
          int height  = frame_info.UsrData.sSystemBuffer.iHeight;
          int strideY = frame_info.UsrData.sSystemBuffer.iStride[0];
          int strideU = frame_info.UsrData.sSystemBuffer.iStride[1];
          int strideV = frame_info.UsrData.sSystemBuffer.iStride[1];

          uint8_t *y = pouts[0];
          uint8_t *u = pouts[1];
          uint8_t *v = pouts[2];

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
#endif
          frame_count++;
        } /* frame_info.iBufferStatus == 1 */
      } /* avpkt->size != 0 */
    }

    av_packet_unref(avpkt);
  }

  fclose(out_fp);

  (*decoder)->Uninitialize(decoder);
  WelsDestroyDecoder(decoder);

  avformat_close_input(&format_ctx);

  printf("Decoded %d frames\n", frame_count);

  return 0;
}
