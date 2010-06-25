/* arch/arm/mach-msm/qdsp6/audio_ctrl.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/msm_audio.h>
#include <mach/qdsp6/msm8k_cad.h>
#include <mach/qdsp6/msm8k_cad_ioctl.h>
#include <mach/qdsp6/msm8k_ard.h>
#include <mach/qdsp6/msm8k_cad_devices.h>
#include <mach/qdsp6/msm8k_cad_volume.h>

#define AUDIO_UPDATE_ACDB    _IOW(AUDIO_IOCTL_MAGIC, 34, unsigned)
#define AUDIO_START_VOICE    _IOW(AUDIO_IOCTL_MAGIC, 35, unsigned)
#define AUDIO_STOP_VOICE     _IOW(AUDIO_IOCTL_MAGIC, 36, unsigned)
#define AUDIO_REINIT_ACDB    _IOW(AUDIO_IOCTL_MAGIC, 39, unsigned)

#define BUFSZ (0)

#define ADSP_AUDIO_DEVICE_ID_HANDSET_MIC        0x107ac8d
#define ADSP_AUDIO_DEVICE_ID_HEADSET_MIC        0x1081510
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MIC     0x1081512
#define ADSP_AUDIO_DEVICE_ID_BT_SCO_MIC         0x1081518
#define ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_MIC    0x108151b
#define ADSP_AUDIO_DEVICE_ID_I2S_MIC            0x1089bf3

/* Special loopback pseudo device to be paired with an RX device */
/* with usage ADSP_AUDIO_DEVICE_USAGE_MIXED_PCM_LOOPBACK */
#define ADSP_AUDIO_DEVICE_ID_MIXED_PCM_LOOPBACK_TX      0x1089bf2

/* Sink (RX) devices */
#define ADSP_AUDIO_DEVICE_ID_HANDSET_SPKR                       0x107ac88
#define ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_MONO                  0x1081511
#define ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_STEREO                0x107ac8a
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO                    0x1081513
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_MONO_HEADSET     0x108c508
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_STEREO_HEADSET   0x108c894
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO                  0x1081514
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_MONO_HEADSET   0x108c895
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_STEREO_HEADSET 0x108c509
#define ADSP_AUDIO_DEVICE_ID_BT_SCO_SPKR                        0x1081519
#define ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_SPKR                   0x108151c
#define ADSP_AUDIO_DEVICE_ID_I2S_SPKR                           0x1089bf4

#define A_HANDSET_MIC                ADSP_AUDIO_DEVICE_ID_HANDSET_MIC
#define A_HANDSET_SPKR               ADSP_AUDIO_DEVICE_ID_HANDSET_SPKR
#define A_HEADSET_MIC                ADSP_AUDIO_DEVICE_ID_HEADSET_MIC
#define A_HEADSET_SPKR_MONO          ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_MONO
#define A_HEADSET_SPKR_STEREO        ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_STEREO
#define A_SPKR_PHONE_MIC             ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MIC
#define A_SPKR_PHONE_MONO            ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO
#define A_SPKR_PHONE_STEREO          ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO
#define A_BT_A2DP_SPKR               ADSP_AUDIO_DEVICE_ID_BT_A2DP_SPKR
#define A_BT_SCO_MIC                 ADSP_AUDIO_DEVICE_ID_BT_SCO_MIC
#define A_BT_SCO_SPKR                ADSP_AUDIO_DEVICE_ID_BT_SCO_SPKR
#define A_TTY_HEADSET_MIC            ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_MIC
#define A_TTY_HEADSET_SPKR           ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_SPKR
#define A_FM_HEADSET                 ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_STEREO
#define A_FM_SPKR                    ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO
#define A_SPKR_PHONE_HEADSET_STEREO  ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_MONO_HEADSET


static DEFINE_MUTEX(voice_lock);

static int q6_open(struct inode *inode, struct file *file)
{
	return 0;
}

static struct  {
	u32 cad_r_handle;
	u32 cad_w_handle;
	u32 volume;
} *voice;

