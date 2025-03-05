// SPDX-License-Identifier: GPL-2.0-or-later

#define LOG_TAG         "Test"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_strerror.h"
#include "cts_test.h"
#include "cts_tcs.h"

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* for ksys_mkdir/sys_mkdir */
#include <linux/syscalls.h>
#include <linux/namei.h>
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

const char *cts_test_item_str(int test_item)
{
#define case_test_item(item) \
    case CTS_TEST_ ## item: return #item "-TEST"

    switch (test_item) {
        case_test_item(RESET_PIN);
        case_test_item(INT_PIN);
        case_test_item(RAWDATA);
        case_test_item(NOISE);
        case_test_item(OPEN);
        case_test_item(SHORT);
        case_test_item(COMPENSATE_CAP);

    default:
        return "INVALID";
    }
#undef case_test_item
}

#define CTS_TEST_SHORT                            (0x01)
#define CTS_TEST_OPEN                             (0x02)

#define CTS_SHORT_TEST_UNDEFINED                  (0x00)
#define CTS_SHORT_TEST_BETWEEN_COLS               (0x01)
#define CTS_SHORT_TEST_BETWEEN_ROWS               (0x02)
#define CTS_SHORT_TEST_BETWEEN_GND                (0x03)

#define SHORT_COLS_TEST_LOOP                      (1)
#define SHORT_ROWS_TEST_LOOP                      (3)


#define RAWDATA_BUFFER_SIZE(cts_dev) \
    (cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)

struct cts_fw_short_test_param {
    u8 type;
    u32 col_pattern[2];
    u32 row_pattern[2];
};

int cts_write_file(struct file *filp, const void *data, size_t size)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): kernel_write is forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    loff_t pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
#endif
}

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
/* copied from fs/namei.c */
long do_mkdirat(int dfd, const char __user *pathname, umode_t mode)
{
    struct dentry *dentry;
    struct path path;
    int error;
    unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
    dentry = user_path_create(dfd, pathname, &path, lookup_flags);
    if (IS_ERR(dentry))
          return PTR_ERR(dentry);

    if (!IS_POSIXACL(path.dentry->d_inode))
        mode &= ~current_umask();
    error = security_path_mkdir(&path, dentry, mode);
    if (!error)
        error = vfs_mkdir(path.dentry->d_inode, dentry, mode);
    done_path_create(&path, dentry);
    if (retry_estale(error, lookup_flags)) {
        lookup_flags |= LOOKUP_REVAL;
        goto retry;
    }
    return error;
}

/* Make directory for filepath
 * If filepath = "/A/B/C/D.file", it will make dir /A/B/C recursive
 * like userspace mkdir -p
 */
int cts_mkdir_for_file(const char *filepath, umode_t mode)
{
#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
    return -EPERM;
#else
    char *dirname = NULL;
    int dirname_len;
    char *s;
    int ret;
    mm_segment_t fs;

    if (filepath == NULL) {
        cts_err("Create dir for file with filepath = NULL");
        return -EINVAL;
    }

    if (filepath[0] == '\0' || filepath[0] != '/') {
        cts_err("Create dir for file with invalid filepath[0]: %c", filepath[0]);
        return -EINVAL;
    }

    dirname_len = strrchr(filepath, '/') - filepath;
    if (dirname_len == 0) {
        cts_warn("Create dir for file '%s' in root dir", filepath);
        return 0;
    }

    dirname = kstrndup(filepath, dirname_len, GFP_KERNEL);
    if (dirname == NULL) {
        cts_err("Create dir alloc mem for dirname failed");
        return -ENOMEM;
    }

    cts_info("Create dir '%s' for file '%s'", dirname, filepath);

    fs = get_fs();
    set_fs(KERNEL_DS);

    s = dirname + 1;    /* Skip leading '/' */
    while (1) {
        char c = '\0';

        /* Bypass leading non-'/'s and then subsequent '/'s */
        while (*s) {
            if (*s == '/') {
                do {
                    ++s;
                } while (*s == '/');
                c = *s;    /* Save current char */
                *s = '\0';    /* and replace it with nul */
                break;
            }
            ++s;
        }

        cts_dbg(" - Create dir '%s'", dirname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
        ret = ksys_mkdir(dirname, 0777);
#else
        ret = sys_mkdir(dirname, 0777);
#endif
        if (ret < 0 && ret != -EEXIST) {
            cts_info("Create dir '%s' failed %d(%s)", dirname, ret,
                cts_strerror(ret));
            /* Remove any inserted nul from the path */
            *s = c;
            break;
        }
        /* Reset ret to 0 if return -EEXIST */
        ret = 0;

        if (c) {
            /* Remove any inserted nul from the path */
            *s = c;
        } else {
            break;
        }
    }

    set_fs(fs);

    if (dirname) {
        kfree(dirname);
    }

    return ret;
#endif
}
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

static struct file *cts_test_data_filp;
int cts_start_dump_test_data_to_file(const char *filepath, bool append_to_file)
{
    int ret;

#define START_BANNER \
    ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"

    cts_info("Start dump test data to file '%s'", filepath);

#ifdef CTS_CONFIG_MKDIR_FOR_CTS_TEST
    ret = cts_mkdir_for_file(filepath, 0777);
    if (ret) {
        cts_err("Create dir for test data file failed %d", ret);
        return ret;
    }
#endif /* CTS_CONFIG_MKDIR_FOR_CTS_TEST */

#ifdef CFG_CTS_FOR_GKI
    cts_info("%s(): filp_open is forbiddon with GKI Version!", __func__);
    ret = -EPERM;
    return ret;
#else
    cts_test_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append_to_file ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_test_data_filp)) {
        ret = PTR_ERR(cts_test_data_filp);
        cts_test_data_filp = NULL;
        cts_err("Open file '%p' for test data failed %d", cts_test_data_filp, ret);
        return ret;
    }

    cts_write_file(cts_test_data_filp, START_BANNER, strlen(START_BANNER));

    return 0;
