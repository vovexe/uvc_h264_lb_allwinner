/*
 * Copyright (c) 2020 Yuri Bobrov <ubobrov@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed as is in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sched.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <linux/videodev2.h>

#include "ve.h"
#include "video_device.h"
#include "h264enc.h"
#include "csc.h"
#include "overlay.h"

#define USE_V4L_DEV
#define USE_FPS_MEASUREMENT
//#define USE_OVERLAY
#define USE_POLL_INSTEAD_OF_SELECT

#if defined(USE_OVERLAY)
#define LEFT_CORNER     0
#define RIGHT_CORNER(w,l)    (w)-((l)*SG_W)
#define CENTER(w,l)          ((w)/2)-((l)*SG_W/2)
#endif
/*********************** H3 measurement ******************
CSC NEON YUV 4:2:2 => NV12
    640x480 = 1ms
    1280x720 = 3ms
    1920x1080 = 7ms

H264 encode
    640x480 = 4ms
    1280x720 = 12ms
    1920x1080 = 27ms
**********************************************************/

#define DEF_VIDEO_DEV 	"/dev/video0"
#define DEF_VIDEO_H		640
#define DEF_VIDEO_W		480
#define DEF_PIX_FMT		"UYVY"

#define LB_DRV_NAME 	"v4l2loopback"
#define LB_NAME_OFFSET	8 // starts with /dev/videoN(offset)

static char VIDEO_DEV[20] = DEF_VIDEO_DEV;


#define N_LB_DEV    1
/* graphic thread init struct */
static struct pthr_start {
    char *lb_name;
    int lb_codec;
    int lb_fd;
    int lb_nbuf;
    int lb_w;
    int lb_h;
    void *lb_pbuf;
    int tofile;
    int file_fd;
    char *fname;
    int pix_format;
} th_start[] = {
    {
        .lb_name = "/dev/video8",
        .lb_codec = H264_LB,
        .lb_w = -1,
        .lb_h = -1,
        .tofile = 0,
        .file_fd = -1,
        .pix_format = V4L2_PIX_FMT_H264,
    },
    {
        .lb_name = "/dev/video9",
        .lb_codec = H264_LB,
        .lb_w = -1,
        .lb_h = -1,
        .tofile = 0,
        .file_fd = -1,
        .fname = "out_sunxi_tst.mkv",
        .pix_format = V4L2_PIX_FMT_YUV420,
    }
};


# ifndef MAX
#  define MAX(x, y) ( ((x)>(y))?(x):(y) )
# endif

# ifndef MIN
#  define MIN(x, y) ( ((x)<(y))?(x):(y) )
# endif
/*
*/
unsigned long int time_diff(struct timespec *ts1, struct timespec *ts2) {
    static struct timespec ts;
    ts.tv_sec = MAX(ts2->tv_sec, ts1->tv_sec) - MIN(ts2->tv_sec, ts1->tv_sec);
    ts.tv_nsec = MAX(ts2->tv_nsec, ts1->tv_nsec) - MIN(ts2->tv_nsec, ts1->tv_nsec);

    if (ts.tv_sec > 0) {
        ts.tv_sec--;
        ts.tv_nsec += 1000000000;
    }

    return((ts.tv_sec * 1000000000) + ts.tv_nsec);
}


/*
 *
 */
static int read_frame(int fd, void *buffer, int size) {
	int total = 0, len;
	while (total < size)
	{
		len = read(fd, buffer + total, size - total);
		if (len == 0)
			return 0;
		total += len;
	}
	return 1;
}

/*
 *
 */