static int msm8k_voice_open(void) {
	struct cad_open_struct_type  cos;
	struct cad_stream_info_struct_type cad_stream_info;
	struct cad_stream_device_struct_type cad_stream_dev;
	u32 stream_device[1];
	int rc;

	if(voice) {
		printk("Voice already opened!\n");
		return -EBUSY;
	}
	voice = kmalloc(sizeof(*voice), GFP_KERNEL);
	if (voice == NULL) {
		pr_err("Could not allocate memory for voice driver\n");
		return CAD_RES_FAILURE;
	}

	memset(&cad_stream_info, 0, sizeof(struct cad_stream_info_struct_type));

	memset(voice, 0, sizeof(*voice));

	cos.op_code = CAD_OPEN_OP_WRITE;
	voice->cad_w_handle = cad_open(&cos);

	if (voice->cad_w_handle == 0)
		return CAD_RES_FAILURE;

	cad_stream_info.app_type = CAD_STREAM_APP_VOICE;
	cad_stream_info.priority = 0;
	cad_stream_info.buf_mem_type = CAD_STREAM_BUF_MEM_HEAP;
	cad_stream_info.ses_buf_max_size = 1024;
	rc = cad_ioctl(voice->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_INFO,
		&cad_stream_info,
		sizeof(struct cad_stream_info_struct_type));
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_SET_STREAM_INFO failed\n");
		return CAD_RES_FAILURE;
	}

	stream_device[0] = CAD_HW_DEVICE_ID_DEFAULT_RX;
	cad_stream_dev.device = (u32 *)&stream_device[0];
	cad_stream_dev.device_len = 1;
	rc = cad_ioctl(voice->cad_w_handle, CAD_IOCTL_CMD_SET_STREAM_DEVICE,
		&cad_stream_dev,
		sizeof(struct cad_stream_device_struct_type));
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_SET_STREAM_DEVICE failed\n");
		return CAD_RES_FAILURE;
	}

	rc = cad_ioctl(voice->cad_w_handle, CAD_IOCTL_CMD_STREAM_START,
		NULL, 0);
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_STREAM_START failed\n");
		return CAD_RES_FAILURE;
	}

	cos.op_code = CAD_OPEN_OP_READ;
	voice->cad_r_handle = cad_open(&cos);

	if (voice->cad_r_handle == 0)
		return CAD_RES_FAILURE;

	cad_stream_info.app_type = CAD_STREAM_APP_VOICE;
	cad_stream_info.priority = 0;
	cad_stream_info.buf_mem_type = CAD_STREAM_BUF_MEM_HEAP;
	cad_stream_info.ses_buf_max_size = 1024;
	rc = cad_ioctl(voice->cad_r_handle, CAD_IOCTL_CMD_SET_STREAM_INFO,
		&cad_stream_info,
		sizeof(struct cad_stream_info_struct_type));
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_SET_STREAM_INFO failed\n");
		return CAD_RES_FAILURE;
	}

	stream_device[0] = CAD_HW_DEVICE_ID_DEFAULT_TX;
	cad_stream_dev.device = (u32 *)&stream_device[0];
	cad_stream_dev.device_len = 1;
	rc = cad_ioctl(voice->cad_r_handle, CAD_IOCTL_CMD_SET_STREAM_DEVICE,
		&cad_stream_dev,
		sizeof(struct cad_stream_device_struct_type));
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_SET_STREAM_DEVICE failed\n");
		return CAD_RES_FAILURE;
	}

	rc = cad_ioctl(voice->cad_r_handle, CAD_IOCTL_CMD_STREAM_START,
		NULL, 0);
	if (rc) {
		pr_err("cad_ioctl() CAD_IOCTL_CMD_STREAM_START failed\n");
		return CAD_RES_FAILURE;
	}

	return rc;
}

static int msm8k_voice_release(void){
	int rc = CAD_RES_SUCCESS;

	if(voice) {
		cad_close(voice->cad_w_handle);
		cad_close(voice->cad_r_handle);
		kfree(voice);
		voice=NULL;
	}

	return rc;
}

static int q6_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	int rc=0;
	uint32_t n;
	uint32_t id[2];
	struct msm_mute_info mute_info;

	switch (cmd) {
	case AUDIO_SWITCH_DEVICE:
		rc = copy_from_user(&id, (void *)arg, sizeof(id));
		//id[0]=dev_id
		//id[1]=acdb id
		if (!rc) {
			printk("switch device!(%d,%d)\n", id[0], id[1]);
			switch(id[0]) {
				case A_HANDSET_MIC:
					n=HANDSET_MIC;
					break;
				case A_HANDSET_SPKR:
					n=HANDSET_SPKR;
					break;
				case A_HEADSET_MIC:
					n=HEADSET_MIC;
					break;
				case A_HEADSET_SPKR_MONO:
					n=HEADSET_SPKR_MONO;
					break;
				case A_HEADSET_SPKR_STEREO:
					n=HEADSET_SPKR_STEREO;
					break;
				case A_SPKR_PHONE_MIC:
					n=SPKR_PHONE_MIC;
					break;
				case A_SPKR_PHONE_MONO:
					n=SPKR_PHONE_MONO;
					break;
				case A_SPKR_PHONE_STEREO:
					n=SPKR_PHONE_STEREO;
					break;
			};
			rc = audio_switch_device(n);
		}
		break;
	case AUDIO_SET_VOLUME:
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		printk("set volume %d!\n", n);
		if (!rc)
			rc = audio_set_device_volume(n);
		break;
	case AUDIO_SET_MUTE:
		printk("set (mic?) mute!\n");
		rc = copy_from_user(&n, (void *)arg, sizeof(n));
		if (!rc) {
			mute_info.mute=n;
			mute_info.path=CAD_RX_DEVICE;
			rc = audio_set_device_mute(&mute_info);
			mute_info.mute=n;
			mute_info.path=CAD_TX_DEVICE;
			rc = audio_set_device_mute(&mute_info);
		}
		break;
	case AUDIO_UPDATE_ACDB:
		rc = copy_from_user(&id, (void *)arg, sizeof(id));
		printk("Unhandled: update ACDB\n");
		break;
	case AUDIO_START_VOICE:
		msm8k_voice_open();
		mute_info.mute=0;
		mute_info.path=CAD_RX_DEVICE;
		rc = audio_set_device_mute(&mute_info);
		mute_info.mute=0;
		mute_info.path=CAD_TX_DEVICE;
		rc = audio_set_device_mute(&mute_info);
		printk("Unhandled: start voice\n");
		break;
	case AUDIO_STOP_VOICE:
		msm8k_voice_release();
		break;
	case AUDIO_REINIT_ACDB:
		printk("Unhandled: reinit acdb\n");
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}


static int q6_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations q6_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= q6_open,
	.ioctl		= q6_ioctl,
	.release	= q6_release,
};

struct miscdevice q6_control_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_ctl",
	.fops	= &q6_dev_fops,
};


static int __init q6_audio_ctl_init(void) {
	return misc_register(&q6_control_device);
}

device_initcall(q6_audio_ctl_init);