#endif
#undef START_BANNER
}

void cts_stop_dump_test_data_to_file(void)
{
#define END_BANNER \
    "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
#ifndef CFG_CTS_FOR_GKI
    int r;
#endif

    cts_info("Stop dump test data to file");

    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, END_BANNER, strlen(END_BANNER));
#ifndef CFG_CTS_FOR_GKI
        r = filp_close(cts_test_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
#endif
        cts_test_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }
#undef END_BANNER
}

static void cts_dump_tsdata(struct cts_device *cts_dev,
    const char *desc, const u16 *data, bool to_console)
{
#define SPLIT_LINE_STR \
    "--------------------------------------------------------"\
    "--------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[512];
    int count = 0;

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < cts_dev->fwdata.cols; r++) {
        for (c = 0; c < cts_dev->fwdata.rows; c++) {
            u16 val = data[r * cts_dev->fwdata.rows + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = c;
                max_c = r;
            } else if (val < min) {
                min = val;
                min_r = c;
                min_c = r;
            }
        }
    }
    average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < cts_dev->fwdata.rows; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < cts_dev->fwdata.cols; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.rows; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, data[r * cts_dev->fwdata.rows + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}


static void cts_dump_stylus_data(struct cts_device *cts_dev,
    const char *desc, const u16 *data, u8 px_rows, u8 px_cols, bool to_console)
{
#define SPLIT_LINE_STR \
    "--------------------------------------------------------"\
    "--------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%-5u "
#define DATA_FORMAT_STR     "%-5u "

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[512];
    int count = 0;

    max = min = data[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < px_rows; r++) {
        for (c = 0; c < px_cols; c++) {
            u16 val = data[r * px_cols + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = c;
                max_c = r;
            } else if (val < min) {
                min = val;
                min_r = c;
                min_c = r;
            }
        }
    }
    average = sum / (px_rows * px_cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " %s test data MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        desc, min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "   |  ");
    for (c = 0; c < px_cols; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < px_rows; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < px_cols; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, data[r * px_cols + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}


static bool is_invalid_node(u32 *invalid_nodes, u32 num_invalid_nodes,
    u16 row, u16 col)
{
    int i;

    for (i = 0; i < num_invalid_nodes; i++) {
        if (MAKE_INVALID_NODE(row, col) == invalid_nodes[i]) {
            return true;
        }
    }

    return false;
}

/* Return number of failed nodes */
static int validate_tsdata(struct cts_device *cts_dev, const char *desc, u16 *data,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    int failed_cnt = 0;

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

/* Return number of failed nodes */
static int validate_stylus_pc_data(struct cts_device *cts_dev, const char *desc, u16 *data,
        u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"
    u8 pc_rows_used = cts_dev->fwdata.pc_rows_used;
    u8 pc_cols_used = cts_dev->fwdata.pc_cols_used;
    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;

    int r, c;
    int failed_cnt = 0;

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < pc_rows_used; r++) {
        for (c = 0; c < pc_cols_used * pen_freq_num; c++) {
            int offset = r * pc_cols_used * pen_freq_num + c;
            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}


/* Return number of failed nodes */
static int validate_stylus_pr_data(struct cts_device *cts_dev, const char *desc, u16 *data,
        u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"
    u8 pr_rows_used = cts_dev->fwdata.pr_rows_used;
    u8 pr_cols_used = cts_dev->fwdata.pr_cols_used;
    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;

    int r, c;
    int failed_cnt = 0;

    cts_info("%s validate data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
        desc, per_node ? "Per-Node" : "Uniform-Threshold",
        num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < pr_rows_used * pen_freq_num; r++) {
        for (c = 0; c < pr_cols_used; c++) {
            int offset = r * pr_cols_used + c;
            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && data[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && data[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, data[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

#ifdef CFG_CTS_HAS_RESET_PIN
int cts_test_reset_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Reset-pin test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    cts_info("Reset-Pin test, flags: 0x%08x, drive log file: '%s' buf size: %d",
        param->flags, param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);

    cts_plat_set_reset(cts_dev->pdata, 0);

    mdelay(50);

    if (cts_plat_is_normal_mode(cts_dev->pdata)) {
        ret = -EIO;
        cts_err("Device is alive while reset is low");
    }
    cts_plat_set_reset(cts_dev->pdata, 1);
    mdelay(120);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    if (!cts_plat_is_normal_mode(cts_dev->pdata)) {
        ret = -EIO;
        cts_err("Device is offline while reset is high");
    }

    cts_unlock_device(cts_dev);
    cts_start_device(cts_dev);

    if (!cts_dev->rtdata.program_mode) {
        cts_set_normal_addr(cts_dev);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Reset-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Reset-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}
#endif

int cts_test_int_pin(struct cts_device *cts_dev, struct cts_test_param *param)
{
    ktime_t start_time, end_time, delta_time;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Int-pin test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    cts_info("Int-Pin test, flags: 0x%08x, drive log file: '%s' buf size: %d",
        param->flags, param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto show_test_result;
    }

    cts_lock_device(cts_dev);

    ret = cts_tcs_set_int_test(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test failed");
        goto unlock_device;
    }

    ret = cts_tcs_set_int_pin(cts_dev, 1);
    if (ret) {
        cts_err("Enable Int Test High failed");
        goto exit_int_test;
    }

    mdelay(10);

    if (cts_plat_get_int_pin(cts_dev->pdata) == 0) {
        cts_err("INT pin state != HIGH");
        ret = -EFAULT;
        goto exit_int_test;
    }

    ret = cts_tcs_set_int_pin(cts_dev, 0);
    if (ret) {
        cts_err("Enable Int Test LOW failed");
        goto exit_int_test;
    }

    mdelay(10);

    if (cts_plat_get_int_pin(cts_dev->pdata) != 0) {
        cts_err("INT pin state != LOW");
        ret = -EFAULT;
        goto exit_int_test;
    }

exit_int_test:
    {
        int r = cts_tcs_set_int_test(cts_dev, 0);
        if (r) {
            cts_err("Disable Int Test failed %d", r);
        }
    }
    mdelay(10);

unlock_device:
    cts_unlock_device(cts_dev);
    cts_start_device(cts_dev);

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret) {
        cts_info("Int-Pin test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Int-Pin test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_test_rawdata(struct cts_device *cts_dev, struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_test_if_validate_fail = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    int  fail_frame = 0;
    u16 *rawdata = NULL;
    ktime_t start_time, end_time, delta_time;
    int i;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Rawdata test with dev or null param");
        return -EINVAL;
    }

    if (param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Rawdata test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Rawdata test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Rawdata test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, priv_param->frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        rawdata = (u16 *) param->test_data_buf;
    } else {
        rawdata = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (rawdata == NULL) {
            cts_err("Allocate memory for rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_CFG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Set CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_RAWDATA failed %d", ret);
        goto unlock;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_POLLING failed %d", ret);
        goto unlock;
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        bool data_valid = false;

        for (i = 0; i < 3; i++) {
            ret = cts_polling_test_data(cts_dev, (u8 *)rawdata,
                    RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
            if (ret < 0) {
                cts_err("Get raw data failed: %d", ret);
                mdelay(30);
            } else {
                data_valid = true;
                break;
            }
        }

        if (!data_valid) {
            ret = -EIO;
            break;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Rawdata", rawdata, dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Rawdata", rawdata, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Rawdata test failed %d", ret);
                fail_frame++;
                cts_err("Rawdata test has %d nodes failed", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }

        if (dump_test_data_to_user) {
            rawdata += num_nodes;
        }
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && rawdata != NULL) {
        kfree(rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    if (ret) {
        cts_info("Rawdata test FAIL %d(%s), ELAPSED TIME: %lldms",
            ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else if (fail_frame > 0) {
        cts_info("Rawdata test has %d frame(s) FAIL, ELAPSED TIME: %lldms",
            fail_frame, ktime_to_ms(delta_time));
    } else if (fail_frame == 0) {
        cts_info("Rawdata test PASS, ELAPSED TIME: %lldms",
            ktime_to_ms(delta_time));
    }

    return (ret ? ret : (fail_frame ? fail_frame : 0));
}

int cts_test_noise(struct cts_device *cts_dev, struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    u16 *buffer = NULL;
    int buf_size = 0;
    u16 *curr_rawdata = NULL;
    u16 *max_rawdata = NULL;
    u16 *min_rawdata = NULL;
    u16 *noise = NULL;
    bool first_frame = true;
    ktime_t start_time, end_time, delta_time;
    int i;
    int ret;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Noise test with dev or null param");
        return -EINVAL;
    }

    if (param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Noise test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames < 2) {
        cts_err("Noise test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Noise test, flags: 0x%08x, frames: %d, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, priv_param->frames, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    buf_size = (driver_validate_data ? 4 : 1) * tsdata_frame_size;
    buffer = (u16 *) kmalloc(buf_size, GFP_KERNEL);
    if (buffer == NULL) {
           cts_err("Alloc mem for touch data failed");
           ret = -ENOMEM;
           goto show_test_result;
    }

    curr_rawdata = buffer;
    if (driver_validate_data) {
        noise        = curr_rawdata + 1 * num_nodes;
        min_rawdata  = curr_rawdata + 2 * num_nodes;
        max_rawdata  = curr_rawdata + 3 * num_nodes;
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_CFG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Set CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock;
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_RAWDATA failed %d", ret);
        goto unlock;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_POLLING failed %d", ret);
        goto unlock;
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        for (i = 0; i < 3; i++) {
            ret = cts_polling_test_data(cts_dev, (u8 *)curr_rawdata,
                RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
            if (ret < 0) {
                cts_err("Get raw data failed: %d", ret);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise-rawdata", curr_rawdata,
                dump_test_data_to_console);
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + frame * 2 * num_nodes,
                curr_rawdata, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(min_rawdata, curr_rawdata, 2 * num_nodes);
                memcpy(max_rawdata, curr_rawdata, 2 * num_nodes);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (curr_rawdata[i] > max_rawdata[i]) {
                        max_rawdata[i] = curr_rawdata[i];
                    } else if (curr_rawdata[i] < min_rawdata[i]) {
                        min_rawdata[i] = curr_rawdata[i];
                    }
                }
            }
        }
    }

    if (driver_validate_data) {
        for (i = 0; i < num_nodes; i++) {
            noise[i] = max_rawdata[i] - min_rawdata[i];
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + (priv_param->frames + 0) * 2 * num_nodes,
                noise, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
            memcpy(param->test_data_buf + (priv_param->frames + 1) * 2 * num_nodes,
                min_rawdata, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
            memcpy(param->test_data_buf + (priv_param->frames + 2) * 2 * num_nodes,
                max_rawdata, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Noise", noise,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MIN", min_rawdata,
                dump_test_data_to_console);
            cts_dump_tsdata(cts_dev, "Rawdata MAX", max_rawdata,
                dump_test_data_to_console);
        }

        ret = validate_tsdata(cts_dev, "Noise test", noise, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

disable_get_tsdata:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (buffer != NULL) {
        kfree(buffer);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Noise test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Noise test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Noise test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_open(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int ret;
    u16 *test_result = NULL;
    ktime_t start_time, end_time, delta_time;
    u8 old_int_data_method;
    u16 old_int_data_types;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Open test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    old_int_data_method = cts_dev->fwdata.int_data_method;
    old_int_data_types = cts_dev->fwdata.int_data_types;

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Open test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate memory for test result faild");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_openshort_mode(cts_dev, CTS_TEST_OPEN);
    if (ret) {
        cts_err("Set test type to OPEN_TEST failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_TEST failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("wait_to_curr_mode failed %d", ret);
        goto unlock_device;
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_RAWDATA failed %d", ret);
        goto unlock_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_POLLING failed %d", ret);
        goto unlock_device;
    }

    ret = cts_polling_test_data(cts_dev, (u8 *)test_result,
            RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto unlock_device;
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size += 2 * num_nodes;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "Open-circuit", test_result,
            dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "Open-circuit", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Open test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Open test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Open test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

/* Return 0 success
    negative value while error occurs
    positive value means how many nodes fail */
int cts_test_short(struct cts_device *cts_dev, struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool stop_if_failed = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int loopcnt;
    int ret;
    u16 *test_result = NULL;
    ktime_t start_time, end_time, delta_time;
    u8 old_int_data_method;
    u16 old_int_data_types;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Short test with invalid param: cts_dev: %p test param: %p",
             cts_dev, param);
        return -EINVAL;
    }

    old_int_data_method = cts_dev->fwdata.int_data_method;
    old_int_data_types = cts_dev->fwdata.int_data_types;

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = 2 * cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    validate_data_per_node =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_if_failed =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Short test, flags: 0x%08x, num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        test_result = (u16 *) param->test_data_buf;
    } else {
        test_result = (u16 *) kmalloc(tsdata_frame_size, GFP_KERNEL);
        if (test_result == NULL) {
            cts_err("Allocate test result buffer failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto err_free_test_result;
    }

    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    cts_info("Test short to GND");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_UNDEFINED);
    if (ret) {
        cts_err("Set short test type to SHORT_TO_GND failed %d", ret);
        goto unlock_device;
    }
    ret = cts_tcs_set_openshort_mode(cts_dev, CTS_TEST_SHORT);
    if (ret) {
        cts_err("Set test type to SHORT failed %d", ret);
        goto unlock_device;
    }
    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_TEST failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_OPEN_SHORT_DET_MODE);
    if (ret) {
        cts_err("wait_to_curr_mode failed %d", ret);
        goto unlock_device;
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_RAWDATA failed %d", ret);
        goto unlock_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_POLLING failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_GND);
    if (ret) {
        cts_err("Set short test type to SHORT_TO_GND failed %d", ret);
        goto unlock_device;
    }

    ret = cts_polling_test_data(cts_dev, (u8 *)test_result,
        RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
    if (ret) {
        cts_err("Read test result failed %d", ret);
        goto unlock_device;
    }
    if (dump_test_data_to_user) {
        *param->test_data_wr_size += 2 * num_nodes;
    }
    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }
    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_tsdata(cts_dev, "GND-short", test_result,
            dump_test_data_to_console);
    }
    if (driver_validate_data) {
        ret = validate_tsdata(cts_dev, "GND-short", test_result, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
        if (ret) {
            cts_err("Short to GND test failed %d", ret);
            if (stop_if_failed) {
                goto stop_dump_test_data_to_file;
            }
        }
    }
    if (dump_test_data_to_user) {
        test_result += num_nodes;
    }

    /* Short between colums */
    cts_info("Test short between columns");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_COLS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_COLS failed %d", ret);
        goto stop_dump_test_data_to_file;
    }

    for (loopcnt = 0; loopcnt < SHORT_COLS_TEST_LOOP; loopcnt++) {
        ret = cts_polling_test_data(cts_dev, (u8 *)test_result,
                RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto stop_dump_test_data_to_file;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Col-short", test_result,
                dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Col-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between columns test failed %d", ret);
                if (stop_if_failed) {
                    goto stop_dump_test_data_to_file;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
        }
    }

    /* Short between rows */
    cts_info("Test short between rows");
    ret = cts_tcs_set_short_test_type(cts_dev, CTS_SHORT_TEST_BETWEEN_ROWS);
    if (ret) {
        cts_err("Set short test type to BETWEEN_ROWS failed %d", ret);
        goto stop_dump_test_data_to_file;
    }
    for (loopcnt = 0; loopcnt < SHORT_ROWS_TEST_LOOP; loopcnt++) {
        ret = cts_polling_test_data(cts_dev, (u8 *)test_result,
                RAWDATA_BUFFER_SIZE(cts_dev), INT_DATA_TYPE_RAWDATA);
        if (ret) {
            cts_err("Read test result failed %d", ret);
            goto stop_dump_test_data_to_file;
        }
        if (dump_test_data_to_user) {
            *param->test_data_wr_size += 2 * num_nodes;
        }
        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_tsdata(cts_dev, "Row-short", test_result,
                dump_test_data_to_console);
        }
        if (driver_validate_data) {
            ret = validate_tsdata(cts_dev, "Row-short", test_result, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Short between rows test failed %d", ret);
                if (stop_if_failed) {
                    goto stop_dump_test_data_to_file;
                }
            }
        }
        if (dump_test_data_to_user) {
            test_result += num_nodes;
        }
    }

stop_dump_test_data_to_file:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

err_free_test_result:
    if (!dump_test_data_to_user && test_result) {
        kfree(test_result);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Short test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Short test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Short test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

static int validate_comp_cap(struct cts_device *cts_dev, const char *desc, u8 *cap,
    u32 *invalid_nodes, u32 num_invalid_nodes, bool per_node, int *min, int *max)
{
#define SPLIT_LINE_STR \
    "------------------------------"

    int r, c;
    int failed_cnt = 0;

    cts_info("Validate %s data: %s, num invalid node: %u, thresh[0]=[%d, %d]",
            desc, per_node ? "Per-Node" : "Uniform-Threshold",
            num_invalid_nodes, min ? min[0] : INT_MIN, max ? max[0] : INT_MAX);

    for (r = 0; r < cts_dev->fwdata.rows; r++) {
        for (c = 0; c < cts_dev->fwdata.cols; c++) {
            int offset = r * cts_dev->fwdata.cols + c;

            if (num_invalid_nodes &&
                is_invalid_node(invalid_nodes, num_invalid_nodes, r, c)) {
                continue;
            }

            if ((min != NULL && cap[offset] < min[per_node ? offset : 0]) ||
                (max != NULL && cap[offset] > max[per_node ? offset : 0])) {
                if (failed_cnt == 0) {
                    cts_info(SPLIT_LINE_STR);
                    cts_info("%s failed nodes:", desc);
                }
                failed_cnt++;

                cts_info("  %3d: [%-2d][%-2d] = %u", failed_cnt, r, c, cap[offset]);
            }
        }
    }

    if (failed_cnt) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s test %d node total failed", desc, failed_cnt);
    }

    return failed_cnt;

#undef SPLIT_LINE_STR
}

static void cts_dump_comp_cap(struct cts_device *cts_dev, u8 *cap, bool to_console)
{
#define SPLIT_LINE_STR \
    "-----------------------------------------------------------------------------"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%3u "
#define DATA_FORMAT_STR     "%4d"

    int r, c;
    u32 max, min, sum, average;
    int max_r, max_c, min_r, min_c;
    char line_buf[512];
    int count;

    max = min = cap[0];
    sum = 0;
    max_r = max_c = min_r = min_c = 0;
    for (r = 0; r < cts_dev->fwdata.cols; r++) {
        for (c = 0; c < cts_dev->fwdata.rows; c++) {
            u16 val = cap[r * cts_dev->fwdata.rows + c];

            sum += val;
            if (val > max) {
                max = val;
                max_r = r;
                max_c = c;
            } else if (val < min) {
                min = val;
                min_r = r;
                min_c = c;
            }
        }
    }
    average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count,
        " Compensate Cap MIN: [%u][%u]=%u, MAX: [%u][%u]=%u, AVG=%u",
        min_r, min_c, min, max_r, max_c, max, average);
    if (to_console) {
        cts_info(SPLIT_LINE_STR);
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    count = 0;
    count += scnprintf(line_buf + count, sizeof(line_buf) - count, "      ");
    for (c = 0; c < cts_dev->fwdata.rows; c++) {
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            COL_NUM_FORMAT_STR, c);
    }
    if (to_console) {
        cts_info("%s", line_buf);
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, line_buf, count);
        cts_write_file(cts_test_data_filp, "\n", 1);
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }

    for (r = 0; r < cts_dev->fwdata.cols; r++) {
        count = 0;
        count += scnprintf(line_buf + count, sizeof(line_buf) - count,
            ROW_NUM_FORMAT_STR, r);
        for (c = 0; c < cts_dev->fwdata.rows; c++) {
            count += scnprintf(line_buf + count, sizeof(line_buf) - count,
                DATA_FORMAT_STR, cap[r * cts_dev->fwdata.rows + c]);
        }
        if (to_console) {
            cts_info("%s", line_buf);
        }
        if (cts_test_data_filp) {
            cts_write_file(cts_test_data_filp, line_buf, count);
            cts_write_file(cts_test_data_filp, "\n", 1);
        }
    }

    if (to_console) {
        cts_info(SPLIT_LINE_STR);
    }
    if (cts_test_data_filp) {
        cts_write_file(cts_test_data_filp, SPLIT_LINE_STR, strlen(SPLIT_LINE_STR));
        cts_write_file(cts_test_data_filp, "\n", 1);
    }
#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR
}

int cts_test_compensate_cap(struct cts_device *cts_dev,
    struct cts_test_param *param)
{
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    u8 *cap = NULL;
    int ret = 0;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("Compensate cap test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    num_nodes = cts_dev->fwdata.rows * cts_dev->fwdata.cols;
    tsdata_frame_size = cts_dev->hwdata->num_row * cts_dev->hwdata->num_col;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);

    cts_info("Compensate cap test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        cap = (u8 *) param->test_data_buf;
    } else {
        cap = (u8 *) kzalloc(tsdata_frame_size, GFP_KERNEL);
        if (cap == NULL) {
            cts_err("Alloc mem for compensate cap failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    /* Stop device to avoid un-wanted interrrupt */
    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }

    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Set CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_FIN_DBG_MODE);
    if (ret) {
        cts_err("Wait CTS_KRANG_FIN_DBG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_top_get_cnegdata(cts_dev, cap, num_nodes, 0);
    if (ret) {
        cts_err("Get compensate cap failed %d", ret);
    }

    if (dump_test_data_to_user) {
        *param->test_data_wr_size = num_nodes;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    if (dump_test_data_to_console || dump_test_data_to_file) {
        cts_dump_comp_cap(cts_dev, cap, dump_test_data_to_console);
    }

    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

    if (driver_validate_data) {
        ret = validate_comp_cap(cts_dev, "Compensate-Cap", cap, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && cap) {
        kfree(cap);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Compensate-Cap test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Compensate-Cap test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Compensate-Cap test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_test_stylus_rawdata(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool stop_test_if_validate_fail = false;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    u16 *stylus_rawdata = NULL;
    u16 data_type;
    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;
    u8 cascade_num = cts_dev->fwdata.cascade_num;
    u8 pr_rows_used = cts_dev->fwdata.pr_rows_used;
    u8 pr_cols_used = cts_dev->fwdata.pr_cols_used;
    u8 pc_cols_used = cts_dev->fwdata.pc_cols_used;
    u8 pc_rows_used = cts_dev->fwdata.pc_rows_used;
    u8 pr_rows = cts_dev->fwdata.pr_rows;
    u8 pr_cols = cts_dev->fwdata.pr_cols;
    u8 pc_rows = cts_dev->fwdata.pc_rows;
    u8 pc_cols = cts_dev->fwdata.pc_cols;
    int pr_data_index = pc_cols_used * pc_rows_used * pen_freq_num;
    int  fail_frame = 0;
    int i, ret = 0;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("stylus rawdata test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    if (param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Stylus rawdata test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Stylus rawdata test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = (pr_rows_used * pr_cols_used + pc_cols_used * pc_rows_used)
            * pen_freq_num;
    tsdata_frame_size = (pr_rows * pr_cols + pc_rows * pc_cols)
            * pen_freq_num * sizeof(u16) * cascade_num;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Stylus rawdata test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        stylus_rawdata = (u16 *) param->test_data_buf;
    } else {
        stylus_rawdata = (u16 *) kzalloc(tsdata_frame_size, GFP_KERNEL);
        if (stylus_rawdata == NULL) {
            cts_err("Alloc mem for stylus_rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }
    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_STY_DBG_MODE);
    if (ret) {
        cts_err("Set krang CTS_KRANG_STY_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_STY_DBG_MODE);
    if (ret) {
        cts_err("Wait krang CTS_KRANG_STY_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set PWR_MODE_ACTIVE failed");
        goto unlock_device;
    }

    data_type = INT_DATA_TYPE_RAWDATA | BIT(14);
    ret = cts_set_int_data_types(cts_dev, data_type);
    if (ret) {
        cts_err("Set data_type '0x%04x' failed", data_type);
        goto unlock_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set data_method polling failed");
        goto unlock_device;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        for (i = 0; i < 3; i++) {
            ret = cts_polling_test_data(cts_dev, (u8 *)stylus_rawdata,
                tsdata_frame_size, data_type);
            if (ret < 0) {
                cts_err("Get stylus rawdata failed: %d", ret);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read stylus rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_stylus_data(cts_dev, "Stylus-Rawdata-pc", stylus_rawdata,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
            cts_dump_stylus_data(cts_dev, "Stylus-Rawdata-pr", stylus_rawdata + pr_data_index,
                    pr_rows_used * pen_freq_num, pr_cols_used, dump_test_data_to_console);
        }

        if (driver_validate_data) {
            ret = validate_stylus_pc_data(cts_dev, "Stylus-Rawdata-pc",
                stylus_rawdata, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            ret += validate_stylus_pr_data(cts_dev, "Stylus-Rawdata-pr",
                stylus_rawdata + pr_data_index, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Stylus rawdata test failed %d", ret);
                fail_frame++;
                cts_err("Stylus rawdata test has %d nodes failed", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }
    }

disable_get_tsdata:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && stylus_rawdata) {
        kfree(stylus_rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Stylus-rawdata test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Stylus-rawdata test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Stylus-Noise test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}


int cts_test_stylus_noise(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_noise_test_priv_param *priv_param;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    bool first_frame = true;
    u16 *stylus_rawdata = NULL;
    u16 *max_stylus = NULL;
    u16 *min_stylus = NULL;
    u16 *noise_stylus = NULL;
    u16 data_type;
    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;
    u8 cascade_num = cts_dev->fwdata.cascade_num;
    u8 pr_rows_used = cts_dev->fwdata.pr_rows_used;
    u8 pr_cols_used = cts_dev->fwdata.pr_cols_used;
    u8 pc_cols_used = cts_dev->fwdata.pc_cols_used;
    u8 pc_rows_used = cts_dev->fwdata.pc_rows_used;
    u8 pr_rows = cts_dev->fwdata.pr_rows;
    u8 pr_cols = cts_dev->fwdata.pr_cols;
    u8 pc_rows = cts_dev->fwdata.pc_rows;
    u8 pc_cols = cts_dev->fwdata.pc_cols;
    int pr_data_index = pc_cols_used * pc_rows_used * pen_freq_num;
    int i, ret = 0;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("stylus noise test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    if (param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Stylus noise test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Stylus noise test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    cts_info("pc_rows=%d", pc_rows);
    cts_info("pc_cols=%d", pc_cols);
    cts_info("pr_rows=%d", pr_rows);
    cts_info("pr_cols=%d", pr_cols);

    cts_info("pc_rows_used=%d", pc_rows_used);
    cts_info("pc_cols_used=%d", pc_cols_used);
    cts_info("pr_rows_used=%d", pr_rows_used);
    cts_info("pr_cols_used=%d", pr_cols_used);

    cts_info("pen_freq_num=%d", pen_freq_num);
    cts_info("cascade_num=%d", cascade_num);

    num_nodes = (pr_rows_used * pr_cols_used + pc_cols_used * pc_rows_used) * pen_freq_num;
    tsdata_frame_size = (pr_rows * pr_cols + pc_rows * pc_cols) * pen_freq_num * sizeof(u16) * cascade_num;

    driver_validate_data =
        !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);


    cts_info("Stylus rawdata test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        stylus_rawdata = (u16 *) param->test_data_buf;
    } else {
        stylus_rawdata = (u16 *) kzalloc(tsdata_frame_size * 4, GFP_KERNEL);
        if (stylus_rawdata == NULL) {
            cts_err("Alloc mem for stylus_rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    max_stylus = stylus_rawdata + 1 * tsdata_frame_size/sizeof(u16);
    min_stylus = stylus_rawdata + 2 * tsdata_frame_size/sizeof(u16);
    noise_stylus = stylus_rawdata + 3 * tsdata_frame_size/sizeof(u16);

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }
    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_STY_DBG_MODE);
    if (ret) {
        cts_err("Set krang CTS_KRANG_STY_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_STY_DBG_MODE);
    if (ret) {
        cts_err("Wait krang CTS_KRANG_STY_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_ACTIVE);
    if (ret) {
        cts_err("Set PWR_MODE_ACTIVE failed");
        goto unlock_device;
    }

    data_type = INT_DATA_TYPE_RAWDATA | BIT(14);
    ret = cts_set_int_data_types(cts_dev, data_type);
    if (ret) {
        cts_err("Set data_type '0x%04x' failed", data_type);
        goto unlock_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set data_method polling failed");
        goto unlock_device;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        for (i = 0; i < 3; i++) {
            ret = cts_polling_test_data(cts_dev, (u8 *)stylus_rawdata,
                tsdata_frame_size, data_type);
            if (ret < 0) {
                cts_err("Get stylus rawdata failed: %d", ret);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read stylus rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_stylus_data(cts_dev, "Stylus-Rawdata-pc", stylus_rawdata,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
            cts_dump_stylus_data(cts_dev, "Stylus-Rawdata-pr", stylus_rawdata + pr_data_index,
                    pr_rows_used * pen_freq_num, pr_cols_used, dump_test_data_to_console);
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + frame * 2 * num_nodes,
                stylus_rawdata, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (driver_validate_data) {
            if (unlikely(first_frame)) {
                memcpy(min_stylus, stylus_rawdata, 2 * num_nodes);
                memcpy(max_stylus, stylus_rawdata, 2 * num_nodes);
                first_frame = false;
            } else {
                for (i = 0; i < num_nodes; i++) {
                    if (stylus_rawdata[i] > max_stylus[i]) {
                        max_stylus[i] = stylus_rawdata[i];
                    }
                    if (stylus_rawdata[i] < min_stylus[i]) {
                        min_stylus[i] = stylus_rawdata[i];
                    }
                }
            }
        }
    }

    if (driver_validate_data) {
        for (i = 0; i < num_nodes; i++) {
            noise_stylus[i] = max_stylus[i] - min_stylus[i];
        }

        if (dump_test_data_to_user) {
            memcpy(param->test_data_buf + (priv_param->frames + 0) * 2 * num_nodes,
                noise_stylus, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
            memcpy(param->test_data_buf + (priv_param->frames + 1) * 2 * num_nodes,
                min_stylus, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
            memcpy(param->test_data_buf + (priv_param->frames + 2) * 2 * num_nodes,
                max_stylus, 2 * num_nodes);
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_stylus_data(cts_dev, "Stylus-Noise-pc", noise_stylus,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
            cts_dump_stylus_data(cts_dev, "Stylus-Noise-pr", noise_stylus + pr_data_index,
                    pr_rows_used * pen_freq_num, pr_cols_used, dump_test_data_to_console);

            cts_dump_stylus_data(cts_dev, "Stylus-MIN-pc", min_stylus,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
            cts_dump_stylus_data(cts_dev, "Stylus-MIN-pr", min_stylus + pr_data_index,
                    pr_rows_used * pen_freq_num, pr_cols_used, dump_test_data_to_console);

            cts_dump_stylus_data(cts_dev, "Stylus-MAX-pc", max_stylus,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
            cts_dump_stylus_data(cts_dev, "Stylus-MAX-pr", max_stylus + pr_data_index,
                    pr_rows_used * pen_freq_num, pr_cols_used, dump_test_data_to_console);
        }

        ret = validate_stylus_pc_data(cts_dev, "Stylus-Noise-pc",
            noise_stylus, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
        ret += validate_stylus_pr_data(cts_dev, "Stylus-Noise-pr",
            noise_stylus + pr_data_index, param->invalid_nodes,
            param->num_invalid_node, validate_data_per_node, param->min, param->max);
    }

disable_get_tsdata:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && stylus_rawdata) {
        kfree(stylus_rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Stylus-Noise test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Stylus-Noise test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Stylus-Noise test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

int cts_test_stylus_mnt_rawdata(struct cts_device *cts_dev,
        struct cts_test_param *param)
{
    struct cts_rawdata_test_priv_param *priv_param;
    bool stop_test_if_validate_fail = false;
    bool driver_validate_data = false;
    bool validate_data_per_node = false;
    bool dump_test_data_to_user = false;
    bool dump_test_data_to_console = false;
    bool dump_test_data_to_file = false;
    int num_nodes;
    int tsdata_frame_size;
    int frame;
    u16 *stylus_rawdata = NULL;
    u16 data_type;
    u8 pen_freq_num = cts_dev->fwdata.pen_freq_num;
    u8 cascade_num = cts_dev->fwdata.cascade_num;
    u8 pr_rows_used = cts_dev->fwdata.pr_rows_used;
    u8 pr_cols_used = cts_dev->fwdata.pr_cols_used;
    u8 pc_cols_used = cts_dev->fwdata.pc_cols_used;
    u8 pc_rows_used = cts_dev->fwdata.pc_rows_used;
    u8 pr_rows = cts_dev->fwdata.pr_rows;
    u8 pr_cols = cts_dev->fwdata.pr_cols;
    u8 pc_rows = cts_dev->fwdata.pc_rows;
    u8 pc_cols = cts_dev->fwdata.pc_cols;
    int  fail_frame = 0;
    int i, ret = 0;
    ktime_t start_time, end_time, delta_time;

    if (cts_dev == NULL || param == NULL) {
        cts_err("stylus mnt rawdata test with invalid param: cts_dev: %p test param: %p",
            cts_dev, param);
        return -EINVAL;
    }

    if (param->priv_param_size != sizeof(*priv_param) ||
        param->priv_param == NULL) {
        cts_err("Stylus mnt rawdata test with invalid param: priv param: %p size: %d",
            param->priv_param, param->priv_param_size);
        return -EINVAL;
    }

    priv_param = param->priv_param;
    if (priv_param->frames <= 0) {
        cts_info("Stylus mnt rawdata test with too little frame %u", priv_param->frames);
        return -EINVAL;
    }

    num_nodes = (pr_rows_used * pr_cols_used + pc_cols_used * pc_rows_used) * cascade_num;
    tsdata_frame_size = (pr_rows * pr_cols + pc_rows * pc_cols) * pen_freq_num * sizeof(u16) * cascade_num;

    driver_validate_data = !!(param->flags & CTS_TEST_FLAG_VALIDATE_DATA);
    if (driver_validate_data) {
        validate_data_per_node =
            !!(param->flags & CTS_TEST_FLAG_VALIDATE_PER_NODE);
    }
    dump_test_data_to_user =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_USERSPACE);
    dump_test_data_to_console =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE);
    dump_test_data_to_file =
        !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE);
    stop_test_if_validate_fail =
        !!(param->flags & CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED);

    cts_info("Stylus mnt rawdata test, flags: 0x%08x num invalid node: %u, "
         "test data file: '%s' buf size: %d, drive log file: '%s' buf size: %d",
         param->flags, param->num_invalid_node,
         param->test_data_filepath, param->test_data_buf_size,
         param->driver_log_filepath, param->driver_log_buf_size);

    start_time = ktime_get();

    if (dump_test_data_to_user) {
        stylus_rawdata = (u16 *) param->test_data_buf;
    } else {
        stylus_rawdata = (u16 *) kzalloc(tsdata_frame_size, GFP_KERNEL);
        if (stylus_rawdata == NULL) {
            cts_err("Alloc mem for stylus_rawdata failed");
            ret = -ENOMEM;
            goto show_test_result;
        }
    }

    ret = cts_stop_device(cts_dev);
    if (ret) {
        cts_err("Stop device failed %d", ret);
        goto free_mem;
    }
    cts_lock_device(cts_dev);

    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_product_en(cts_dev, 1);
    if (ret) {
        cts_err("Set product en failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Set firmware work mode to WORK_MODE_CONFIG failed %d", ret);
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_CFG_MODE);
    if (ret) {
        cts_err("Wait CTS_CFG_MODE failed %d", ret);
        goto unlock_device;
    }

    ret = cts_tcs_set_krang_workmode(cts_dev, CTS_KRANG_MNT_DBG_MODE);
    if (ret) {
        cts_err("Set krang CTS_KRANG_MNT_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_MNT_DBG_MODE);
    if (ret) {
        cts_err("Wait krang CTS_KRANG_MNT_DBG_MODE failed");
        goto unlock_device;
    }

    ret = cts_tcs_set_pwr_mode(cts_dev, CTS_PWR_MODE_MNT_DBG);
    if (ret) {
        cts_err("Set CTS_PWR_MODE_MNT_DBG failed");
        goto unlock_device;
    }

    data_type = INT_DATA_TYPE_RAWDATA | BIT(14);
    ret = cts_set_int_data_types(cts_dev, data_type);
    if (ret) {
        cts_err("Set data_type '0x%04x' failed", data_type);
        goto unlock_device;
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_POLLING);
    if (ret) {
        cts_err("Set data_method polling failed");
        goto unlock_device;
    }

    if (dump_test_data_to_file) {
        int r = cts_start_dump_test_data_to_file(param->test_data_filepath,
            !!(param->flags & CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE_APPEND));
        if (r) {
            cts_err("Start dump test data to file failed %d", r);
        }
    }

    for (frame = 0; frame < priv_param->frames; frame++) {
        for (i = 0; i < 3; i++) {
            ret = cts_polling_test_data(cts_dev, (u8 *)stylus_rawdata,
                tsdata_frame_size, data_type);
            if (ret < 0) {
                cts_err("Get stylus mnt rawdata failed: %d", ret);
                mdelay(30);
            } else {
                break;
            }
        }

        if (i >= 3) {
            cts_err("Read stylus mnt rawdata failed");
            ret = -EIO;
            goto disable_get_tsdata;
        }

        if (dump_test_data_to_user) {
            *param->test_data_wr_size += 2 * num_nodes;
        }

        if (dump_test_data_to_console || dump_test_data_to_file) {
            cts_dump_stylus_data(cts_dev, "Stylus-mnt-Rawdata-pc", stylus_rawdata,
                    pc_rows_used, pc_cols_used * pen_freq_num, dump_test_data_to_console);
        }

        if (driver_validate_data) {
            /* Only validate pc data */
            ret = validate_stylus_pc_data(cts_dev, "Stylus-mnt-Rawdata-pc",
                stylus_rawdata, param->invalid_nodes,
                param->num_invalid_node, validate_data_per_node, param->min, param->max);
            if (ret) {
                cts_err("Stylus mnt rawdata test failed %d", ret);
                fail_frame++;
                cts_err("Stylus mnt rawdata test has %d nodes failed", ret);
                if (stop_test_if_validate_fail) {
                    break;
                }
            }
        }
    }

disable_get_tsdata:
    if (dump_test_data_to_file) {
        cts_stop_dump_test_data_to_file();
    }

unlock_device:
    cts_reset_device(cts_dev);

    ret = cts_wait_current_mode(cts_dev, CTS_KRANG_NORMAL_MODE);
    if (ret) {
        cts_err("Wait fw to normal work failed %d", ret);
    }

    ret = cts_set_int_data_types(cts_dev, INT_DATA_TYPE_NONE);
    if (ret) {
        cts_err("Set INT_DATA_TYPE_NONE failed %d", ret);
    }

    ret = cts_set_int_data_method(cts_dev, INT_DATA_METHOD_NONE);
    if (ret) {
        cts_err("Set INT_DATA_METHOD_NONE failed %d", ret);
    }

    cts_unlock_device(cts_dev);

    cts_start_device(cts_dev);

free_mem:
    if (!dump_test_data_to_user && stylus_rawdata) {
        kfree(stylus_rawdata);
    }

show_test_result:
    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);
    if (ret > 0) {
        cts_info("Stylus-mnt-rawdata test has %d nodes FAIL, ELAPSED TIME: %lldms",
             ret, ktime_to_ms(delta_time));
    } else if (ret < 0) {
        cts_info("Stylus-mnt-rawdata test FAIL %d(%s), ELAPSED TIME: %lldms",
             ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
        cts_info("Stylus-mnt-rawdata test PASS, ELAPSED TIME: %lldms",
             ktime_to_ms(delta_time));
    }

    return ret;
}

