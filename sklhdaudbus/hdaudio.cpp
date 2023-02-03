#include "driver.h"
#include "crabrave.h"

NTSTATUS HDA_TransferCodecVerbs(
	_In_ PVOID _context,
	_In_ ULONG Count,
	_Inout_updates_(Count)
	PHDAUDIO_CODEC_TRANSFER CodecTransfer,
	_In_opt_ PHDAUDIO_TRANSFER_COMPLETE_CALLBACK Callback,
	_In_opt_ PVOID Context
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Count: %d)!\n", __func__, Count);

	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	for (ULONG i = 0; i < Count; i++) {
		PHDAUDIO_CODEC_TRANSFER transfer = &CodecTransfer[i];
		RtlZeroMemory(&transfer->Input, sizeof(transfer->Input));
		UINT32 response = 0;
		//DbgPrint("Command: 0x%x\n", transfer->Output.Command);
		status = hdac_bus_exec_verb(devData->FdoContext, devData->CodecIds.CodecAddress, transfer->Output.Command, &response);
		transfer->Input.Response = response;
		if (NT_SUCCESS(status)) {
			transfer->Input.IsValid = 1;
			//DbgPrint("Complete Response: 0x%llx\n", transfer->Input.CompleteResponse);
		} else {
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "%s: Verb exec failed! 0x%x\n", __func__, status);
		}
	}

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s exit (Count: %d)!\n", __func__, Count);

	if (Callback) {
		DbgPrint("Got Callback\n");
		Callback(CodecTransfer, Context);
	}
	return STATUS_SUCCESS;
}

