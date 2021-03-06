/*
 * Copyright (C) 2016 Shane Francis / Jens Andersen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "QSEEComAPI.h"
#include "QSEEComFunc.h"
#include "fpc_imp.h"
#include "tz_api_kitakami.h"
#include <string.h>
#include "common.h"

#define LOG_TAG "FPC IMP"
#define LOG_NDEBUG 0

#include <cutils/log.h>


#define SPI_CLK_FILE "/sys/bus/spi/devices/spi0.1/clk_enable"
#define SPI_PREP_FILE "/sys/bus/spi/devices/spi0.1/spi_prepare"
#define SPI_WAKE_FILE "/sys/bus/spi/devices/spi0.1/wakeup_enable"
#define SPI_IRQ_FILE "/sys/bus/spi/devices/spi0.1/irq"

static struct QSEECom_handle * mFPCHandle;
static struct QSEECom_handle * mKeymasterHandle;
static struct qsee_handle_t *qsee_handle = NULL;


err_t device_enable()
{
    if (sysfs_write(SPI_PREP_FILE,"enable")< 0) {
        return -1;
    }

    if (sysfs_write(SPI_CLK_FILE,"1")< 0) {
        return -1;
    }
    return 1;
}

err_t device_disable()
{
    if (sysfs_write(SPI_CLK_FILE,"0")< 0) {
        return -1;
    }

    if (sysfs_write(SPI_PREP_FILE,"disable")< 0) {
        return -1;
    }
    return 1;
}

err_t send_modified_command_to_tz(uint32_t cmd, struct QSEECom_handle * handle, void * buffer, uint32_t len)
{

    fpc_send_mod_cmd_t* send_cmd = (fpc_send_mod_cmd_t*) handle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) handle->ion_sbuffer + 64;

    struct QSEECom_ion_fd_info  ion_fd_info;
    struct qcom_km_ion_info_t ihandle;

    ihandle.ion_fd = 0;

    if (qsee_handle->ion_alloc(&ihandle, len) <0) {
        ALOGE("ION allocation  failed");
        return -1;
    }

    memset(&ion_fd_info, 0, sizeof(struct QSEECom_ion_fd_info));
    ion_fd_info.data[0].fd = ihandle.ifd_data_fd;
    ion_fd_info.data[0].cmd_buf_offset = 4;

    send_cmd->cmd_id = cmd;
    send_cmd->v_addr = (intptr_t) ihandle.ion_sbuffer;
    send_cmd->length = len;
    send_cmd->extra = 0x00;

    memcpy((unsigned char *)ihandle.ion_sbuffer, buffer, len);

    int ret = qsee_handle->send_modified_cmd(handle,send_cmd,64,rec_cmd,64,&ion_fd_info);

    if(ret < 0) {
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    if (send_cmd->v_addr != 0) {
        ALOGE("Error on TZ\n");
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    qsee_handle->ion_free(&ihandle);
    return 0;
}

err_t send_normal_command(uint32_t cmd, uint32_t param, struct QSEECom_handle * handle)
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) handle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) handle->ion_sbuffer + 64;

    send_cmd->cmd_id = cmd;
    send_cmd->ret_val = param;

    int ret = qsee_handle->send_cmd(handle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return rec_cmd->ret_val;
}

int64_t get_int64_command(uint32_t cmd, uint32_t param, struct QSEECom_handle * handle)
{

    fpc_send_int64_cmd_t* send_cmd = (fpc_send_int64_cmd_t*) handle->ion_sbuffer;
    fpc_send_int64_cmd_t* rec_cmd = (fpc_send_int64_cmd_t*) handle->ion_sbuffer + 64;

    send_cmd->cmd_id = cmd;
    send_cmd->ret_val = param;

    int ret = qsee_handle->send_cmd(handle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return send_cmd->ret_val;

}

int fpc_set_auth_challenge(int64_t __unused challenge)
{
    return send_normal_command(FPC_SET_AUTH_CHALLENGE,0,mFPCHandle);
}

int64_t fpc_load_auth_challenge()
{
    return get_int64_command(FPC_GET_AUTH_CHALLENGE,0,mFPCHandle);
}

int64_t fpc_load_db_id()
{
    return get_int64_command(FPC_GET_DB_ID,0,mFPCHandle);
}

err_t fpc_get_hw_auth_obj(void * buffer, uint32_t length)
{

    fpc_send_mod_cmd_t* send_cmd = (fpc_send_mod_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    struct QSEECom_ion_fd_info  ion_fd_info;
    struct qcom_km_ion_info_t ihandle;

    ihandle.ion_fd = 0;

    if (qsee_handle->ion_alloc(&ihandle, length) <0) {
        ALOGE("ION allocation  failed");
        return -1;
    }

    memset(&ion_fd_info, 0, sizeof(struct QSEECom_ion_fd_info));
    ion_fd_info.data[0].fd = ihandle.ifd_data_fd;
    ion_fd_info.data[0].cmd_buf_offset = 4;

    send_cmd->cmd_id = FPC_GET_AUTH_HAT;
    send_cmd->v_addr = (intptr_t) ihandle.ion_sbuffer;
    send_cmd->length = length;
    send_cmd->extra = 0x00;

    memset((unsigned char *)ihandle.ion_sbuffer, 0, length);

    int ret = qsee_handle->send_modified_cmd(mFPCHandle,send_cmd,64,rec_cmd,64,&ion_fd_info);

    memcpy(buffer, (unsigned char *)ihandle.ion_sbuffer, length);

    if(ret < 0) {
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    if (send_cmd->v_addr != 0) {
        ALOGE("Error on TZ\n");
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    qsee_handle->ion_free(&ihandle);
    return 0;

}

err_t fpc_verify_auth_challenge(void* hat, uint32_t size)
{
    return send_modified_command_to_tz(FPC_VERIFY_AUTH_CHALLENGE,mFPCHandle,hat,size);
}

static err_t fpc_get_remaining_touches()
{
    return send_normal_command(FPC_GET_REMAINING_TOUCHES,0,mFPCHandle);
}

err_t fpc_del_print_id(uint32_t id)
{

    uint32_t print_count = fpc_get_print_count();
    ALOGD("%s : print count is : %u", __func__, print_count);
    fpc_fingerprint_index_t print_indexs = fpc_get_print_ids(print_count);
    ALOGI("%s : delete print : %lu", __func__,(unsigned long) id);

    for (uint32_t i = 0; i < print_indexs.print_count; i++){

        uint32_t print_id = fpc_get_print_id(print_indexs.prints[i]);

        if (print_id == id){
                ALOGD("%s : Print index found at : %d", __func__, i);
                return send_normal_command(FPC_GET_DEL_PRINT,print_indexs.prints[i],mFPCHandle);
        }
    }

    return -1;
}

// Returns -1 on error, 1 on check again and 0 on ready to capture
err_t fpc_wait_for_finger()
{

    int finger_state  = send_normal_command(FPC_CHK_FP_LOST,FPC_CHK_FP_LOST,mFPCHandle);

    if (finger_state == 4) {
        ALOGD("%s : WAIT FOR FINGER UP\n", __func__);
    } else if (finger_state == 8) {
        ALOGD("%s : WAIT FOR FINGER DOWN\n", __func__);
    } else if (finger_state == 2) {
        ALOGD("%s : WAIT FOR FINGER NOT NEEDED\n", __func__);
        return 1;
    } else {
        return -1;
    }

    sysfs_write(SPI_WAKE_FILE,"1");
    if (send_normal_command(FPC_SET_WAKE,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_SET_WAKE to tz\n");
        return -1;
    }
    sysfs_write(SPI_CLK_FILE,"0");

    ALOGD("Attempting to poll device IRQ\n");

    if (sys_fs_irq_poll(SPI_IRQ_FILE) < 0) {
        sysfs_write(SPI_CLK_FILE,"1");
        sysfs_write(SPI_WAKE_FILE,"0");
        return 1;
    }

    sysfs_write(SPI_CLK_FILE,"1");
    sysfs_write(SPI_WAKE_FILE,"0");

    int wake_type = send_normal_command(FPC_GET_WAKE_TYPE,0,mFPCHandle);

    if (wake_type == 3) {
        ALOGD("%s : READY TO CAPTURE\n", __func__);
        return 0;
    } else {
        ALOGD("%s : NOT READY TRY AGAIN\n", __func__);
        return 1;
    }

    return 1;
}

// Attempt to capture image
err_t fpc_capture_image()
{

    if (device_enable() < 0) {
        ALOGE("Error starting device\n");
        return -1;
    }


    int ret = fpc_wait_for_finger();

    if (ret == 0) {
        //If wait reported 0 we can try and capture the image
        ret = send_normal_command(FPC_CAPTURE_IMAGE,0,mFPCHandle);
    } else {
        //return a high value as to not trigger a user notification
        ret = 1000; //same as FINGERPRINT_ERROR_VENDOR_BASE
    }

    if (device_disable() < 0) {
        ALOGE("Error stopping device\n");
        return -1;
    }

    return ret;
}

err_t fpc_enroll_step(uint32_t *remaining_touches)
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_ENROLL_STEP;
    send_cmd->ret_val = 0x24;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }
    if(rec_cmd->ret_val < 0)
        return rec_cmd->ret_val;

    int touches = fpc_get_remaining_touches();
    if(touches < 0)
        return touches;

    *remaining_touches = (uint32_t)touches;
    return rec_cmd->ret_val;
}

err_t fpc_enroll_start(int print_index)
{
    fpc_send_enroll_start_cmd_t* send_cmd = (fpc_send_enroll_start_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_ENROLL_START;
    send_cmd->ret_val = 0x00;
    send_cmd->na1 = 0x45;
    send_cmd->print_index = print_index;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return rec_cmd->ret_val;
}

err_t fpc_enroll_end(uint32_t *print_id)
{

    int index = send_normal_command(FPC_ENROLL_END,0x0,mFPCHandle);

    if (index < 0 || index > 4) {
        ALOGE("Error sending FPC_ENROLL_END to tz\n");
        return -1;
    }

    *print_id = fpc_get_print_id(index);

    return 0;
}

fpc_fingerprint_index_t fpc_get_print_ids(uint32_t count)
{

    fpc_fingerprint_index_t data;

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_get_pint_index_cmd_t* rec_cmd = (fpc_get_pint_index_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_GET_ID_LIST;
    send_cmd->ret_val = count;
    send_cmd->length = count;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    data.prints[0] = rec_cmd->p1;
    data.prints[1] = rec_cmd->p2;
    data.prints[2] = rec_cmd->p3;
    data.prints[3] = rec_cmd->p4;
    data.prints[4] = rec_cmd->p5;
    data.print_count = rec_cmd->print_count;

    return data;
}

err_t fpc_auth_start()
{

    uint32_t print_count = (uint32_t)fpc_get_print_count();
    fpc_fingerprint_index_t prints;
    ALOGI("%s : Number Of Prints Available : %d",__func__,print_count);

    prints = fpc_get_print_ids(print_count);

    fpc_get_pint_index_cmd_t* send_cmd = (fpc_get_pint_index_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;


    send_cmd->cmd_id = FPC_AUTH_START;
    send_cmd->p1 = prints.prints[0];
    send_cmd->p2 = prints.prints[1];
    send_cmd->p3 = prints.prints[2];
    send_cmd->p4 = prints.prints[3];
    send_cmd->p5 = prints.prints[4];
    send_cmd->print_count = prints.print_count;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        ALOGE("Error sending FPC_AUTH_START to tz\n");
        return -1;
    }

    return rec_cmd->ret_val;
}

err_t fpc_auth_step(uint32_t *print_id)
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_auth_cmd_t* rec_cmd = (fpc_send_auth_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_AUTH_STEP;
    send_cmd->ret_val = 0x00;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    //if the print didnt capture properly return error and continue
    if (rec_cmd->ret_val < 2) {
        return -1;
    }



    *print_id = (uint32_t)fpc_get_print_id(rec_cmd->id);
    return 0;
}

err_t fpc_auth_end()
{

    err_t ret = send_normal_command(FPC_AUTH_END,0x0,mFPCHandle);

    if (ret != 0) {
        ALOGE("Error sending FPC_AUTH_END to tz\n");
        return -1;
    }
    return ret;
}

err_t fpc_get_print_id(int id)
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_GET_PRINT_ID;
    send_cmd->ret_val = id;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return send_cmd->ret_val;
}


err_t fpc_get_print_count()
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_GET_ID_COUNT;
    send_cmd->ret_val = 0x00;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return send_cmd->ret_val;
}


fpc_fingerprint_index_t fpc_get_print_index(uint32_t count)
{

    fpc_fingerprint_index_t data;

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_get_pint_index_cmd_t* rec_cmd = (fpc_get_pint_index_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_GET_ID_LIST;
    send_cmd->ret_val = count;
    send_cmd->length = count;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    data.prints[0] = (uint32_t)fpc_get_print_id(rec_cmd->p1);
    data.prints[1] = (uint32_t)fpc_get_print_id(rec_cmd->p2);
    data.prints[2] = (uint32_t)fpc_get_print_id(rec_cmd->p3);
    data.prints[3] = (uint32_t)fpc_get_print_id(rec_cmd->p4);
    data.prints[4] = (uint32_t)fpc_get_print_id(rec_cmd->p5);
    data.print_count = rec_cmd->print_count;

    return data;
}


err_t fpc_get_user_db_length()
{

    fpc_send_std_cmd_t* send_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    send_cmd->cmd_id = FPC_GET_DB_LENGTH;
    send_cmd->ret_val = 0x00;

    int ret = qsee_handle->send_cmd(mFPCHandle,send_cmd,64,rec_cmd,64);

    if(ret < 0) {
        return -1;
    }

    return send_cmd->ret_val;
}


err_t fpc_load_user_db(char* path)
{

    FILE *f = fopen(path, "r");

    if (f == NULL) {
        ALOGE("Error opening file : %s", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    ALOGI("Loading DB of size : %ld", fsize);

    struct qcom_km_ion_info_t ihandle;
    struct QSEECom_ion_fd_info  ion_fd_info;

    if (qsee_handle->ion_alloc(&ihandle, fsize) <0) {
        ALOGE("ION allocation  failed");
        return -1;
    }

    fread(ihandle.ion_sbuffer, fsize, 1, f);

    fclose(f);

    fpc_send_mod_cmd_t* send_cmd = (fpc_send_mod_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    memset(&ion_fd_info, 0, sizeof(struct QSEECom_ion_fd_info));
    ion_fd_info.data[0].fd = ihandle.ifd_data_fd;
    ion_fd_info.data[0].cmd_buf_offset = 4;

    send_cmd->cmd_id = FPC_SET_DB_DATA;
    send_cmd->v_addr = (intptr_t) ihandle.ion_sbuffer;
    send_cmd->length = fsize;
    send_cmd->extra = 0x00;

    int ret = qsee_handle->send_modified_cmd(mFPCHandle,send_cmd,64,rec_cmd,64,&ion_fd_info);

    if(ret < 0) {
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    if (send_cmd->v_addr != 0) {
        ALOGE("Error on TZ\n");
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    qsee_handle->ion_free(&ihandle);
    return 0;

}

err_t fpc_store_user_db(uint32_t length, char* path)
{

    fpc_send_mod_cmd_t* send_cmd = (fpc_send_mod_cmd_t*) mFPCHandle->ion_sbuffer;
    fpc_send_std_cmd_t* rec_cmd = (fpc_send_std_cmd_t*) mFPCHandle->ion_sbuffer + 64;

    struct QSEECom_ion_fd_info  ion_fd_info;
    struct qcom_km_ion_info_t ihandle;

    ihandle.ion_fd = 0;

    if (qsee_handle->ion_alloc(&ihandle, length) <0) {
        ALOGE("ION allocation  failed");
        return -1;
    }

    memset(&ion_fd_info, 0, sizeof(struct QSEECom_ion_fd_info));
    ion_fd_info.data[0].fd = ihandle.ifd_data_fd;
    ion_fd_info.data[0].cmd_buf_offset = 4;

    send_cmd->cmd_id = FPC_GET_DB_DATA;
    send_cmd->v_addr = (intptr_t) ihandle.ion_sbuffer;
    send_cmd->length = length;
    send_cmd->extra = 0x00;

    memset((unsigned char *)ihandle.ion_sbuffer, 0, length);

    int ret = qsee_handle->send_modified_cmd(mFPCHandle,send_cmd,64,rec_cmd,64,&ion_fd_info);

    if(ret < 0) {
        qsee_handle->ion_free(&ihandle);
        return -1;
    }

    if (send_cmd->v_addr != 0) {
        ALOGE("Error on TZ\n");
        qsee_handle->ion_free(&ihandle);
        return -1;
    }


    FILE *f = fopen(path, "w");

    if (f == NULL) {
        ALOGE("Error opening file : %s", path);
        return -1;
    }

    fwrite(ihandle.ion_sbuffer, length, 1, f);

    fclose(f);

    qsee_handle->ion_free(&ihandle);
    return 0;
}

err_t fpc_set_gid(uint32_t __unused gid)
{
    // Not used on kitakami
    return 0;
};

err_t fpc_close()
{
    if (device_disable() < 0) {
        ALOGE("Error stopping device\n");
        return -1;
    }
    return 1;
}

err_t fpc_init() {
    int ret = 0;

    ALOGE("INIT FPC TZ APP\n");

    if (qsee_open_handle(&qsee_handle) != 0) {
        ALOGE("Error loading QSEECom library");
        return -1;
    }

    if (device_enable() < 0) {
        ALOGE("Error starting device\n");
        return -1;
    }

    if (qsee_handle->load_trustlet(qsee_handle, &mFPCHandle, FP_TZAPP_PATH,
                                   FP_TZAPP_NAME, 1024) < 0) {
        ALOGE("Could not load app %s\n", FP_TZAPP_NAME);
        return -1;
    }

    if (qsee_handle->load_trustlet(qsee_handle, &mKeymasterHandle, KM_TZAPP_PATH,
                             KM_TZAPP_NAME, 1024) < 0) {
        ALOGE("Could not load app %s\n", KM_TZAPP_NAME);
        return -1;
    }

    // Start creating one off command to get cert from keymaster
    fpc_send_std_cmd_t *req = (fpc_send_std_cmd_t *) mKeymasterHandle->ion_sbuffer;
    req->cmd_id = 0x205;
    req->ret_val = 0x02;

    void * send_buf = mKeymasterHandle->ion_sbuffer;
    void * rec_buf = mKeymasterHandle->ion_sbuffer + 64;

    if (qsee_handle->send_cmd(mKeymasterHandle, send_buf, 64, rec_buf, 1024-64) < 0) {
        return -1;
    }

    //Send command to keymaster
    fpc_send_std_cmd_t* ret_data = (fpc_send_std_cmd_t*) rec_buf;

    ALOGE("Keymaster Response Code : %u\n", ret_data->ret_val);
    ALOGE("Keymaster Response Length : %u\n", ret_data->length);

    void * data_buff = &ret_data->length + 1;

    if (send_modified_command_to_tz(FPC_SET_INIT_DATA,mFPCHandle,data_buff,ret_data->length) < 0) {
        ALOGE("Error sending data to tz\n");
        return -1;
    }

    if (send_normal_command(FPC_INIT,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_INIT to tz\n");
        return -1;
    }

    if (send_normal_command(FPC_GET_INIT_STATE,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_GET_INIT_STATE to tz\n");
        return -1;
    }

    if (send_normal_command(FPC_INIT_UNK_1,0,mFPCHandle) != 12) {
        ALOGE("Error sending FPC_INIT_UNK_1 to tz\n");
        return -1;
    }

    if (device_enable() < 0) {
        ALOGE("Error starting device\n");
        return -1;
    }

    if (send_normal_command(FPC_INIT_UNK_2,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_INIT_UNK_2 to tz\n");
        return -1;
    }

    int fpc_info = send_normal_command(FPC_INIT_UNK_0,0,mFPCHandle);

    ALOGI("Got device data : %d \n", fpc_info);

    if (device_disable() < 0) {
        ALOGE("Error stopping device\n");
        return -1;
    }

    qsee_handle->set_bandwidth(mFPCHandle,true);

    if (send_normal_command(FPC_INIT_NEW_DB,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_INIT_NEW_DB to tz\n");
        return -1;
    }

    if (send_normal_command(FPC_SET_FP_STORE,0,mFPCHandle) != 0) {
        ALOGE("Error sending FPC_SET_FP_STORE to tz\n");
        return -1;
    }

    qsee_handle->set_bandwidth(mFPCHandle,false);

    return 1;

}
