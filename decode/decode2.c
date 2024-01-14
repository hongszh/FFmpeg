#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libavcodec/avcodec.h>

#define INBUF_SIZE 4096

/*
 * 说明
 * 这是一个用avcodec_send_packet，avcodec_receive_frame解码的例子，
 * 解码后将frame直接释放。
 */

int main(int argc, char **argv)
{
    AVCodec *codec;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *pkt;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    int ret, len;
    int fd;

    if (argc <= 1) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        exit(1);
    }

    // Open input file
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Could not open input file '%s': %s\n", argv[1], strerror(errno));
        exit(1);
    }

    // Allocate codec context
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
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

    // Read input file and decode
    while (1) {
        // Read input data
        len = read(fd, inbuf, INBUF_SIZE);
        if (len < 0) {
            fprintf(stderr, "Error while reading input file: %s\n", strerror(errno));
            exit(1);
        }
        if (len == 0) {
            break;
        }

        // Send packet to decoder
        pkt->data = inbuf;
        pkt->size = len;
        ret = avcodec_send_packet(codec_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error sending packet to decoder: %s\n", av_err2str(ret));
            exit(1);
        }

        // Receive decoded frame from decoder
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                fprintf(stderr, "Error receiving frame from decoder: %s\n", av_err2str(ret));
                exit(1);
            }

            // Process decoded frame
            printf("Decoded frame: width=%d, height=%d, format=%d\n", frame->width, frame->height, frame->format);

            // Free frame memory
            av_frame_unref(frame);
        }
    }

    // Free packet and frame memory
    av_packet_free(&pkt);
    av_frame_free(&frame);

    // Close codec and codec context
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);

    // Close input file
    close(fd);

    return 0;
}

// build
// gcc -o decode-2 decode-2.c -lavformat -lavcodec -lavutil -lswscale -L/usr/lib/x86_64-linux-gnu/ -lpng16
// ./decode-2 ~/video/256x144.mp4