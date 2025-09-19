/*!
 @file nds03_module_i2c.c
 @brief
 @author lull
 @date 2025-06
 @copyright Copyright (c) 2025  Shenzhen Nephotonics  Semiconductor Technology Co., Ltd.
 @license BSD 3-Clause License
          This file is part of the Nephotonics sensor SDK.
          It is licensed under the BSD 3-Clause License.
          A copy of the license can be found in the project root directory, in the file named LICENSE.
 */
#include "nds03.h"
#include "nds03_platform.h"

struct nds03_context *nds03_context_obj = NULL;

// int nds03_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
static int nds03_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct nds03_context * ctx = NULL;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if(IS_ERR(ctx)) {
		nds03_errmsg("nds03_data memory allocation failed\n");
		ret = -ENOMEM;
		goto module_reg_err;
	}

	nds03_context_obj = ctx;
	ctx->client = client;

	ret = nds03_common_probe(ctx);
	if(ret != 0) {
		nds03_errmsg("Failed to register nds03 module: %d\n", ret);
		goto module_reg_err;
	}
	i2c_set_clientdata(client, ctx);
	return 0;

module_reg_err:
	return ret;
}

static void nds03_i2c_remove(struct i2c_client *client)
{
	struct nds03_context * ctx = i2c_get_clientdata(client);
	nds03_common_remove(ctx);
	// return 0;
}

static const struct i2c_device_id tof_nds03_id[] = {
	{TOF_NDS03_DRV_NAME, 0 },
	{   }
};
MODULE_DEVICE_TABLE(i2c, tof_nds03_id);

static const struct of_device_id tof_nds03_dt_match[] = {
	{ .compatible = "nx,"TOF_NDS03_DRV_NAME, },
	{  }
};
MODULE_DEVICE_TABLE(of, tof_nds03_dt_match);

static struct i2c_driver tof_nds03_driver = {
	.driver = {
		.name	= TOF_NDS03_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = tof_nds03_dt_match,
	},
	.probe	= nds03_i2c_probe,
	.remove	= nds03_i2c_remove,
	.id_table = tof_nds03_id,

};

static int __init tof_nds03_init_i2c(void)
{
	int ret = 0;

	nds03_dbgmsg("Enter\n");
	ret = i2c_add_driver(&tof_nds03_driver);
	if (ret)
		nds03_errmsg("%d erro ret:%d\n", __LINE__, ret);
	return ret;
}

static void __exit  tof_nds03_exit_i2c(void )
{
	nds03_dbgmsg("Exit\n");
	i2c_del_driver(&tof_nds03_driver);
}

module_init(tof_nds03_init_i2c);
module_exit(tof_nds03_exit_i2c);

MODULE_DESCRIPTION("Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
