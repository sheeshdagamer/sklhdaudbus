#include "driver.h"

//New
NTSTATUS ResetHDAController(PFDO_CONTEXT fdoCtx, BOOLEAN wakeup) {
	UINT32 gctl;

	//Clear STATESTS
	hda_write16(fdoCtx, STATESTS, STATESTS_INT_MASK);

	//Stop all Streams DMA Engine
	for (UINT32 i = 0; i < fdoCtx->numStreams; i++) {
		hdac_stream_stop(&fdoCtx->streams[i]);
	}

	//Stop CORB and RIRB
	hda_write8(fdoCtx, CORBCTL, 0);
	hda_write8(fdoCtx, RIRBCTL, 0);

	//Reset DMA position buffer
	hda_write32(fdoCtx, DPLBASE, 0);
	hda_write32(fdoCtx, DPUBASE, 0);

	//Reset the controller for at least 100 us
	gctl = hda_read32(fdoCtx, GCTL);
	hda_write32(fdoCtx, GCTL, gctl & ~HDA_GCTL_RESET);

	for (int count = 0; count < 1000; count++) {
		gctl = hda_read32(fdoCtx, GCTL);
		if (!(gctl & HDA_GCTL_RESET)) {
			break;
		}
		udelay(10);
	}

	if (gctl & HDA_GCTL_RESET) {
		DbgPrint("Error: unable to put controller in reset\n");
		return STATUS_DEVICE_POWER_FAILURE;
	}

	//If wakeup not requested, leave in reset state
	if (!wakeup)
		return STATUS_SUCCESS;

	udelay(100);
	gctl = hda_read32(fdoCtx, GCTL);
	hda_write32(fdoCtx, GCTL, gctl | HDA_GCTL_RESET);

	for (int count = 0; count < 1000; count++) {
		gctl = hda_read32(fdoCtx, GCTL);
		if (gctl & HDA_GCTL_RESET) {
			break;
		}
		udelay(10);
	}
	if (!(gctl & HDA_GCTL_RESET)) {
		DbgPrint("Error: controller stuck in reset\n");
		return STATUS_DEVICE_POWER_FAILURE;
	}

	//Wait for codecs to finish their own reset sequence. Delay from VoodooHDA so it resets properly
	udelay(1000);

	if (!fdoCtx->codecMask) {
		fdoCtx->codecMask = hda_read16(fdoCtx, STATESTS);
		SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
			"codec mask = 0x%lx\n", fdoCtx->codecMask);
	}

	return STATUS_SUCCESS;
}

NTSTATUS GetHDACapabilities(PFDO_CONTEXT fdoCtx) {
	UINT16 gcap = hda_read16(fdoCtx, GCAP);
	SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"chipset global capabilities = 0x%x\n", gcap);

	fdoCtx->is64BitOK = (gcap & 0x1);
	if (!fdoCtx->is64BitOK) {
		return STATUS_DEVICE_PROTOCOL_ERROR; //64 bit required
	}

	fdoCtx->hwVersion = (hda_read8(fdoCtx, VMAJ) << 8) | hda_read8(fdoCtx, VMIN);

	fdoCtx->captureStreams = (gcap >> 8) & 0x0f;
	fdoCtx->playbackStreams = (gcap >> 12) & 0x0f;

	SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"streams (cap %d, playback %d)\n", fdoCtx->captureStreams, fdoCtx->playbackStreams);

	fdoCtx->captureIndexOff = 0;
	fdoCtx->playbackIndexOff = fdoCtx->captureStreams;
	fdoCtx->numStreams = fdoCtx->captureStreams + fdoCtx->playbackStreams;

	UINT8 corbSize = hda_read8(fdoCtx, CORBSIZE);
	if (!(corbSize & 0x40)) {
		return STATUS_DEVICE_FEATURE_NOT_SUPPORTED; //CORB must support 256
	}

	UINT8 rirbSize = hda_read8(fdoCtx, RIRBSIZE);
	if (!(rirbSize & 0x40)) {
		return STATUS_DEVICE_FEATURE_NOT_SUPPORTED; //RIRB must support 256
	}

	return STATUS_SUCCESS;
}

void HDAInitCorb(PFDO_CONTEXT fdoCtx) {
	//Set the corb size to 256 entries
	hda_write8(fdoCtx, CORBSIZE, 0x02);

	//Setup CORB address
	fdoCtx->corb.buf = (UINT32*)fdoCtx->rb;
	fdoCtx->corb.addr = MmGetPhysicalAddress(fdoCtx->corb.buf);
	hda_write32(fdoCtx, CORBLBASE, fdoCtx->corb.addr.LowPart);
	hda_write32(fdoCtx, CORBUBASE, fdoCtx->corb.addr.HighPart);

	//Set WP and RP
	fdoCtx->corb.wp = 0;
	hda_write16(fdoCtx, CORBWP, fdoCtx->corb.wp);
	hda_write16(fdoCtx, CORBRP, HDA_CORBRP_RST);

	udelay(10); //Delay for 10 us to reset

	hda_write16(fdoCtx, CORBRP, 0);
}

