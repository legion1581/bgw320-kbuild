# LTE/5G USB modems. Serial side on top of the STOCK usbserial.ko (CONFIG_USB_SERIAL=m in the stock kernel):
#   usb_wwan, option (most modems' AT/diag/PPP ports), qcserial (Qualcomm/Gobi), sierra, cdc-acm (ACM AT ports)
# Network side on top of our usbnet/cdc_ncm (../usbnet): cdc-wdm (QMI/MBIM control), qmi_wwan, cdc_mbim, huawei_cdc_ncm
#   ./build-module.sh modules/<this dir>   (build modules/usbnet first)
obj-m := usb_wwan.o option.o qcserial.o sierra.o cdc-acm.o cdc-wdm.o qmi_wwan.o cdc_mbim.o huawei_cdc_ncm.o
ccflags-y += -DCONFIG_USB_SERIAL_WWAN_MODULE=1 -DCONFIG_USB_SERIAL_OPTION_MODULE=1 -DCONFIG_USB_SERIAL_QUALCOMM_MODULE=1 \
	-DCONFIG_USB_SERIAL_SIERRAWIRELESS_MODULE=1 -DCONFIG_USB_ACM_MODULE=1 -DCONFIG_USB_WDM_MODULE=1 \
	-DCONFIG_USB_NET_QMI_WWAN_MODULE=1 -DCONFIG_USB_NET_CDC_MBIM_MODULE=1 -DCONFIG_USB_NET_HUAWEI_CDC_NCM_MODULE=1 \
	-DCONFIG_USB_NET_CDC_NCM_MODULE=1 -DCONFIG_USB_USBNET_MODULE=1
KBUILD_EXTRA_SYMBOLS := $(src)/../usbnet/Module.symvers