NTSTATUS HDA_AllocateCaptureDmaEngine(
	_In_ PVOID _context,
	_In_ UCHAR CodecAddress,
	_In_ PHDAUDIO_STREAM_FORMAT StreamFormat,
	_Out_ PHANDLE Handle,
	_Out_ PHDAUDIO_CONVERTER_FORMAT ConverterFormat
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS HDA_AllocateRenderDmaEngine(
	_In_ PVOID _context,
	_In_ PHDAUDIO_STREAM_FORMAT StreamFormat,
	_In_ BOOLEAN Stripe,
	_Out_ PHANDLE Handle,
	_Out_ PHDAUDIO_CONVERTER_FORMAT ConverterFormat
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PFDO_CONTEXT fdoContext = devData->FdoContext;

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	for (int i = 0; i < fdoContext->playbackStreams; i++) {
		int tag = fdoContext->playbackIndexOff;
		PHDAC_STREAM stream = &fdoContext->streams[tag];
		if (stream->PdoContext != NULL) {
			continue;
		}

		SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s Allocated render stream idx %d, tag %d, channels %d, bits %d, sample rate %d!\n", __func__, tag, stream->streamTag, StreamFormat->NumberOfChannels, StreamFormat->ValidBitsPerSample, StreamFormat->SampleRate);


		stream->stripe = Stripe;
		stream->PdoContext = devData;
		stream->running = FALSE;
		stream->streamFormat = *StreamFormat;

		ConverterFormat->ConverterFormat = 0;
		switch (StreamFormat->ValidBitsPerSample) {
		case 32:
			ConverterFormat->BitsPerSample = 4;
			break;
		case 24:
			ConverterFormat->BitsPerSample = 3;
			break;
		case 20:
			ConverterFormat->BitsPerSample = 2;
			break;
		case 16:
			ConverterFormat->BitsPerSample = 1;
			break;
		case 8:
		default:
			ConverterFormat->BitsPerSample = 1;
			break;
		}
		ConverterFormat->NumberOfChannels = StreamFormat->NumberOfChannels - 1;
		switch (StreamFormat->SampleRate) {
		case 192000:
			ConverterFormat->SampleRate = 24;
			break;
		case 96000:
			ConverterFormat->SampleRate = 8;
			break;
		case 48000:
			ConverterFormat->SampleRate = 0;
			break;
		case 44100:
		default:
			ConverterFormat->SampleRate = 64;
			break;
		}
		ConverterFormat->StreamType = 0;

		if (Handle)
			*Handle = (HANDLE)stream;

		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_SUCCESS;
	}

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS HDA_ChangeBandwidthAllocation(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PHDAUDIO_STREAM_FORMAT StreamFormat,
	_Out_ PHDAUDIO_CONVERTER_FORMAT ConverterFormat
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS HDA_AllocateDmaBuffer(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ SIZE_T RequestedBufferSize,
	_Out_ PMDL* BufferMdl,
	_Out_ PSIZE_T AllocatedBufferSize,
	_Out_ PUCHAR StreamId,
	_Out_ PULONG FifoSize
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Requested: %lld bytes, IRQL: %d)!\n", __func__, RequestedBufferSize, KeGetCurrentIrql());

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS HDA_FreeDmaBuffer(
	_In_ PVOID _context,
	_In_ HANDLE Handle
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS HDA_FreeDmaEngine(
	_In_ PVOID _context,
	_In_ HANDLE Handle
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	if (stream->running) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	stream->PdoContext = NULL;
	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	return STATUS_SUCCESS;
}

NTSTATUS HDA_SetDmaEngineState(
	_In_ PVOID _context,
	_In_ HDAUDIO_STREAM_STATE StreamState,
	_In_ ULONG NumberOfHandles,
	_In_reads_(NumberOfHandles) PHANDLE Handles
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	for (int i = 0; i < NumberOfHandles; i++) {
		PHDAC_STREAM stream = (PHDAC_STREAM)Handles[i];
		if (stream->PdoContext != devData) {
			return STATUS_INVALID_HANDLE;
		}

		WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

		if (StreamState == RunState && !stream->running) {
			hdac_stream_start(stream);
			stream->running = TRUE;
		}
		else if ((StreamState == PauseState || StreamState == StopState) && stream->running) {
			hdac_stream_stop(stream);
			stream->running = FALSE;
		}
		else if (StreamState == ResetState) {
			if (!stream->running) {
				hdac_stream_reset(stream);
			}
			else {
				return STATUS_INVALID_PARAMETER;
			}
		}

		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	}

	return STATUS_SUCCESS;
}

VOID HDA_GetWallClockRegister(
	_In_ PVOID _context,
	_Out_ PULONG* Wallclock
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return;
	}
	*Wallclock = (ULONG *)((devData->FdoContext)->m_BAR0.Base.baseptr + HDA_REG_WALLCLK);
}

NTSTATUS HDA_GetLinkPositionRegister(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_Out_ PULONG* Position
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	*Position = (ULONG *)stream->posbuf;

	return STATUS_SUCCESS;
}

NTSTATUS HDA_RegisterEventCallback(
	_In_ PVOID _context,
	_In_ PHDAUDIO_UNSOLICITED_RESPONSE_CALLBACK Routine,
	_In_opt_ PVOID Context,
	_Out_ PUCHAR Tag
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	if (!_context)
		return STATUS_NO_SUCH_DEVICE;


	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (devData->FdoContext) {
		WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	}

	for (int i = 0; i < MAX_UNSOLICIT_CALLBACKS; i++) {
		if (devData->unsolitCallbacks[i].inUse)
			continue;

		if (Tag)
			*Tag = i;

		SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s Allocated tag %d!\n", __func__, i);
		devData->unsolitCallbacks[i].inUse = TRUE;
		devData->unsolitCallbacks[i].Context = Context;
		devData->unsolitCallbacks[i].Routine = Routine;

		if (devData->FdoContext) {
			WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		}
		return STATUS_SUCCESS;
	}

	if (devData->FdoContext) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	}
	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS HDA_UnregisterEventCallback(
	_In_ PVOID _context,
	_In_ UCHAR Tag
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->unsolitCallbacks[Tag].inUse) {
		SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s Not registered!\n", __func__);
		return STATUS_NOT_FOUND;
	}

	if (devData->FdoContext) {
		WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	}

	devData->unsolitCallbacks[Tag].Routine = NULL;
	devData->unsolitCallbacks[Tag].Context = NULL;
	devData->unsolitCallbacks[Tag].inUse = FALSE;

	if (devData->FdoContext) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	}

	return STATUS_SUCCESS;
}

NTSTATUS HDA_GetDeviceInformation(
	_In_ PVOID _context,
	_Inout_ PHDAUDIO_DEVICE_INFORMATION DeviceInformation
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	return STATUS_UNSUCCESSFUL;
}

void HDA_GetResourceInformation(
	_In_ PVOID _context,
	_Out_ PUCHAR CodecAddress,
	_Out_ PUCHAR FunctionGroupStartNode
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);
	if (!_context)
		return;
	
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (CodecAddress)
		*CodecAddress = devData->CodecIds.CodecAddress;
	if (FunctionGroupStartNode)
		*FunctionGroupStartNode = devData->CodecIds.FunctionGroupStartNode;

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Addr: %d, Start: %d)!\n", __func__, devData->CodecIds.CodecAddress, devData->CodecIds.FunctionGroupStartNode);
}