void HDAInitRirb(PFDO_CONTEXT fdoCtx) {
	//Set the rirb size to 256 entries
	hda_write8(fdoCtx, RIRBSIZE, 0x02);

	//Setup CORB address
	fdoCtx->rirb.buf = (UINT32*)(fdoCtx->rb + 0x800);
	fdoCtx->rirb.addr = MmGetPhysicalAddress(fdoCtx->rirb.buf);
	RtlZeroMemory(fdoCtx->rirb.cmds, sizeof(fdoCtx->rirb.cmds));
	hda_write32(fdoCtx, RIRBLBASE, fdoCtx->rirb.addr.LowPart);
	hda_write32(fdoCtx, RIRBUBASE, fdoCtx->rirb.addr.HighPart);

	//Set WP and RP
	fdoCtx->rirb.rp = 0;
	hda_write16(fdoCtx, RIRBWP, HDA_RIRBWP_RST);

	//Set interrupt threshold
	hda_write16(fdoCtx, RINTCNT, 1);

	//Enable Received response reporting
	hda_write8(fdoCtx, RIRBCTL, HDA_RBCTL_IRQ_EN);
}

void HDAStartCorb(PFDO_CONTEXT fdoCtx) {
	UINT8 corbCTL;
	corbCTL = hda_read8(fdoCtx, CORBCTL);
	corbCTL |= HDA_CORBCTL_RUN;
	hda_write8(fdoCtx, CORBCTL, corbCTL);
}

void HDAStartRirb(PFDO_CONTEXT fdoCtx) {
	UINT8 rirbCTL;
	rirbCTL = hda_read8(fdoCtx, RIRBCTL);
	rirbCTL |= HDA_RBCTL_DMA_EN;
	hda_write8(fdoCtx, RIRBCTL, rirbCTL);
}

NTSTATUS StartHDAController(PFDO_CONTEXT fdoCtx) {
	NTSTATUS status;
	WdfInterruptAcquireLock(fdoCtx->Interrupt);

	status = ResetHDAController(fdoCtx, TRUE);
	if (!NT_SUCCESS(status)) {
		goto exit;
	}

	//Clear STATESTS
	hda_write16(fdoCtx, STATESTS, STATESTS_INT_MASK);

	HDAInitCorb(fdoCtx);
	HDAInitRirb(fdoCtx);

	HDAStartCorb(fdoCtx);
	HDAStartRirb(fdoCtx);

	//Enabling Controller Interrupt
	hda_write32(fdoCtx, GCTL, hda_read32(fdoCtx, GCTL) | HDA_GCTL_UNSOL);
	hda_write32(fdoCtx, INTCTL, hda_read32(fdoCtx, INTCTL) | HDA_INT_CTRL_EN | HDA_INT_GLOBAL_EN);

	//Program position buffer
	PHYSICAL_ADDRESS posbufAddr = MmGetPhysicalAddress(fdoCtx->posbuf);
	hda_write32(fdoCtx, DPLBASE, posbufAddr.LowPart);
	hda_write32(fdoCtx, DPUBASE, posbufAddr.HighPart);

	udelay(1000);

exit:
	WdfInterruptReleaseLock(fdoCtx->Interrupt);
	return status;
}

NTSTATUS StopHDAController(PFDO_CONTEXT fdoCtx) {
	WdfInterruptAcquireLock(fdoCtx->Interrupt);

	NTSTATUS status = ResetHDAController(fdoCtx, FALSE);

	WdfInterruptReleaseLock(fdoCtx->Interrupt);

	return status;
}

static UINT16 HDACommandAddr(UINT32 cmd) {
	return (cmd >> 28) & 0xF;
}

