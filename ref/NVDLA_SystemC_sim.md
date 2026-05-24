https://zhuanlan.zhihu.com/p/630822241

NVDLA SystemC模型仿真
小艾同学
小艾同学​
哈尔滨工业大学 电子信息硕士
收录于 · NVDLA加速器
20 人赞同了该文章
NVDLA的c模型一般是在vp中使用umd/runtime来运行，一般来说运行网络推理的时候，compiler和runtime必不可少，但是运行单层网络，或者对NVDLA的某部分模块进行仿真验证，有时候不需要通过runtime运行compiler得到的loadable文件。像运行c程序那样，给予SystemC模型测试激励即可，但是NVDLA本身是需要借助CPU发送数据给内部，所以依然脱离不了vp平台。

接下来将演示重用hw目录下的UVM测试案例，编写相应的c程序，在vp中启动NVDLA的c模型，并打印输出结果。

思考：

1.NVDLA vp平台是依靠qemu运行的，也就是在qemu模拟的arm cortex a57上运行demo，因为有arm cortex a57下的linux系统，所以可以编写c程序运行。

2.NVDLA的启动过程完全依靠内部的寄存器配置，通过配置寄存器实现NVDLA的启动和结束。

3.hw/verif/tests/trace_tests/nv_small下有UVM提供的测试案例和相应寄存器的配置，是否可以利用这些测试案例搭建SystemC模型的仿真。

我在个人的ubuntu上，搭建了全流程的hw+sw+vp的编译，目前可以跑通全流程的编译和运行，整个虚拟机的大小也只有30g。

首先是编译c模型，在nvdla hw文件夹下，运行./tools/bin/tmake -build cmod_top:


图1 cmod编译
裁剪过的nv_small 的c模型，编译速度很快，当然不兼容nv_large操作了。

在vp目录下的conf/aarch64_nvdla.lua里，记录了qemu系统启动的参数以及nvdla的寄存器地址分配范围。

CPU = {
    library = "libqbox-nvdla.so",
    extra_arguments = "-machine virt -cpu cortex-a57 -machine type=virt -nographic -smp 1 -m 1024 -kernel /home/zwy/NVDLA_zwy/sw/prebuilt/arm64-linux/images/linux-4.13.3/Image --append \"root=/dev/vda\" -drive file=/home/zwy/NVDLA_zwy/sw/prebuilt/arm64-linux/images/linux-4.13.3/rootfs.ext4,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -fsdev local,id=r,path=.,security_model=none -device virtio-9p-device,fsdev=r,mount_tag=r -netdev user,id=user0,hostfwd=tcp::6666-:6666,hostfwd=tcp::6667-:22 -device virtio-net-device,netdev=user0"
}
-machine virt -cpu cortex-a57：选定了arm cortex a57作为模拟系统；

-smp 1 -m 1024：选择单核CPU，内存大小为1g。

ram = {
    size = 1048576,
    target_port = {
        base_addr = 0xc0000000,
        high_addr = 0xffffffff
    }
}
ram的列表里分配了1g的内存，模拟的是NVDLA的mcif模块所连接的片外DRAM。地址起始是0xc0000000，结束是0xffffffff。

nvdla = {
    irq_number = 176,
    csb_port = {
        base_addr = 0x10200000,
        high_addr = 0x1021ffff
    }
}
nvdla的列表里给出了寄存器的地址分配范围，0x10200000~0x1021ffff。

在vp平台里运行以下三条指令，可以启动vp环境，但是我们先不急。

cmake -DCMAKE_INSTALL_PREFIX=build -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ -DNVDLA_HW_PREFIX=/home/zwy/NVDLA_zwy/hw-nv_small -DNVDLA_HW_PROJECT=nv_small
make
sudo make install
我们回到hw目录下的verif/tests/trace_tests/nv_small,里面包括了很多测试案例。


图2 UVM平台下的测试案例
取其中的dc_8x8x36_4x4x36x16_dilation_int8_0 ，目录下包括了三个文件。