HDAUDIO_BUS_INTERFACE HDA_BusInterface(PVOID Context) {
	HDAUDIO_BUS_INTERFACE busInterface;
	RtlZeroMemory(&busInterface, sizeof(HDAUDIO_BUS_INTERFACE));

	busInterface.Size = sizeof(HDAUDIO_BUS_INTERFACE);
	busInterface.Version = 0x0100;
	busInterface.Context = Context;
	busInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	busInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	busInterface.TransferCodecVerbs = HDA_TransferCodecVerbs;
	busInterface.AllocateCaptureDmaEngine = HDA_AllocateCaptureDmaEngine;
	busInterface.AllocateRenderDmaEngine = HDA_AllocateRenderDmaEngine;
	busInterface.ChangeBandwidthAllocation = HDA_ChangeBandwidthAllocation;
	busInterface.AllocateDmaBuffer = HDA_AllocateDmaBuffer;
	busInterface.FreeDmaBuffer = HDA_FreeDmaBuffer;
	busInterface.FreeDmaEngine = HDA_FreeDmaEngine;
	busInterface.SetDmaEngineState = HDA_SetDmaEngineState;
	busInterface.GetWallClockRegister = HDA_GetWallClockRegister;
	busInterface.GetLinkPositionRegister = HDA_GetLinkPositionRegister;
	busInterface.RegisterEventCallback = HDA_RegisterEventCallback;
	busInterface.UnregisterEventCallback = HDA_UnregisterEventCallback;
	busInterface.GetDeviceInformation = HDA_GetDeviceInformation;
	busInterface.GetResourceInformation = HDA_GetResourceInformation;

	return busInterface;
}

NTSTATUS HDA_AllocateDmaBufferWithNotification(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ ULONG NotificationCount,
	_In_ SIZE_T RequestedBufferSize,
	_Out_ PMDL* BufferMdl,
	_Out_ PSIZE_T AllocatedBufferSize,
	_Out_ PSIZE_T OffsetFromFirstPage,
	_Out_ PUCHAR StreamId,
	_Out_ PULONG FifoSize
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Requested: %lld bytes, IRQL: %d)!\n", __func__, RequestedBufferSize, KeGetCurrentIrql());

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PHYSICAL_ADDRESS lowAddr;
	lowAddr.QuadPart = 0;
	PHYSICAL_ADDRESS maxAddr;
	maxAddr.QuadPart = MAXUINT64;

	PHYSICAL_ADDRESS skipBytes;
	skipBytes.QuadPart = 0;

	if (KeGetCurrentIrql() > APC_LEVEL) {
		return STATUS_UNSUCCESSFUL;
	}

	PMDL mdl = MmAllocatePagesForMdl(lowAddr, maxAddr, skipBytes, RequestedBufferSize);
	if (!mdl) {
		return STATUS_NO_MEMORY;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	stream->mdlBuf = mdl;
	stream->bufSz = RequestedBufferSize;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	*BufferMdl = mdl;
	*AllocatedBufferSize = RequestedBufferSize;
	*OffsetFromFirstPage = MmGetMdlByteOffset(mdl);
	*StreamId = stream->streamTag;

	DbgPrint("Notification Count: %d\n", NotificationCount);

	int frags = 0;
	{
		//Set up the BDL
		UINT32* bdl = stream->bdl;
		INT64 size = stream->bufSz;
		PPFN_NUMBER pfnArray = MmGetMdlPfnArray(mdl);
		UINT32 offset = 0;
		while (size > 0) {
			if (frags > HDA_MAX_BDL_ENTRIES) {
				DbgPrint("Too many BDL entries!\n");
				frags = HDA_MAX_BDL_ENTRIES;
				break;
			}

			UINT32 chunk = PAGE_SIZE;
			PFN_NUMBER pfn = pfnArray[frags];
			PHYSICAL_ADDRESS addr = { 0 };
			addr.QuadPart = pfn << PAGE_SHIFT;
			/* program the address field of the BDL entry */
			bdl[0] = addr.LowPart;
			bdl[1] = addr.HighPart;
			/* program the size field of the BDL entry */
			bdl[2] = (size > chunk) ? chunk : size;
			/* program the IOC to enable interrupt
			 * only when the whole fragment is processed
			 */
			size -= chunk;
			bdl[3] = (size > 0) ? 0 : 1;
			bdl += 4;
			frags++;
			offset += chunk;
		}
	}
	stream->frags = frags;

	hdac_stream_setup(stream);

	*FifoSize = stream->fifoSize;
	return STATUS_SUCCESS;
}