NTSTATUS SendHDACmds(PFDO_CONTEXT fdoCtx, ULONG count, PHDAUDIO_CODEC_TRANSFER CodecTransfer) {
	WdfInterruptAcquireLock(fdoCtx->Interrupt);
	for (ULONG i = 0; i < count; i++) {
		PHDAUDIO_CODEC_TRANSFER transfer = &CodecTransfer[i];
		RtlZeroMemory(&transfer->Input, sizeof(transfer->Input));

		UINT16 addr = HDACommandAddr(transfer->Output.Command);

		//Add command to corb
		UINT16 wp = hda_read16(fdoCtx, CORBWP);
		if (wp == 0xffff) {
			//Something wrong, controller likely went to sleep
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"%s: device not found\n", __func__);
			WdfInterruptReleaseLock(fdoCtx->Interrupt);
			return STATUS_DEVICE_DOES_NOT_EXIST;
		}

		wp++;
		wp %= HDA_MAX_CORB_ENTRIES;

		UINT16 rp = hda_read16(fdoCtx, CORBRP);
		if (wp == rp) {
			//Oops it's full
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"%s: device busy\n", __func__);
			WdfInterruptReleaseLock(fdoCtx->Interrupt);
			return STATUS_RETRY;
		}

		fdoCtx->rirb.xfer[addr][fdoCtx->rirb.cmds[addr]] = transfer;
		InterlockedIncrement(&fdoCtx->rirb.cmds[addr]);

		fdoCtx->corb.buf[wp] = transfer->Output.Command;

		hda_write16(fdoCtx, CORBWP, wp);
	}

	WdfInterruptReleaseLock(fdoCtx->Interrupt);
	return STATUS_SUCCESS;
}

NTSTATUS RunSingleHDACmd(PFDO_CONTEXT fdoCtx, ULONG val, ULONG* res) {
	HDAUDIO_CODEC_TRANSFER transfer = { 0 };
	transfer.Output.Command = val;

	NTSTATUS status = SendHDACmds(fdoCtx, 1, &transfer);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	int timeout_ms = 1000;
	for (ULONG loopcounter = 0; ; loopcounter++) {
		if (transfer.Input.IsValid) {
			if (res) {
				*res = transfer.Input.Response;
			}
			return STATUS_SUCCESS;
		}

		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			UINT16 addr = HDACommandAddr(transfer.Output.Command);

			InterlockedDecrement(&fdoCtx->rirb.cmds[addr]);
			return STATUS_IO_TIMEOUT;
		}

		udelay(100);
	}
}

//Old
#define HDA_RIRB_EX_UNSOL_EV	(1<<4)

void hdac_bus_process_unsol_events(PFDO_CONTEXT fdoCtx) {
	UINT rp, caddr, res;
	while (fdoCtx->unsol_rp != fdoCtx->unsol_wp) {
		rp = (fdoCtx->unsol_rp + 1) % HDA_UNSOL_QUEUE_SIZE;
		fdoCtx->unsol_rp = rp;
		rp <<= 1;
		res = fdoCtx->unsol_queue[rp];
		caddr = fdoCtx->unsol_queue[rp + 1];

		if (!(caddr & (1 << 4))) //no unsolicited event
			continue;

		UINT8 addr = caddr & 0x0f;
		PPDO_DEVICE_DATA codec = fdoCtx->codecs[addr];
		if (!codec || codec->FdoContext != fdoCtx)
			continue;

		HDAUDIO_CODEC_RESPONSE response;
		RtlZeroMemory(&response, sizeof(HDAUDIO_CODEC_RESPONSE));

		response.Response = res;
		response.IsUnsolicitedResponse = 1;

		UINT tag = response.Unsolicited.Tag;
		if (codec->unsolitCallbacks[tag].inUse && codec->unsolitCallbacks[tag].Routine) {
			codec->unsolitCallbacks[tag].Routine(response, codec->unsolitCallbacks[tag].Context);
		}
	}
}

void hdac_bus_update_rirb(PFDO_CONTEXT fdoCtx) {
	UINT rp, wp;
	UINT32 res, res_ex;
	UINT16 addr;

	wp = hda_read16(fdoCtx, RIRBWP);
	if (wp == 0xffff) {
		//Something wrong, controller likely went to sleep
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"%s: device not found\n", __func__);
		return;
	}

	if (wp == fdoCtx->rirb.wp)
		return;
	fdoCtx->rirb.wp = wp;

	while (fdoCtx->rirb.rp != wp) {
		fdoCtx->rirb.rp++;
		fdoCtx->rirb.rp %= HDA_MAX_RIRB_ENTRIES;

		rp = fdoCtx->rirb.rp << 1; /* an RIRB entry is 8-bytes */
		res_ex = fdoCtx->rirb.buf[rp + 1];
		res = fdoCtx->rirb.buf[rp];
		addr = res_ex & 0xf;
		if (addr >= HDA_MAX_CODECS) {
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"spurious response %#x: %#x, rp = % d, wp = % d\n",
				res, res_ex, fdoCtx->rirb.rp, wp);
		}
		else if (res_ex & HDA_RIRB_EX_UNSOL_EV) {
			UINT unsol_wp = (fdoCtx->unsol_wp + 1) % HDA_UNSOL_QUEUE_SIZE;
			fdoCtx->unsol_wp = unsol_wp;

			unsol_wp <<= 1;
			fdoCtx->unsol_queue[unsol_wp] = res;
			fdoCtx->unsol_queue[unsol_wp + 1] = res_ex;

			fdoCtx->processUnsol = TRUE;
		}
		else if (fdoCtx->rirb.cmds[addr]) {
			DbgPrint("Got response for addr 0x%x\n", addr);

			LONG curCmd = fdoCtx->rirb.cmds[addr] - 1;
			curCmd %= HDA_MAX_CORB_ENTRIES;

			fdoCtx->rirb.xfer[addr][curCmd]->Input.Response = res;
			fdoCtx->rirb.xfer[addr][curCmd]->Input.IsValid = TRUE;
			InterlockedDecrement(&fdoCtx->rirb.cmds[addr]);

			if (!fdoCtx->rirb.cmds[addr]) {
				//TODO: Notify for RIRB processed
			}
		}
		else {
			LONG curCmd = fdoCtx->rirb.cmds[addr] - 1;
			curCmd %= HDA_MAX_CORB_ENTRIES;

			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"spurious response %#x:%#x, last cmd=%#08x\n",
				res, res_ex, fdoCtx->rirb.xfer[addr][curCmd]->Output.Command);
		}
	}
}