int main(const int argc, const char **argv) {
	int nframes_ps = 0, sfps = 0;
	int in = -1, out = -1;
	int size_out;
	char input_file[50] = "";
	char output_file[50] = "";
	int height, width;
	int video_fd;
	struct buffer *buffers;
	static int n_buffers;
	/* V4L2 */
	enum v4l2_buf_type type;
	struct v4l2_buffer buf;
	int i, cnt;
	char mod_param[128];
	int opt;
    unsigned int frame_rate = 30;
	int cap_dev_pix_fmt =  v4l2_fourcc(DEF_PIX_FMT[0], DEF_PIX_FMT[1], DEF_PIX_FMT[2], DEF_PIX_FMT[3]);
    int measurement_en = 0;

	width = DEF_VIDEO_W;
	height = DEF_VIDEO_H;

    if (argc < 2) {
        printf("Usage: %s -v videodev -i input file -o output file -w width -h height -f format\n", argv[0]);
        exit(0);
    }

	while ((opt = getopt(argc, argv, "v:i:o:w:h:f:m:")) != -1) {
        switch (opt) {
            case 'v':
                strcpy(VIDEO_DEV, optarg);
                break;
            case 'i':
                strcpy(input_file, optarg);
                break;
            case 'o':
                strcpy(output_file, optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;   
            case 'h':
                height = atoi(optarg);
                break; 
            case 'f':
                cap_dev_pix_fmt = v4l2_fourcc(optarg[0], optarg[1], optarg[2], optarg[3]);
                break;             
            case 'm':
                measurement_en = atoi(optarg);
                break;

            default:
                printf("Usage: %s -v videodev -i input file -o output file -w width -h height -f format\n", argv[0]);
                exit(0);
                break;    
        }
    }

    if (strlen(input_file) > 0) {
	    if (strcmp(input_file, "-") != 0) {
			if ((in = open(argv[1], O_RDONLY)) == -1) {
				printf("could not open input file\n");
				return EXIT_FAILURE;
			}
		} else {
			in = 0;
		}
	}

	if (strlen(output_file) > 0) {
		if ((out = open(argv[4], O_CREAT | O_RDWR | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
			printf("could not open output file\n");
			return EXIT_FAILURE;
		}
	}

#if defined(USE_V4L_DEV)
	open_capture_dev(VIDEO_DEV, &video_fd);
	if (video_fd < 0) {
		errno_exit("open video device");
	}

	if (dev_try_format(video_fd,  width, height, cap_dev_pix_fmt)) {
		printf("Incompattible capture pixel format %c%c%c%c!\n", FOURCC_PRINTF_PARMS(cap_dev_pix_fmt));
		close(video_fd);
        goto app_exit;
	}

    setup_capture_device(VIDEO_DEV, video_fd, &width, &height, 30, cap_dev_pix_fmt);
    
    frame_rate = 30;
    v4l2_set_fps(&video_fd, &frame_rate);
    printf("Device %s set frame rate %dfps\n", VIDEO_DEV, frame_rate);

    buffers = init_capt_mmap(VIDEO_DEV, video_fd, &n_buffers);
#endif	

    h264enc *encoder = NULL;
    void *output_buf = NULL;
    void *input_buf = NULL;

    if (cap_dev_pix_fmt != V4L2_PIX_FMT_H264) {
    	struct h264enc_params params;
        if (width == 800) height = 560;
    	params.src_width = (width + 15) & ~15;
    	params.width = width;
    	params.src_height = (height + 15) & ~15;
    	params.height = height;
    	params.src_format = H264_FMT_NV12;
    	params.profile_idc = 77;
    	params.level_idc = 41;
    	params.entropy_coding_mode = H264_EC_CABAC;
    	params.qp = 24;
    	params.keyframe_interval = 25;
    	params.work_mode = ENC_MODE_STREAMING;

    	if (!ve_open()) {
    		printf("Failed to open CedarX device %s\n", "/dev/cedar_dev");
    		return EXIT_FAILURE;
    	}

    	encoder = h264enc_new(&params);

    	if (encoder == NULL) {
    		printf("could not create encoder\n");
    		goto err;
    	} else {
    		printf("H264 encoder initialized: %dx%d\n", width, height);
    	}

    	output_buf = h264enc_get_bytestream_buffer(encoder);

    	int input_size = params.src_width * (params.src_height + params.src_height / 2);
    	input_buf = h264enc_get_input_buffer(encoder);
    	size_out = ((width) * (height) * 12 / 8);

    	if (in > 0 && out > 0) {
    		printf("Runnig h264 encoding from file %s...\n", input_file);
    		while (read_frame(in, input_buf, input_size)) {
    			if (h264enc_encode_picture(encoder)) {
    				write(out, output_buf, h264enc_get_bytestream_length(encoder));
    			} else {
    				printf("encoding error\n");
    			}
    		}
    		printf("Done!\n");
    		goto complete;
    	}
    }

#if defined(USE_V4L_DEV)
	printf("Runnig h264 encoding from V4L device %s...\n", VIDEO_DEV);
	// rmmod
    remove_mod(LB_DRV_NAME);
    cnt = sprintf(mod_param, "video_nr=");
    for (i = 0;i < N_LB_DEV;i++) {
        if (i != 0) {
            strcat(mod_param, ",");
            cnt++;
        }
        cnt += sprintf(mod_param + cnt, "%d", i + LB_NAME_OFFSET);
    }
    // insmod
    init_mod("//usr//lib//"LB_DRV_NAME".ko", mod_param);

    for (i = 0;i < N_LB_DEV;i++) {
    	th_start[i].lb_w = width;
        th_start[i].lb_h = height;
    	open_out_dev(th_start[i].lb_name, width, height, th_start[i].lb_codec, &th_start[i].lb_fd, th_start[i].pix_format);
    	th_start[i].lb_pbuf = init_out_mmap(&th_start[i].lb_fd, &th_start[i].lb_nbuf);

    	if (!th_start[i].lb_pbuf) {
            printf("No buffers for %s\n", th_start[i].lb_name);
            exit(EXIT_FAILURE);
        }

        /* open the file for writing codec bitstream */
        if (th_start[i].tofile == 1) {
            th_start[i].file_fd = open(th_start[i].fname, O_WRONLY | O_CREAT | O_TRUNC, 0755);
            if (-1 == th_start[i].file_fd) {
                printf("Failed to open file for writing %s\n", th_start[i].fname);
                exit(EXIT_FAILURE);
            }
        }
    }

    /* start capture */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(video_fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");

#if defined(USE_OVERLAY)
    sw_overlay_nv12_init();
#endif    
    
    while (1) {
        fd_set fds;
        struct timeval tv;
        int r;
#ifdef USE_FPS_MEASUREMENT
        struct timespec tm[2];
        clock_gettime(CLOCK_MONOTONIC, &tm[0]);
#endif

#ifndef USE_POLL_INSTEAD_OF_SELECT
        FD_ZERO(&fds);
        FD_SET(video_fd, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(video_fd + 1, &fds, NULL, NULL, &tv);
        if (-1 == r) {
            if (EINTR == errno)  
                continue;
            
            errno_exit("select");
        }
        if (0 == r) {
            fprintf(stderr, "select timeout\n");
            exit(EXIT_FAILURE);
        }
#else
        r = v4l2_poll(video_fd);
        if(r < 0) {
            if (EINTR == errno)  
                continue;
            errno_exit("poll");
        }
#endif        
        /* dequeue captured buffer */
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (-1 == xioctl(video_fd, VIDIOC_DQBUF, &buf)) {
            if (errno == EAGAIN) {
                //printf("EAGAIN\n");
                continue;
            }
            errno_exit("VIDIOC_DQBUF");
        }
        assert(buf.index < n_buffers);

        for (i = 0;i < N_LB_DEV;i++) {
        	int len = 0;
        	void *pb = NULL;

            /* just copy to loopback if input is H264 */
        	if (th_start[i].lb_codec == H264_LB) {  
                if (cap_dev_pix_fmt == V4L2_PIX_FMT_H264) {
                    pb = buffers[buf.index].start;
                    len = buf.bytesused;
                    wrt_to_lpbck(th_start[i].lb_fd, 
                                  pb,
                                  len, 
                                  n_buffers, 
                                  th_start[i].lb_pbuf);
                } else {
#if defined(CPU_HAS_NEON) 		        	
        			int src_stride = width*2;
        			int dst_stride_y = width;
        			int dst_stride_uv = width;
        			int uv_offset = width*height;
#endif        			        	 
        		    if (cap_dev_pix_fmt == V4L2_PIX_FMT_UYVY) {		
#if defined(CPU_HAS_NEON) 		        	
        			    UYVYToNV12_neon(buffers[buf.index].start, src_stride,
		               		   input_buf, dst_stride_y,
		               		   input_buf + uv_offset, dst_stride_uv,
		                       width, height);
#else
        			    uyvy422toNV12(width, height, buffers[buf.index].start, input_buf);
#endif        			
		    	     } else if (cap_dev_pix_fmt == V4L2_PIX_FMT_YUYV) {
#if defined(CPU_HAS_NEON) 
		    		    YUYVToNV12_neon(buffers[buf.index].start, src_stride,
		               		   input_buf, dst_stride_y,
		               		   input_buf + uv_offset, dst_stride_uv,
		                       width, height);
#else		    		
		    		    yuyv422toNV12(width, height, buffers[buf.index].start, input_buf);
#endif
		    	    } else if (cap_dev_pix_fmt == V4L2_PIX_FMT_NV12) {
                        memcpy(input_buf, buffers[buf.index].start, buf.bytesused);
                    } else {
		    		    continue;
		    	    }

#if defined(USE_OVERLAY)
                    {
                        char buf[100]= {0};
                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
                        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                        tm.tm_hour, tm.tm_min, tm.tm_sec);
                        // line 1
                        int tlen = strlen(buf);
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, RIGHT_CORNER(width,tlen), TXT_LINE(0));
                        // line 2
                        sprintf(buf, "%c%c%c%c %dx%d %dFPS", FOURCC_PRINTF_PARMS(cap_dev_pix_fmt), width, height, sfps);
                        tlen = strlen(buf);
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, RIGHT_CORNER(width,tlen), TXT_LINE(1));
#ifdef EVAL_STR
                        sprintf(buf, EVAL_STR);
                        tlen = strlen(buf);
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, RIGHT_CORNER(width,tlen), TXT_LINE(2));
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, LEFT_CORNER, TXT_LINE(2));
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, CENTER(width,tlen), TXT_LINE(height/2/SG_H));
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, LEFT_CORNER, TXT_LINE((height-1)/SG_H));
                        sw_overlay_nv12(input_buf, buf, tlen, width, height, RIGHT_CORNER(width,tlen), TXT_LINE((height-1)/SG_H));
#endif
                    }
#endif
   		
            		if (h264enc_encode_picture(encoder)) {       			
    					len = h264enc_get_bytestream_length(encoder);
    					pb = output_buf;
    				} 	

    				wrt_to_lpbck(th_start[i].lb_fd, 
    							  pb,
    							  len, 
    							  n_buffers, 
    							  th_start[i].lb_pbuf);
                }    
        	} else if (th_start[i].lb_codec == SIMPLE_LB) {
        		struct v4l2_buffer dev_ibuf;
    			CLEAR(dev_ibuf);

                if (cap_dev_pix_fmt == V4L2_PIX_FMT_H264) {
                    continue;
                }

        		pb = obtain_lbck_current_input_buf(th_start[i].lb_fd, n_buffers, th_start[i].lb_pbuf, &dev_ibuf);

        		if (th_start[i].pix_format == cap_dev_pix_fmt) {
        			len = buf.bytesused;
        			memcpy(pb, buffers[buf.index].start, len);
        		} else {
#if defined(CPU_HAS_NEON)        			
        			int src_stride = width*2;
        			int dst_stride_y = width;
        			int dst_stride_uv = width/2;
        			int u_offset = width*height;
        			int v_offset = u_offset + (u_offset/4);

                    if (cap_dev_pix_fmt == V4L2_PIX_FMT_UYVY) {
            			UYVYTo420P_neon(buffers[buf.index].start, src_stride,
    		               		   pb, dst_stride_y,
    		               		   pb + u_offset, dst_stride_uv,
    		               		   pb + v_offset, dst_stride_uv,
    		                       width, height);
                    } else if (cap_dev_pix_fmt == V4L2_PIX_FMT_YUYV) {
                        YUYVTo420P_neon(buffers[buf.index].start, src_stride,
                                   pb, dst_stride_y,
                                   pb + u_offset, dst_stride_uv,
                                   pb + v_offset, dst_stride_uv,
                                   width, height);
                    }
        		}
#else 
        		uyvy422to420(width, height, buffers[buf.index].start, pb);
#endif
				len = size_out;				
				write_current_input_buf_to_lbck(th_start[i].lb_fd, &dev_ibuf, len);
			}
			
			if (th_start[i].tofile == 1) {
				write(th_start[i].file_fd, pb, len);
			}
        }

#ifdef USE_FPS_MEASUREMENT
            
            /* measure fps of the capture device */
            clock_gettime(CLOCK_MONOTONIC, &tm[1]);

            int dt_ms = (time_diff(&tm[0], &tm[1])) / 1000000;
            if (dt_ms >= 1000) {
                if (measurement_en) {
                    printf("CAPTURE FPS: %d\n",nframes_ps);
                    measurement_en--;
                }
                sfps = nframes_ps;
                nframes_ps = 0;    
#ifdef PROTECT
                PROTECT(sfps);
#endif
            }
#endif
        /* queue buffer */
        if (-1 == xioctl(video_fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");

        nframes_ps++;
    }

	printf("Done!\n");
	for (i = 0;i < N_LB_DEV;i++) {
        uninit_out_mmap(th_start[i].lb_fd, th_start[i].lb_pbuf, th_start[i].lb_nbuf);
        
        if (th_start[i].tofile == 1) {
            close(th_start[i].file_fd);
        }
    }
#endif	

complete:
    if (encoder)
	   h264enc_free(encoder);

err:
	ve_close();
app_exit:    
	close(out);
	close(in);

	return EXIT_SUCCESS;
}
