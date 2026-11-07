# USB tethering: usbnet family (stock kernel has CONFIG_USB_NET_DRIVERS off) + ipheth with the iOS 17+ NCM backport.
# Sources: ACK android13-5.15 tree; ipheth.c = ACK ipheth.c + the upstream CDC NCM series (ipheth-ncm-patches/).
#   ./build-module.sh modules/<this dir>
obj-m := mii.o usbnet.o cdc_ether.o rndis_host.o cdc_ncm.o ipheth.o
ccflags-y += -DCONFIG_USB_NET_RNDIS_HOST_MODULE=1 -DCONFIG_USB_NET_CDCETHER_MODULE=1 -DCONFIG_USB_NET_CDC_NCM_MODULE=1 -DCONFIG_USB_USBNET_MODULE=1 -DCONFIG_MII_MODULE=1