dc_8x8x36_4x4x36x16_dilation_int8_0.cfg
dc_8x8x36_4x4x36x16_dilation_int8_0_dt.dat
dc_8x8x36_4x4x36x16_dilation_int8_0_wt.dat
cfg中首先实现了对寄存器的配置，使用了mem_init初始化cdma的data地址，weight地址，sdp的输出地址，其中pri_mem代指NVDLA的mcif对应的片外DRAM。

mem_init(pri_mem，初始地址，大小，ALL_ZERO)；

mem_load(pri_mem，初始地址，大小，读取的配置文件)；

//Feature
mem_init(pri_mem, 0x80002800, 0xa000, ALL_ZERO);
mem_load(pri_mem, 0x80002800, "dc_8x8x36_4x4x36x16_dilation_int8_0_dt.dat");
//Weight
mem_init(pri_mem, 0x90000100, 0x2400, ALL_ZERO);
mem_load(pri_mem, 0x90000100, "dc_8x8x36_4x4x36x16_dilation_int8_0_wt.dat");
//Dest
mem_init(pri_mem, 0xa0000000, 0x500, ALL_ZERO);
接下来cfg文件中对寄存器进行了配置：

//Disable CDMA DATA_DONE and WEIGHT_DONE interrupts
reg_write(NVDLA_GLB.S_INTR_MASK_0, 0x3f03fc);
reg_write(NVDLA_GLB.S_INTR_STATUS_0, 0x0);
reg_write(NVDLA_SDP.S_POINTER_0, 0x0);
reg_write(NVDLA_SDP.D_DATA_CUBE_CHANNEL_0, 0xf);
reg_write(NVDLA_SDP.D_DP_BS_ALU_SRC_VALUE_0, 0xa838);
reg_write(NVDLA_SDP.D_DP_EW_CFG_0, 0xd);
reg_write(NVDLA_SDP.D_DP_EW_MUL_CFG_0, 0x3);
reg_write(NVDLA_SDP.D_CVT_OFFSET_0, 0x0);
reg_write(NVDLA_SDP.D_DP_EW_ALU_CVT_OFFSET_VALUE_0, 0xb4d9b7f);
reg_write(NVDLA_SDP.D_STATUS_NAN_INPUT_NUM_0, 0x0);
reg_write(NVDLA_SDP.D_DP_EW_ALU_CVT_SCALE_VALUE_0, 0x9aa9);
reg_write(NVDLA_SDP.D_PERF_LUT_LE_HIT_0, 0x0);
reg_write(NVDLA_SDP.D_PERF_LUT_LO_HIT_0, 0x0);
reg_write(NVDLA_SDP.D_DP_EW_ALU_CVT_TRUNCATE_VALUE_0, 0x13);
reg_write(NVDLA_SDP.D_DP_BS_CFG_0, 0x2b);
reg_write(NVDLA_SDP.D_PERF_OUT_SATURATION_0, 0x0);
reg_write(NVDLA_SDP.D_PERF_WDMA_WRITE_STALL_0, 0x0);
reg_write(NVDLA_SDP.D_DP_BS_MUL_CFG_0, 0x2701);
......
poll_reg_equal(NVDLA_CDMA.S_CBUF_FLUSH_STATUS_0,0x1);
reg_write(NVDLA_SDP.D_OP_ENABLE_0, 0x1);
reg_write(NVDLA_CACC.D_OP_ENABLE_0, 0x1);
reg_write(NVDLA_CMAC_A.D_OP_ENABLE_0, 0x1);
reg_write(NVDLA_CMAC_B.D_OP_ENABLE_0, 0x1);
reg_write(NVDLA_CSC.D_OP_ENABLE_0, 0x1);
reg_write(NVDLA_CDMA.D_OP_ENABLE_0, 0x1);
intr_notify(SDP_0, sync_id_0);
poll_reg_equal用于比较NVDLA_CDMA.S_CBUF_FLUSH_STATUS_0是否等于0x1，用于判断cdma到cbuf的传输是否结束。置位所有的op_enable后·，NVDLA就开始工作了。

