1、芯片对应关系
----------匹配符-------------------------函数接口----------支持的芯片型号--------------
	{.compatible = "hyn,66xx", .data = &hyn_cst66xx_fuc,},   /*support 36xx、35xx、66xx、68xx */
	{.compatible = "hyn,36xxes", .data = &hyn_cst36xxes_fuc,}, /*support 154es 3654es 3640es*/
	{.compatible = "hyn,3240", .data = &hyn_cst3240_fuc,},   /*support 3240 */
	{.compatible = "hyn,92xx", .data = &hyn_cst92xx_fuc,},   /*support 9217、9220 */
	{.compatible = "hyn,3xx",  .data = &hyn_cst3xx_fuc,},	/*support 340、348、328、128、140、148*/
	{.compatible = "hyn,7xx",  .data = &hyn_cst7xx_fuc,},	/*support 726、826、836u*/
	{.compatible = "hyn,8xxt", .data = &hyn_cst8xxT_fuc,},   /*support 816t、816d、820、08C*/
	{.compatible = "hyn,226se", .data = &hyn_cst226se_fuc,}, /*support 226se 8922*/


2、I2c接口最小dts参考配置，下面都是必配项：
	hynitron@5A {
		compatible = "hyn,66xx";	  //根据芯片型号配置
		reg = <0x5A>;				  //根据芯片型号配置

		interrupt-parent = <&tlmm>;   //根据平台配置
		interrupts = <65 0x02>;	   //根据平台配置
		reset-gpio = <&tlmm 64 0x01>; //根据平台配置
		irq-gpio = <&tlmm 65 0x02>;   //根据平台配置

		//panel = <&dsi_1080p_video>;  //CONFIG_DRM_PANEL 需要配置

		max-touch-number = <5>;
		display-coords = <0 0 800 1280>;
		pos-swap = <0>;	 			//xy坐标交换
		posx-reverse = <0>; 			//x坐标反向
		posy-reverse = <0>; 			//y坐标反向
	};

3、SPI接口最小dts参考配置，下面都是必配项：
	spi@78b900{
		hynitron@0 {
			compatible = "hyn,66xx";	  //根据芯片型号配置
			reg = <0>;					  //根据芯片型号配置
			spi-max-frequency=<6000000>;  //默认配6M

			interrupt-parent = <&tlmm>;   //根据平台配置
			interrupts = <65 0x02>;	   //根据平台配置
			reset-gpio = <&tlmm 64 0x01>; //根据平台配置
			irq-gpio = <&tlmm 65 0x02>;   //根据平台配置

			max-touch-number = <5>;
			display-coords = <0 0 800 1280>;
			pos-swap = <0>;	 			//xy坐标交换
			posx-reverse = <0>; 			//x坐标反向
			posy-reverse = <0>; 			//y坐标反向
		};
	};

4、全dts参考配置:
	hynitron@5A {
		compatible = "hyn,66xx";
		reg = <0x5A>;
		vdd_ana-supply = <&pm8953_l10>;
		vcc_i2c-supply = <&pm8953_l6>;

		interrupt-parent = <&tlmm>;
		interrupts = <65 0x02>;
		reset-gpio = <&tlmm 64 0x01>;
		irq-gpio = <&tlmm 65 0x02>;

		pinctrl-names = "ts_active","ts_suspend";
		pinctrl-0 = <&ts_int_active &ts_reset_active>;
		pinctrl-1 = <&ts_int_suspend &ts_reset_suspend>;

		max-touch-number = <5>;
		display-coords = <0 0 800 1280>;
		pos-swap = <1>;
		posx-reverse = <0>;
		posy-reverse = <0>;

		key-number = <0>;
		keys = <139 102 158>;
		key-y-coord = <2000>;
		key-x-coords = <200 600 800>;
	};

PINCTL配置
	pmx_ts_int_active {
		ts_int_active: ts_int_active {
			mux {
				pins = "gpio65";
				function = "gpio";
			};

			config {
				pins = "gpio65";
				drive-strength = <8>;
				bias-pull-up;
			};
		};
	};

	pmx_ts_reset_active {
		ts_reset_active: ts_reset_active {
			mux {
				pins = "gpio64";
				function = "gpio";
			};

			config {
				pins = "gpio64";
				drive-strength = <8>;
				bias-pull-up;
			};
		};
	};

	pmx_ts_int_suspend {
		ts_int_suspend: ts_int_suspend {
			mux {
				pins = "gpio65";
				function = "gpio";
			};

			config {
				pins = "gpio65";
				drive-strength = <2>;
				bias-pull-down;
			};
		};
	};

	pmx_ts_reset_suspend {
		ts_reset_suspend: ts_reset_suspend {
			mux {
				pins = "gpio64";
				function = "gpio";
			};

			config {
				pins = "gpio64";
				drive-strength = <2>;
				bias-pull-down;
			};
		};
	};


3、参考Makefile配置:

obj-y += hynitron_touch.o
hynitron_touch-objs += hyn_core.o hyn_lib/hyn_i2c.o hyn_lib/hyn_spi.o hyn_lib/hyn_ts_ext.o hyn_lib/hyn_fs_node.o
hynitron_touch-objs += hyn_lib/hyn_tool.o
hynitron_touch-objs += hyn_lib/hyn_gesture.o
hynitron_touch-objs += hyn_lib/hyn_prox.o
hynitron_touch-objs += hyn_chips/hyn_cst66xx.o
hynitron_touch-objs += hyn_chips/hyn_cst3240.o
hynitron_touch-objs += hyn_chips/hyn_cst92xx.o
hynitron_touch-objs += hyn_chips/hyn_cst3xx.o
hynitron_touch-objs += hyn_chips/hyn_cst1xx.o
hynitron_touch-objs += hyn_chips/hyn_cst7xx.o
hynitron_touch-objs += hyn_chips/hyn_cst8xxT.o
hynitron_touch-objs += hyn_chips/hyn_cst7xx.o


4、sys节点操作
1、升级
	通过文件升级
	adb push xxx.bin /sdcard/app.bin
	adb shell "echo fd>/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpfwver"
	通过dump升级(GKI version)
	adb push xxx.bin /sdcard/app.bin
	adb shell "cd /sys/hynitron_debug && echo fwstart>./hyndumpfw && dd if=/sdcard/app.bin of=./hyndumpfw && echo fwend>./hyndumpfw"

2、write
	eg:写 d1 01 02 03 04
	echo w d1 01 02 03 04 >/sys/hynitron_debug/hyntpdbg
3、read
	eg 读 20 byte
	echo r 20 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
3、read reg （max reg长度4 byte max read 256 byte）
	eg:写 d1 01 读 2 byte
	echo w d1 01 r 2 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
	eg:写 d1 01 02 03 读 20 byte
	echo w d1 01 02 03 r 20 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
	如果reg 不变可以直接用 cat /sys/hynitron_debug/hyntpdbg 读（reg沿用上次的操作）
4、调试log debug
	echo 7 >/proc/sys/kernel/printk
	echo log,3>/sys/hynitron_debug/hyntpdbg

5、读版TP_FW本号
	cat /sys/hynitron_debug/hyntpfwver

6、tp0 自测(需要提前准备自测配置文件)
	cat /sys/hynitron_debug/hynselftest

7、充电模式进入和退出
	enter：
	echo c1>/sys/hynitron_debug/hynswitchmode
	exit：
	echo c0>/sys/hynitron_debug/hynswitchmode
8、手套模式进入和退出
	enter：
	echo g1>/sys/hynitron_debug/hynswitchmode
	exit：
	echo g0>/sys/hynitron_debug/hynswitchmode
