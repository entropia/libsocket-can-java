#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include<algorithm>

#include "de_entropia_can_CanSocket.h"

static const int ERRNO_BUFFER_LEN = 1024;

static void throwException(JNIEnv *env, const char *const exception_name,
			const char *const msg)
{
	const jclass exception = env->FindClass(exception_name);
	if (exception == NULL) {
		return;
	}
	env->ThrowNew(exception, msg);
}

static void throwIOExceptionMsg(JNIEnv *env, const char *const msg)
{
	throwException(env, "java/io/IOException", msg);
}

static void throwIOExceptionErrno(JNIEnv *env, const int exc_errno)
{
	char message[ERRNO_BUFFER_LEN];
	const char *const msg = strerror_r(exc_errno, message, ERRNO_BUFFER_LEN);
	throwIOExceptionMsg(env, msg);
}

static void throwIllegalArgumentException(JNIEnv *env, const char *const message)
{
	throwException(env, "java/lang/IllegalArgumentException", message);
}

static void throwOutOfMemoryError(JNIEnv *env, const char *const message)
{
	throwException(env, "java/lang/OutOfMemoryError", message);
}

static jint newCanSocket(JNIEnv *env, int socket_type, int protocol)
{
	const int fd = socket(PF_CAN, socket_type, protocol);
	if (fd != -1) {
		return (jint)fd;
	}
	throwIOExceptionErrno(env, errno);
	return (jint)-1;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1openSocketRAW
(JNIEnv *env, jclass obj)
{
	return newCanSocket(env, SOCK_RAW, CAN_RAW);
}


JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1openSocketBCM
(JNIEnv *env, jclass obj)
{
	return newCanSocket(env, SOCK_DGRAM, CAN_BCM);
}

JNIEXPORT void JNICALL Java_de_entropia_can_CanSocket__1close
(JNIEnv *env, jobject obj, jint fd)
{
	if (close((int)fd) == -1) {
		throwIOExceptionErrno(env, errno);
	}
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1discoverInterfaceIndex
(JNIEnv *env, jclass clazz, jint socketFd, jstring ifName)
{
	struct ifreq ifreq;
	const jsize ifNameSize = env->GetStringUTFLength(ifName);
	if ((int)ifNameSize > IFNAMSIZ-1) {
		throwIllegalArgumentException(env, "illegal interface name");
		return (jint)-1;
	}
	
	/* fetch interface name */
	memset((void *)&ifreq, 0x0, sizeof(ifreq));
	env->GetStringUTFRegion(ifName, (jsize)0, (jsize)ifNameSize,
				ifreq.ifr_name);	
	if (env->ExceptionCheck() == JNI_TRUE) {
		return (jint)-1;
	}
	/* discover interface id */
	const int err = ioctl((int)socketFd, SIOCGIFINDEX, &ifreq);
	if (err == -1) {
		throwIOExceptionErrno(env, errno);
		return (jint)-1;
	} else {
		return (jint)ifreq.ifr_ifindex;
	}
}

JNIEXPORT jstring JNICALL Java_de_entropia_can_CanSocket__1discoverInterfaceName
(JNIEnv *env, jclass obj, jint fd, jint ifIdx)
{
	struct ifreq ifreq;
	memset((void *)&ifreq, 0x0, sizeof(ifreq));
	ifreq.ifr_ifindex = (int)ifIdx;
	if (ioctl((int)fd, SIOCGIFNAME, &ifreq) == -1) {
		throwIOExceptionErrno(env, errno);
		return NULL;
	}
	const jstring ifname = env->NewStringUTF(ifreq.ifr_name);
	return ifname;
}


JNIEXPORT void JNICALL Java_de_entropia_can_CanSocket__1bindToSocket
(JNIEnv *env, jclass obj, jint fd, jint ifIndex)
{
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = (int)ifIndex;
	if (bind((int)fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		throwIOExceptionErrno(env, errno);
	}
}

JNIEXPORT void JNICALL Java_de_entropia_can_CanSocket__1sendFrame
(JNIEnv *env, jclass obj, jint fd, jint if_idx, jint canid, jbyteArray data)
{
	const int flags = 0;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	memset(&addr, 0, sizeof(addr));
	memset(&frame, 0, sizeof(frame));
	addr.can_family = AF_CAN;
	addr.can_ifindex = (int)if_idx;
	const jsize len = env->GetArrayLength(data);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return;
	}
	frame.can_id = (canid_t)canid;
	frame.can_dlc = (__u8)len;
	env->GetByteArrayRegion(data, 0, len, (jbyte *)&frame.data);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return;
	}
	nbytes = sendto((int)fd, &frame, sizeof(frame), flags,
			(struct sockaddr *)&addr, sizeof(addr));
	if (nbytes == -1) {
		throwIOExceptionErrno(env, errno);
	} else if (nbytes != sizeof(frame)) {
		throwIOExceptionMsg(env, "send partial frame");
	}
}

JNIEXPORT jobject JNICALL Java_de_entropia_can_CanSocket__1recvFrame
(JNIEnv *env, jclass obj, jint fd)
{
	const int flags = 0;
	int nbytes;
	struct sockaddr_can addr;
	socklen_t len = sizeof(addr);
	struct can_frame frame;

	memset(&addr, 0, sizeof(addr));
	memset(&frame, 0, sizeof(frame));
	nbytes = recvfrom((int)fd, &frame, sizeof(frame), flags,
			(struct sockaddr *)&addr, &len);
	if (len != sizeof(addr)) {
		throwIllegalArgumentException(env, "illegal AF_CAN address");
		return NULL;
	}
	if (nbytes == -1) {
		throwIOExceptionErrno(env, errno);
		return NULL;
	} else if (nbytes != sizeof(frame)) {
		throwIOExceptionMsg(env, "invalid length of received frame");
		return NULL;
	}
	const jsize fsize = (jsize)std::min((size_t)frame.can_dlc,
				(size_t)nbytes - offsetof(struct can_frame, data));
	const jclass can_frame_clazz = env->FindClass("de/entropia/can/"
							"CanSocket$CanFrame");
	if (can_frame_clazz == NULL) {
		return NULL;
	}
	const jmethodID can_frame_cstr = env->GetMethodID(can_frame_clazz,
							"<init>", "(II[B)V");
	if (can_frame_cstr == NULL) {
		return NULL;
	}
	const jbyteArray data = env->NewByteArray(fsize);
	if (data == NULL) {
		if (env->ExceptionCheck() != JNI_TRUE) {
			throwOutOfMemoryError(env, "could not allocate ByteArray");
		}
		return NULL;
	}
	env->SetByteArrayRegion(data, (jsize)0, fsize, (jbyte *)&frame.data);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return NULL;
	}
	const jobject ret = env->NewObject(can_frame_clazz, can_frame_cstr,
					(jint)addr.can_ifindex, (jint)frame.can_id,
					data);
	return ret;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetchInterfaceMtu
(JNIEnv *env, jclass obj, jint fd, jstring ifName)
{
	struct ifreq ifreq;

	const jsize ifNameSize = env->GetStringUTFLength(ifName);
	if ((int)ifNameSize > IFNAMSIZ-1) {
		throwIllegalArgumentException(env, "illegal interface name");
		return (jint)-1;
	}
	memset((void *)&ifreq, 0x0, sizeof(ifreq));
	env->GetStringUTFRegion(ifName, (jsize)0, ifNameSize, ifreq.ifr_name);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return (jint)-1;
	}
	if (ioctl((int)fd, SIOCGIFMTU, &ifreq) == -1) {
		throwIOExceptionErrno(env, errno);
		return (jint)-1;
	} else {
		return (jint)ifreq.ifr_mtu;
	}
}