那么我们是否可以仿照这个搭建c模型在vp里的测试程序呢，答案是可以的，我们首先看vp/test目录下，已经包含了一个nvdla_bdma_mmio。里面已经实现了一个针对bdma的测试案例。

它主要做了以下几点：

1.使用mmap将nvdla的寄存器地址 0x10200000~0x1021ffff映射到虚拟地址方便读写。

2.使用mmap将部分nvdla读取片外data和weight的地址范围映射到虚拟地址。

3.定义寄存器配置数据的结构体。

4.获取bdma产生的中断信号，退出程序，bdma的中断信号从寄存器中获取。

我们对它做相应的修改，就可以实现将UVM里的测试案例搬过去。实现一个dc_8x8x36_4x4x36x16_dilation_int8xint8_0案例，首先是对cdma获取的data地址、weight地址、sdp输出地址、nvdla reg base addr的定义：

#define DATA_BASE         0xc0000000
#define WEIGHT_BASE       0xd0000000
#define OUT_BASE          0xf0000000
#define NVDLA_MMIO_BASE   0x10200000

#define DATA_SIZE         (0x270f)
#define WEIGHT_SIZE       (0x8f9)
#define OUT_SIZE          (0x400)
#define NVDLA_MMIO_SIZE  (0x10220000 - 0x10200000)
原本的测试程序使用了mmap将物理地址映射到虚拟地址，但是针对测试case数据量很大的时候，没法在虚拟平台映射，因为vp平台的配置有点低，映射过多会产生堆栈错误，但是我们运行的dc卷积的案例数据量都是很大，所以使用了另一种方法，借助于linux物理内存直接读写工具devmem配置，我自己写了个python脚本，可以将UVM测试的case，dc_8x8x36_4x4x36x16_dilation_int8_0_dt.dat和dc_8x8x36_4x4x36x16_dilation_int8_0_wt.dat转换为两个.sh文件，data的：

devmem 0xc0000000 32 0x39c22f1d
devmem 0xc0000004 32 0xb9fb4a19
devmem 0xc0000008 32 0x000fdfe2
devmem 0xc000000c 32 0x0000c75c
devmem 0xc0000010 32 0x00008219
devmem 0xc0000014 32 0x81f9fca2
devmem 0xc0000018 32 0x9fdf0066
devmem 0xc000001c 32 0x702fd395
devmem 0xc0000020 32 0x0000e2a8
devmem 0xc0000024 32 0x8c570011
devmem 0xc0000028 32 0x008e360c
devmem 0xc000002c 32 0xe5a27bf2
devmem 0xc0000030 32 0xd562e537
devmem 0xc0000034 32 0x00ae4152
devmem 0xc0000038 32 0x0045009f
devmem 0xc000003c 32 0x530000b4
devmem 0xc0000400 32 0x844043cd
devmem 0xc0000404 32 0x5554f500
devmem 0xc0000408 32 0x84fee042
devmem 0xc000040c 32 0x55b00079
......
weight的：

devmem 0xd0000018 32 0x000000d8
devmem 0xd000001c 32 0x00c00000
devmem 0xd0000024 32 0x00c30000
devmem 0xd0000028 32 0x31000000
devmem 0xd0000034 32 0xc2000000
devmem 0xd000003c 32 0x00003200
devmem 0xd0000044 32 0x00001200
devmem 0xd000004c 32 0x000000ef
devmem 0xd0000054 32 0x86000000
devmem 0xd0000058 32 0x001d4700
devmem 0xd0000060 32 0x000000d7
devmem 0xd0000068 32 0x008b0000
devmem 0xd000006c 32 0xc7000000
devmem 0xd0000098 32 0x000000b7
devmem 0xd00000a0 32 0x00f80000
devmem 0xd00000b0 32 0x005e0000
devmem 0xd00000c0 32 0x000000f8
devmem 0xd00000cc 32 0x00950000
devmem 0xd00000d4 32 0x00b40000
devmem 0xd00000d8 32 0x00000200
devmem 0xd000010c 32 0xe7000000
devmem 0xd000011c 32 0x3d000000
devmem 0xd0000140 32 0x00000015
devmem 0xd000015c 32 0x5b000000
devmem 0xd0000168 32 0x000000bf
devmem 0xd000016c 32 0x00980000
devmem 0xd0000170 32 0x00c50000
devmem 0xd0000174 32 0x009d0000
devmem 0xd0000180 32 0x0000d300
.....
所以mmap只保留对原本的寄存器的mmap即可。定义dla_reg结构体：

