/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2011 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <libopencm3/cm3/common.h>
#include <libopencm3/stm32/tools.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/dwc/otg_common.h>
#include "usb_private.h"
#include "usb_dwc_common.h"

#define USB2(x)        MMIO32((x) + (USB2_OTG_FS_BASE))

/* Receive FIFO size in 32-bit words. */
#define RX_FIFO_SIZE 512

static usbd_device *stm32h750_usbd_init(void);

static struct _usbd_device usbd_dev;

const struct _usbd_driver stm32h750_usb_driver = {
	.init = stm32h750_usbd_init,
	.set_address = dwc_set_address,
	.ep_setup = dwc_ep_setup,
	.ep_reset = dwc_endpoints_reset,
	.ep_stall_set = dwc_ep_stall_set,
	.ep_stall_get = dwc_ep_stall_get,
	.ep_nak_set = dwc_ep_nak_set,
	.ep_write_packet = dwc_ep_write_packet,
	.ep_read_packet = dwc_ep_read_packet,
	.poll = dwc_poll,
	.disconnect = dwc_disconnect,
	.base_address = USB2_OTG_FS_BASE,
	.set_address_before_status = 1,
	.rx_fifo_size = RX_FIFO_SIZE,
};

/** Initialize the USB device controller hardware of the STM32. */
static usbd_device *stm32h750_usbd_init(void)
{
	USB2(OTG_GINTSTS) = OTG_GINTSTS_MMIS;

	USB2(OTG_GUSBCFG) |= OTG_GUSBCFG_PHYSEL;
	/* Disable VBUS sensing in device mode and power down the PHY. */
	USB2(OTG_GOTGCTL) |= OTG_GOTGCTL_BVALOEN | OTG_GOTGCTL_BVALOVAL;
	USB2(OTG_GCCFG) |= OTG_GCCFG_PWRDWN;

	/* Wait for AHB idle. */
	while (!(USB2(OTG_GRSTCTL) & OTG_GRSTCTL_AHBIDL));
	/* Do core soft reset. */
	USB2(OTG_GRSTCTL) |= OTG_GRSTCTL_CSRST;
	while (USB2(OTG_GRSTCTL) & OTG_GRSTCTL_CSRST);

	/* Force peripheral only mode. */
	USB2(OTG_GUSBCFG) |= OTG_GUSBCFG_FDMOD | OTG_GUSBCFG_TRDT_MASK;

	/* Full speed device. */
	USB2(OTG_DCFG) |= OTG_DCFG_DSPD;

	/* Restart the PHY clock. */
	USB2(OTG_PCGCCTL) = 0;

	USB2(OTG_GRXFSIZ) = stm32h750_usb_driver.rx_fifo_size;
	usbd_dev.fifo_mem_top = stm32h750_usb_driver.rx_fifo_size;

	/* Unmask interrupts for TX and RX. */
	USB2(OTG_GAHBCFG) |= OTG_GAHBCFG_GINT;
	USB2(OTG_GINTMSK) = OTG_GINTMSK_ENUMDNEM |
			 OTG_GINTMSK_RXFLVLM |
			 OTG_GINTMSK_IEPINT |
			 OTG_GINTMSK_USBSUSPM |
			 OTG_GINTMSK_WUIM;
	USB2(OTG_DAINTMSK) = 0xF;
	USB2(OTG_DIEPMSK) = OTG_DIEPMSK_XFRCM;

	return &usbd_dev;
}