int hda_stream_interrupt(PFDO_CONTEXT fdoCtx, unsigned int status) {
	int handled = 0;
	UINT8 sd_status;

	for (UINT32 i = 0; i < fdoCtx->numStreams; i++) {
		PHDAC_STREAM stream = &fdoCtx->streams[i];
		if (status & stream->int_sta_mask) {
			sd_status = stream_read8(stream, SD_STS);
			stream_write8(stream, SD_STS, SD_INT_MASK);
			handled |= 1 << stream->idx;

			if (sd_status & SD_INT_COMPLETE)
				stream->irqReceived = TRUE;
		}
	}
	return handled;
}

BOOLEAN hda_interrupt(
	WDFINTERRUPT Interrupt,
	ULONG MessageID) {
	UNREFERENCED_PARAMETER(MessageID);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PFDO_CONTEXT fdoCtx = Fdo_GetContext(Device);

	BOOLEAN active, handled = FALSE;

	if (fdoCtx->dspInterruptCallback) {
		handled = (BOOLEAN)fdoCtx->dspInterruptCallback(fdoCtx->dspInterruptContext);
	}

	int repeat = 0; //Avoid endless loop
	do {
		UINT32 status = hda_read32(fdoCtx, INTSTS);
		if (status == 0 || status == 0xffffffff)
			break;

		handled = TRUE;
		active = FALSE;

		if (hda_stream_interrupt(fdoCtx, status)) {
			WdfInterruptQueueDpcForIsr(Interrupt);
			active = TRUE;
		}

		status = hda_read16(fdoCtx, RIRBSTS);
		if (status & RIRB_INT_MASK) {
			/*
			 * Clearing the interrupt status here ensures that no
			 * interrupt gets masked after the RIRB wp is read in
			 * snd_hdac_bus_update_rirb. This avoids a possible
			 * race condition where codec response in RIRB may
			 * remain unserviced by IRQ, eventually falling back
			 * to polling mode in azx_rirb_get_response.
			 */
			hda_write16(fdoCtx, RIRBSTS, RIRB_INT_MASK);
			active = TRUE;
			if (status & RIRB_INT_RESPONSE) {
				hdac_bus_update_rirb(fdoCtx);
			}
		}
	} while (active && ++repeat < 10);

	if (fdoCtx->processUnsol) {
		WdfInterruptQueueDpcForIsr(Interrupt);
	}

	return handled;
}

void hda_dpc(
	WDFINTERRUPT Interrupt,
	WDFOBJECT AssociatedObject
) {
	UNREFERENCED_PARAMETER(AssociatedObject);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PFDO_CONTEXT fdoCtx = Fdo_GetContext(Device);

	for (UINT32 i = 0; i < fdoCtx->numStreams; i++) {
		PHDAC_STREAM stream = &fdoCtx->streams[i];
		if (stream->irqReceived) {
			stream->irqReceived = FALSE;

			for (int j = 0; j < MAX_NOTIF_EVENTS; j++) {
				if (stream->registeredCallbacks[j].InUse) {
					LARGE_INTEGER unknownVal = { 0 };
					KeQuerySystemTimePrecise(&unknownVal);
					stream->registeredCallbacks[j].NotificationCallback(stream->registeredCallbacks[j].CallbackContext, unknownVal);
				}
			}

			for (int j = 0; j < MAX_NOTIF_EVENTS; j++) {
				if (stream->registeredEvents[j]) {
					KeSetEvent(stream->registeredEvents[j], IO_NO_INCREMENT, FALSE);
				}
			}
		}
	}

	if (fdoCtx->processUnsol) {
		fdoCtx->processUnsol = FALSE;
		hdac_bus_process_unsol_events(fdoCtx);
	}
}