struct dla_reg {
    int32_t  offset;
    uint32_t value;
};
定义寄存器配置，其中cdma读取片外data和weight的地址 CDMA_D_DAIN_ADDR_HIGH_0_0,CDMA_D_DAIN_ADDR_LOW_0_0，CDMA_D_WEIGHT_ADDR_HIGH_0_0,CDMA_D_WEIGHT_ADDR_LOW_0_0，sdp输出的地址 SDP_D_DST_BASE_ADDR_HIGH_0,SDP_D_DST_BASE_ADDR_LOW_0先配置成0。

const struct dla_reg* test_program(struct tensor* data_in, struct tensor* weight_in,struct tensor* sdp_out)
{
    static struct dla_reg program_initial[] = {
 {   CDMA_D_DAIN_ADDR_HIGH_0_0, 0x0},
 {   CDMA_D_DAIN_ADDR_LOW_0_0, 0x0},  //
 {   CDMA_D_WEIGHT_ADDR_HIGH_0, 0x0},
 {   CDMA_D_WEIGHT_ADDR_LOW_0, 0x0}, //
 {   SDP_D_DST_BASE_ADDR_HIGH_0, 0x0},
 {   SDP_D_DST_BASE_ADDR_LOW_0, 0x0}, //
  {  GLB_S_INTR_MASK_0, 0x3f03fc},
  {  GLB_S_INTR_STATUS_0, 0x0},
  {  SDP_S_POINTER_0, 0x0},
  {  SDP_D_DATA_CUBE_CHANNEL_0, 0xf},
  {  SDP_D_DP_BS_ALU_SRC_VALUE_0, 0xa838},
  {  SDP_D_DP_EW_CFG_0, 0xd},
  {  SDP_D_DP_EW_MUL_CFG_0, 0x3},
  {  SDP_D_CVT_OFFSET_0, 0x0},
  {  SDP_D_DP_EW_ALU_CVT_OFFSET_VALUE_0,
......
最后对地址进行赋值，赋给我们定义的地址宏变量：

  program_initial[0].value = HIGH32( data_in->base  );
  program_initial[1].value = LOW32 ( data_in->base );
  program_initial[2].value = HIGH32( weight_in->base );
  program_initial[3].value = LOW32 ( weight_in->base );
  program_initial[4].value = HIGH32( sdp_out->base );
  program_initial[5].value = LOW32 ( sdp_out->base );
定义op_enable启动的结构体：

const struct dla_reg* start_program()
{
    static struct dla_reg program_start[] = {
    { SDP_D_OP_ENABLE_0,     0x1},
    { CACC_D_OP_ENABLE_0,    0x1},
    { CMAC_A_D_OP_ENABLE_0,  0x1},
    { CMAC_B_D_OP_ENABLE_0,  0x1}, 
    { CSC_D_OP_ENABLE_0,     0x1},
    { CDMA_D_OP_ENABLE_0,    0x1},
     {-1, 0}
    };
    return program_start;
}
在main函数里实现，先配置其它寄存器，最后配置op_enable相关的寄存器，nvdla就启动了。

    while (progs_initial->offset != -1) {
        reg_initial = (uint32_t*)(dla_mmio + progs_initial->offset);
        *reg_initial = progs_initial->value;
        //printf("Write reg 0x%08x, value 0x%08x\n", progs_initial->offset, progs_initial->value);
        ++progs_initial;
     }
    while(*cbuf_status ==  0x1) { 
        printf("cdma -> cbuf done\n");
        break; 
    }
    while (progs_start->offset != -1) {
        reg_start = (uint32_t*)(dla_mmio + progs_start->offset);
        *reg_start = progs_start->value;
        //printf("Write reg 0x%08x, value 0x%08x\n", progs_start->offset, progs_start->value);
        ++progs_start;
    }
我定义了一个指针指向前文所述的NVDLA_CDMA.S_CBUF_FLUSH_STATUS_0，等它等于1，再启动op_enable的配置。接下来只要获取到sdp产生的中断信号，就可以判断完成了，将sdp传输的数据打印到对应的文件里。这部分也是仿照原本的bdma的测试案例改写的。

void test_check(struct tensor* sdp_out, void* out_mm,uint8_t* dla_mmio)
{
    uint32_t* status = (uint32_t*)(dla_mmio + GLB_S_INTR_STATUS_0);
    int32_t loop = 1000000000;
    volatile uint32_t int_status;
    FILE *fp3 = fopen("/mnt/tests/dc_8x8x36_4x4x36x16_dilation_int8xint8_0/sdp2mcif_output.dat","wb");
    while( 1 ) {
        int_status = *status;
        if( (int_status >> GLB_S_INTR_STATUS_0_SDP_DONE_STATUS0_SHIFT) & 1 ) {
            printf("Get SDP interrupt ...\n");
            uint32_t *mem = (uint32_t*)((uint8_t*)out_mm + sdp_out -> base - OUT_BASE);
            //printf("base address3 = 0x%p \n", mem);
             for (int i = 0; i < OUT_SIZE; ++i) {
		 if(mem[i]!= 0){
                    fprintf(fp3, "%x\n", mem[i]);}
            }
            //fprintf(fp3, "\n");
            fclose(fp3);
            break;
          }
        if(loop-- <= 0) {
           printf("Polling timeout\n");
        }
    }
}
在原本的cmod/cbuf/NV_NVDLA_cbuf.cpp下也会将cdma到cbuf的数据进行打印，保存在两个文件里：

#ifdef DEBUG_DUMP
    fp_cdma2cbuf_data = fopen("./cdma2cbuf_data_cmod.dat", "w");
    if(!fp_cdma2cbuf_data) cslInfo(("WARNING! Failed to create cdma2cbuf_data_cmod.dat\n"));
    fp_cdma2cbuf_weight = fopen("./cdma2cbuf_weight_cmod.dat", "w");
    if(!fp_cdma2cbuf_weight) cslInfo(("WARNING! Failed to create cdma2cbuf_weight_cmod.dat\n"));
#endif

......


#ifdef DEBUG_DUMP
        fprintf(fp_cdma2cbuf_data, "%02x", payload_data_ptr[payload->size - idx - 1] & 0xff);
#endif
    } 
其中的fprintf语句就是将结果输出到对应的文件里，我借鉴了这部分的思路。

我们在启动vp平台后，依次运行：

mount -t 9p -o trans=virtio r /mnt
cd /mnt/tests/dc_8x8x36_4x4x36x16_dilation_int8xint8_0
./dat_load.sh
./wt_load.sh
./aarch64_cmod_test
便会打印：


图3 测试pass
会产生以下三个文件，cdma2cbuf_data_cmod.dat、cdma2cbuf_weight_cmod.dat、sdp2mcif_output.dat。

在sdp2mcif_output.dat中产生输出数据：

7f7f7f7f
7f807f7f
807f7f7f
7f808080
80808080
7f807f7f
7f808080
7f807f7f
7f7f7f80
7f7f8074
807f7f80
807f7f7f
80808080
7f7f7f80
80808080
7f7f7f7f
结果验证是正确的。观察UVM测试时产生的波形文件，在相应的hw目录下，运行

./verif/tools/run_test.py -P nv_small dc_8x8x36_4x4x36x16_dilation_int8_0  -outdir uvm_zwy/dc_8x8x36_4x4x36x16_dilation_int8_0_output -wave -v nvdla_utb
cd uvm_zwy/dc_8x8x36_4x4x36x16_dilation_int8_0_output
./run_verdi.sh
观察mcif输出波形：



可以验证单独仿真c模型的结果是正确的。

如果你对原本的c模型进行了修改，并想验证它的结果正确，将原本的输出结果作为golden_case，与现有结果对比即可。