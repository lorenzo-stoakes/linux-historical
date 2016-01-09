/* Base file for all ieee1394 ioctl's. Linux-1394 has allocated base '#'
 * with a range of 0x00-0x3f. */

#ifndef __IEEE1394_IOCTL_H
#define __IEEE1394_IOCTL_H

#include <asm/ioctl.h>
#include <asm/types.h>


/* AMDTP Gets 6 */
#define AMDTP_IOC_CHANNEL	_IOW('#', 0x00, struct amdtp_ioctl)
#define AMDTP_IOC_PLUG		_IOW('#', 0x01, struct amdtp_ioctl)
#define AMDTP_IOC_PING		_IOW('#', 0x02, struct amdtp_ioctl)
#define AMDTP_IOC_ZAP		_IO ('#', 0x03)


/* DV1394 Gets 10 */

/* Get the driver ready to transmit video.  pass a struct dv1394_init* as
 * the parameter (see below), or NULL to get default parameters */
#define DV1394_IOC_INIT			_IOW('#', 0x06, struct dv1394_init)

/* Stop transmitting video and free the ringbuffer */
#define DV1394_IOC_SHUTDOWN		_IO ('#', 0x07)

/* Submit N new frames to be transmitted, where the index of the first new
 * frame is first_clear_buffer, and the index of the last new frame is
 * (first_clear_buffer + N) % n_frames */
#define DV1394_IOC_SUBMIT_FRAMES	_IO ('#', 0x08)

/* Block until N buffers are clear (pass N as the parameter) Because we
 * re-transmit the last frame on underrun, there will at most be n_frames
 * - 1 clear frames at any time */
#define DV1394_IOC_WAIT_FRAMES		_IO ('#', 0x09)

/* Capture new frames that have been received, where the index of the
 * first new frame is first_clear_buffer, and the index of the last new
 * frame is (first_clear_buffer + N) % n_frames */
#define DV1394_IOC_RECEIVE_FRAMES	_IO ('#', 0x0a)

/* Tell card to start receiving DMA */
#define DV1394_IOC_START_RECEIVE	_IO ('#', 0x0b)

/* Pass a struct dv1394_status* as the parameter */
#define DV1394_IOC_GET_STATUS		_IOR('#', 0x0c, struct dv1394_status)


/* Video1394 Gets 10 */

#define VIDEO1394_IOC_LISTEN_CHANNEL		\
	_IOWR('#', 0x10, struct video1394_mmap)
#define VIDEO1394_IOC_UNLISTEN_CHANNEL		\
	_IOW ('#', 0x11, int)
#define VIDEO1394_IOC_LISTEN_QUEUE_BUFFER	\
	_IOW ('#', 0x12, struct video1394_wait)
#define VIDEO1394_IOC_LISTEN_WAIT_BUFFER	\
	_IOWR('#', 0x13, struct video1394_wait)
#define VIDEO1394_IOC_TALK_CHANNEL		\
	_IOWR('#', 0x14, struct video1394_mmap)
#define VIDEO1394_IOC_UNTALK_CHANNEL		\
	_IOW ('#', 0x15, int)
#define VIDEO1394_IOC_TALK_QUEUE_BUFFER 	\
	_IOW ('#', 0x16, sizeof (struct video1394_wait) + \
		sizeof (struct video1394_queue_variable))
#define VIDEO1394_IOC_TALK_WAIT_BUFFER		\
	_IOW ('#', 0x17, struct video1394_wait)
#define VIDEO1394_IOC_LISTEN_POLL_BUFFER	\
	_IOWR('#', 0x18, struct video1394_wait)

#endif /* __IEEE1394_IOCTL_H */
