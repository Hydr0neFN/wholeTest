/**
 * @file lvgl_fs_if.cpp
 * @brief LVGL Filesystem Interface for Arduino LittleFS
 */

#include <LittleFS.h>
#include "lvgl.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void * fs_open (lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close (lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read (lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_seek (lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell (lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);

/**********************
 *  PUBLIC FUNCTIONS
 **********************/

void lvgl_fs_init(void)
{
    /* Initialize LittleFS */
    if(!LittleFS.begin(true)){
        LV_LOG_ERROR("LittleFS Mount Failed");
        return;
    }

    /*Add a new drive to LVGL*/
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'S';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    
    lv_fs_drv_register(&fs_drv);
    LV_LOG_INFO("LVGL LittleFS driver registered ('S:')");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Open a file
 * @param drv pointer to a driver where this function belongs
 * @param path path to the file
 * @param mode `LV_FS_MODE_RD` or `LV_FS_MODE_WR` or `LV_FS_MODE_RD | LV_FS_MODE_WR`
 * @return pointer to a LittleFS file object or NULL on error
 */
static void * fs_open (lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);

    const char * fs_mode;
    if(mode == LV_FS_MODE_WR) fs_mode = "w";
    else if(mode == LV_FS_MODE_RD) fs_mode = "r";
    else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) fs_mode = "r+";
    else return NULL;

    // Use a dynamically allocated File object, since LVGL needs a pointer that persists.
    File* f = new File(LittleFS.open(path, fs_mode));
    
    // Check if the file was opened successfully
    if(!f || !(*f)){
        if(f) delete f; // Clean up if allocation succeeded but open failed
        LV_LOG_WARN("Failed to open file: %s", path);
        return NULL;
    }

    return (void *)f;
}

/**
 * Close an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_close (lv_fs_drv_t * drv, void * file_p)
{
    LV_UNUSED(drv);
    File* fp = (File*)file_p;
    fp->close();
    delete fp; // Free the dynamically allocated File object
    return LV_FS_RES_OK;
}

/**
 * Read data from an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param buf pointer to a memory block where to store the read data
 * @param btr number of Bytes To Read
 * @param br pointer to a variable where to store the number of real read bytes
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_read (lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    LV_UNUSED(drv);
    File* fp = (File*)file_p;
    *br = fp->read((uint8_t*)buf, btr);
    return LV_FS_RES_OK;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param pos the new position of read write pointer
 * @param whence only `LV_FS_SEEK_SET` is supported
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_seek (lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    LV_UNUSED(drv);
    File* fp = (File*)file_p;
    SeekMode mode;
    if(whence == LV_FS_SEEK_SET) mode = SeekSet;
    else if(whence == LV_FS_SEEK_CUR) mode = SeekCur;
    else if(whence == LV_FS_SEEK_END) mode = SeekEnd;
    else return LV_FS_RES_INV_PARAM;

    if(fp->seek(pos, mode)) {
      return LV_FS_RES_OK;
    }
    return LV_FS_RES_UNKNOWN;
}

/**
 * Give the position of the read write pointer
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param pos_p pointer to a variable to store the position
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_tell (lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    LV_UNUSED(drv);
    File* fp = (File*)file_p;
    *pos_p = fp->position();
    return LV_FS_RES_OK;
}
