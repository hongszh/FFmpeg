#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define INBUF_SIZE 4096

/*
 * 说明
 * 这是一个用avcodec_send_packet，avcodec_receive_frame解码的例子，
 * 解码后的buffer保存为yuv文件，没用sws_scale，如果解码不是yuv420p，
 * 应该是要用sws_scale，不然保存文件的代码不能工作。
 */

int main(int argc, char *argv[]) {
    AVCodec *codec;
    AVCodecContext *codec_ctx = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int ret, i;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        exit(1);
    }

    // Register all codecs and formats
    // av_register_all();

    // Open input file
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", argv[1]);
        exit(1);
    }

    // Retrieve stream information
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    // Find the first video stream
    int video_stream_idx = -1;
    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }
    if (video_stream_idx == -1) {
        fprintf(stderr, "Could not find video stream\n");
        exit(1);
    }

    // Get a pointer to the codec context for the video stream
    codec_ctx = avcodec_alloc_context3(NULL);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
        exit(1);
    }
    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);

    // Find the decoder for the video stream
    codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec\n");
        exit(1);
    }

    // Open codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    // Allocate packet and frame
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        exit(1);
    }

    // Open output file
    FILE *outfile = fopen(argv[2], "wb");
    if (!outfile) {
        fprintf(stderr, "Could not open output file '%s'\n", argv[2]);
        exit(1);
    }

    // Initialize conversion context
    struct SwsContext *sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                                codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P,
                                                0, NULL, NULL, NULL);

    // Read packets from input file and decode them
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_idx) {
            // Send packet to decoder

            if (pkt->flags & AV_PKT_FLAG_KEY) {
                av_log(NULL, AV_LOG_INFO, " in   is key frame!\n");
            } else {
                av_log(NULL, AV_LOG_INFO, " in is't key frame!\n");
            }

            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                exit(1);
            }

            // Receive frame from decoder
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error receiving frame from decoder\n");
                    exit(1);
                }

                if (frame->pict_type == AV_PICTURE_TYPE_I) {
                    av_log(NULL, AV_LOG_INFO, "  out   is I frame!\n");
                } else if (frame->pict_type == AV_PICTURE_TYPE_P) {
                    av_log(NULL, AV_LOG_INFO, "  out   is P frame!\n");
                } else if (frame->pict_type == AV_PICTURE_TYPE_B) {
                    av_log(NULL, AV_LOG_INFO, "  out   is B frame!\n");
                }

                // Write YUV data to output file
                fwrite(frame->data[0], 1, codec_ctx->width * codec_ctx->height, outfile);
                fwrite(frame->data[1], 1, codec_ctx->width * codec_ctx->height / 4, outfile);
                fwrite(frame->data[2], 1, codec_ctx->width * codec_ctx->height / 4, outfile);
            }
        }
        av_packet_unref(pkt);
    }

    // Flush decoder
    ret = avcodec_send_packet(codec_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error flushing decoder\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error receiving frame from decoder\n");
            exit(1);
        }

        // Write YUV data to output file
        fwrite(frame->data[0], 1, codec_ctx->width * codec_ctx->height, outfile);
        fwrite(frame->data[1], 1, codec_ctx->width * codec_ctx->height / 4, outfile);
        fwrite(frame->data[2], 1, codec_ctx->width * codec_ctx->height / 4, outfile);
    }

    // Clean up
    fclose(outfile);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

}

// build
// gcc -o decode-3 decode-3.c -lavformat -lavcodec -lavutil -lswscale -L/usr/lib/x86_64-linux-gnu/ -lpng16
// gcc -g -o decode-3 decode-3.c -lavformat -lavcodec -lavutil -lswscale -L/usr/lib/x86_64-linux-gnu/ -lpng16
// ./decode-3 ~/video/256x144.mp4 256x144-yuv420p.yuv
// ffplay -f rawvideo -video_size 256x144 256x144-yuv420p.yuv