NTSTATUS HDA_FreeDmaBufferWithNotification(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PMDL BufferMdl,
	_In_ SIZE_T BufferSize
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (!stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	stream_write32(stream, SD_BDLPL, 0);
	stream_write32(stream, SD_BDLPU, 0);
	stream_write32(stream, SD_CTL, 0);

	MmFreePagesFromMdlEx(stream->mdlBuf, MM_DONT_ZERO_ALLOCATION);
	ExFreePool(stream->mdlBuf);
	stream->mdlBuf = NULL;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s done!\n", __func__);

	return STATUS_SUCCESS;
}

NTSTATUS HDA_RegisterNotificationEvent(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PKEVENT NotificationEvent
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	BOOL registered = FALSE;

	for (int i = 0; i < MAX_NOTIF_EVENTS; i++) {
		if (stream->registeredEvents[i])
			continue;
		stream->registeredEvents[i] = NotificationEvent;
		registered = true;
	}

	return registered ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS HDA_UnregisterNotificationEvent(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PKEVENT NotificationEvent
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	BOOL registered = FALSE;

	for (int i = 0; i < MAX_NOTIF_EVENTS; i++) {
		if (stream->registeredEvents[i] != NotificationEvent)
			continue;
		stream->registeredEvents[i] = NULL;
		registered = true;
	}

	return registered ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

HDAUDIO_BUS_INTERFACE_V2 HDA_BusInterfaceV2(PVOID Context) {
	HDAUDIO_BUS_INTERFACE_V2 busInterface;
	RtlZeroMemory(&busInterface, sizeof(HDAUDIO_BUS_INTERFACE_V2));

	busInterface.Size = sizeof(HDAUDIO_BUS_INTERFACE_V2);
	busInterface.Version = 0x0100;
	busInterface.Context = Context;
	busInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	busInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	busInterface.TransferCodecVerbs = HDA_TransferCodecVerbs;
	busInterface.AllocateCaptureDmaEngine = HDA_AllocateCaptureDmaEngine; //TODO
	busInterface.AllocateRenderDmaEngine = HDA_AllocateRenderDmaEngine;
	busInterface.ChangeBandwidthAllocation = HDA_ChangeBandwidthAllocation;  //TODO
	busInterface.AllocateDmaBuffer = HDA_AllocateDmaBuffer; //TODO
	busInterface.FreeDmaBuffer = HDA_FreeDmaBuffer; //TODO
	busInterface.FreeDmaEngine = HDA_FreeDmaEngine;
	busInterface.SetDmaEngineState = HDA_SetDmaEngineState; //TODO
	busInterface.GetWallClockRegister = HDA_GetWallClockRegister;
	busInterface.GetLinkPositionRegister = HDA_GetLinkPositionRegister;
	busInterface.RegisterEventCallback = HDA_RegisterEventCallback;
	busInterface.UnregisterEventCallback = HDA_UnregisterEventCallback;
	busInterface.GetDeviceInformation = HDA_GetDeviceInformation;  //TODO
	busInterface.GetResourceInformation = HDA_GetResourceInformation;
	busInterface.AllocateDmaBufferWithNotification = HDA_AllocateDmaBufferWithNotification;
	busInterface.FreeDmaBufferWithNotification = HDA_FreeDmaBufferWithNotification;
	busInterface.RegisterNotificationEvent = HDA_RegisterNotificationEvent; //TODO
	busInterface.UnregisterNotificationEvent = HDA_UnregisterNotificationEvent; //TODO

	return busInterface;
}