JNIEXPORT void JNICALL Java_de_entropia_can_CanSocket__1setsockopt
(JNIEnv *env, jclass obj, jint fd, jint op, jint stat)
{
	const int _stat = (int)stat;
	if (setsockopt((int)fd, SOL_CAN_RAW, (int)op, &_stat, sizeof(_stat)) == -1) {
		throwIOExceptionErrno(env, errno);
	}
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1getsockopt
(JNIEnv *env, jclass obj, jint fd, jint op)
{
	int _stat = 0;
	socklen_t len = sizeof(_stat);
	if (getsockopt((int)fd, SOL_CAN_RAW, (int)op, &_stat, &len) == -1) {
		throwIOExceptionErrno(env, errno);
	}
	if (len != sizeof(_stat)) {
		throwIllegalArgumentException(env, "setsockopt return size is different");
		return (jint)-1;
	}
	return (jint)_stat;
}


/*** constants ***/

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1MTU
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_MTU;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1FD_1MTU
(JNIEnv *env, jclass obj)
{
	return (jint)CANFD_MTU;
}

/*** ioctls ***/
JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1RAW_1FILTER
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_RAW_FILTER;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1RAW_1ERR_1FILTER
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_RAW_ERR_FILTER;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1RAW_1LOOPBACK
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_RAW_LOOPBACK;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1RAW_1RECV_1OWN_1MSGS
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_RAW_RECV_OWN_MSGS;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1fetch_1CAN_1RAW_1FD_1FRAMES
(JNIEnv *env, jclass obj)
{
	return (jint)CAN_RAW_FD_FRAMES;
}

/*** ADR MANIPULATION FUNCTIONS ***/

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1getCANID_1SFF
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & CAN_SFF_MASK);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1getCANID_1EFF
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & CAN_EFF_MASK);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1getCANID_1ERR
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & CAN_ERR_MASK);
}

JNIEXPORT jboolean JNICALL Java_de_entropia_can_CanSocket__1isSetEFFSFF
(JNIEnv *env, jclass obj, jint canid)
{
	return (canid & CAN_EFF_FLAG) != 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_de_entropia_can_CanSocket__1isSetRTR
(JNIEnv *env, jclass obj, jint canid)
{
	return (canid & CAN_RTR_FLAG) != 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_de_entropia_can_CanSocket__1isSetERR
(JNIEnv *env, jclass obj, jint canid)
{
	return (canid & CAN_ERR_FLAG) != 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1setEFFSFF
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid | CAN_EFF_FLAG);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1setRTR
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid | CAN_RTR_FLAG);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1setERR
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid | CAN_ERR_FLAG);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1clearEFFSFF
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & ~CAN_EFF_FLAG);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1clearRTR
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & ~CAN_RTR_FLAG);
}

JNIEXPORT jint JNICALL Java_de_entropia_can_CanSocket__1clearERR
(JNIEnv *env, jclass obj, jint canid)
{
	return (jint)(canid & ~CAN_ERR_FLAG);
}
