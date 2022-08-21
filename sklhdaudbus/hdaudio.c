#include "driver.h"

NTSTATUS HDA_TransferCodecVerbs(
	_In_ PVOID _context,
	_In_ ULONG Count,
	_Inout_updates_(Count)
	PHDAUDIO_CODEC_TRANSFER CodecTransfer,
	_In_opt_ PHDAUDIO_TRANSFER_COMPLETE_CALLBACK Callback,
	_In_opt_ PVOID Context
) {
	//SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Count: %d)!\n", __func__, Count);

	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	for (ULONG i = 0; i < Count; i++) {
		PHDAUDIO_CODEC_TRANSFER transfer = &CodecTransfer[i];
		/*if ((transfer->Output.Command & 0x70000) == 0x70000) {
			SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "Command8: 0x%x (Node: 0x%x, Verb: 0x%x, Parameter: 0x%x)\n", transfer->Output.Command, transfer->Output.Verb8.Node, transfer->Output.Verb8.VerbId, transfer->Output.Verb8.Data);
		} 
		else {
			SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "Command16: 0x%x (Node: 0x%x, Verb: 0x%x, Parameter: 0x%x)\n", transfer->Output.Command, transfer->Output.Verb16.Node, transfer->Output.Verb16.VerbId, transfer->Output.Verb16.Data);
		}*/
		RtlZeroMemory(&transfer->Input, sizeof(transfer->Input));
		UINT32 response = 0;
		status = hdac_bus_exec_verb(devData->FdoContext, devData->CodecIds.CodecAddress, transfer->Output.Command, &response);
		transfer->Input.Response = response;
		if (NT_SUCCESS(status)) {
			transfer->Input.IsValid = 1;
			//DbgPrint("Complete Response: 0x%llx\n", transfer->Input.CompleteResponse);
		} else {
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "%s: Verb exec failed! 0x%x\n", __func__, status);
		}
	}

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

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PFDO_CONTEXT fdoContext = devData->FdoContext;

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	for (int i = 0; i < fdoContext->captureStreams; i++) {
		int tag = fdoContext->captureIndexOff;
		PHDAC_STREAM stream = &fdoContext->streams[i];
		if (stream->PdoContext != NULL) {
			continue;
		}

		SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s Allocated capture stream idx %d, tag %d!\n", __func__, i, tag);

		stream->PdoContext = devData;
		stream->prepared = FALSE;
		stream->running = FALSE;
		stream->streamFormat = StreamFormat;

		ConverterFormat->ConverterFormat = 0;
		ConverterFormat->BitsPerSample = StreamFormat->ValidBitsPerSample;
		ConverterFormat->NumberOfChannels = StreamFormat->NumberOfChannels;
		ConverterFormat->SampleRate = StreamFormat->SampleRate;
		ConverterFormat->StreamType = 0;

		if (Handle)
			*Handle = (HANDLE)stream;

		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_SUCCESS;
	}

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	return STATUS_INSUFFICIENT_RESOURCES;
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
		PHDAC_STREAM stream = &fdoContext->streams[i];
		if (stream->PdoContext != NULL) {
			continue;
		}

		SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s Allocated render stream idx %d, tag %d!\n", __func__, i, tag);

		stream->PdoContext = devData;
		stream->prepared = FALSE;
		stream->running = FALSE;
		stream->streamFormat = StreamFormat;

		ConverterFormat->ConverterFormat = 0;
		ConverterFormat->BitsPerSample = StreamFormat->ValidBitsPerSample;
		ConverterFormat->NumberOfChannels = StreamFormat->NumberOfChannels;
		ConverterFormat->SampleRate = StreamFormat->SampleRate;
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

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	if (stream->prepared || stream->running) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	stream->streamFormat = StreamFormat;

	ConverterFormat->ConverterFormat = 0;
	ConverterFormat->BitsPerSample = StreamFormat->ValidBitsPerSample;
	ConverterFormat->NumberOfChannels = StreamFormat->NumberOfChannels;
	ConverterFormat->SampleRate = StreamFormat->SampleRate;
	ConverterFormat->StreamType = 0;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	return STATUS_SUCCESS;
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

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->prepared || stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PHYSICAL_ADDRESS lowAddr;
	lowAddr.QuadPart = 0;
	PHYSICAL_ADDRESS maxAddr;
	maxAddr.QuadPart = MAXULONG64;

	PHYSICAL_ADDRESS skipBytes;
	skipBytes.QuadPart = 0;

	if (KeGetCurrentIrql() > APC_LEVEL) {
		return STATUS_UNSUCCESSFUL;
	}

	

	PMDL mdl = MmAllocatePagesForMdlEx(lowAddr, maxAddr, skipBytes, RequestedBufferSize, MmNonCached, 0);
	if (!mdl) {
		return STATUS_NO_MEMORY;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	stream->mdlBuf = mdl;
	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	*BufferMdl = mdl;
	*AllocatedBufferSize = mdl->ByteCount;
	*StreamId = stream->streamTag;
	*FifoSize = 0;

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s: Requested %lld, got %lld bytes\n", __func__, RequestedBufferSize, *AllocatedBufferSize);

	mdelay(1000);

	//TODO: Program DMA to device

	return STATUS_SUCCESS;
}

NTSTATUS HDA_FreeDmaBuffer(
	_In_ PVOID _context,
	_In_ HANDLE Handle
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->prepared || stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (!stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	MmFreePagesFromMdl(stream->mdlBuf);
	stream->mdlBuf = NULL;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s done!\n", __func__);
	mdelay(1000);

	//TODO: Deprogram DMA from device

	return STATUS_SUCCESS;
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

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	if (stream->prepared || stream->running) {
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
	return STATUS_NO_SUCH_DEVICE;
}

NTSTATUS HDA_GetWallClockRegister(
	_In_ PVOID _context,
	_Out_ PULONG* Wallclock
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}
	*Wallclock = (devData->FdoContext)->m_BAR0.Base.baseptr + HDA_REG_WALLCLK;

	return STATUS_SUCCESS;
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

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	*Position = stream->posbuf;

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

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!_context || !devData->FdoContext)
		return STATUS_NO_SUCH_DEVICE;

	if (DeviceInformation->Size < sizeof(HDAUDIO_DEVICE_INFORMATION)) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	DeviceInformation->CodecsDetected = devData->FdoContext->numCodecs;
	DeviceInformation->DeviceVersion = 2 << 4;
	DeviceInformation->DriverVersion = 1 << 4;
	DeviceInformation->IsStripingSupported = TRUE;

	if (DeviceInformation->Size >= sizeof(HDAUDIO_DEVICE_INFORMATION_V2)) {
		DeviceInformation->Size = sizeof(HDAUDIO_DEVICE_INFORMATION_V2);

		PHDAUDIO_DEVICE_INFORMATION_V2 DeviceInformation2 = (PHDAUDIO_DEVICE_INFORMATION_V2)DeviceInformation;
		DeviceInformation2->CtrlRevision = devData->CodecIds.RevId;
		DeviceInformation2->CtrlVendorId = devData->CodecIds.CtlrVenId;
		DeviceInformation2->CtrlDeviceId = devData->CodecIds.CtlrDevId;
	}
	else {
		DeviceInformation->Size = sizeof(HDAUDIO_DEVICE_INFORMATION);
	}
	return STATUS_SUCCESS;
}

NTSTATUS HDA_GetResourceInformation(
	_In_ PVOID _context,
	_Out_ PUCHAR CodecAddress,
	_Out_ PUCHAR FunctionGroupStartNode
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;
	
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (CodecAddress)
		*CodecAddress = devData->CodecIds.CodecAddress;
	if (FunctionGroupStartNode)
		*FunctionGroupStartNode = devData->CodecIds.FunctionGroupStartNode;

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called (Addr: %d, Start: %d)!\n", __func__, devData->CodecIds.CodecAddress, devData->CodecIds.FunctionGroupStartNode);
	return STATUS_SUCCESS;
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

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->prepared || stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PHYSICAL_ADDRESS lowAddr;
	lowAddr.QuadPart = 0;
	PHYSICAL_ADDRESS maxAddr;
	maxAddr.QuadPart = MAXULONG64;

	PHYSICAL_ADDRESS skipBytes;
	skipBytes.QuadPart = 0;

	if (KeGetCurrentIrql() > APC_LEVEL) {
		return STATUS_UNSUCCESSFUL;
	}

	PMDL mdl = MmAllocatePagesForMdlEx(lowAddr, maxAddr, skipBytes, RequestedBufferSize, MmNonCached, 0);
	if (!mdl) {
		return STATUS_NO_MEMORY;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	stream->mdlBuf = mdl;
	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	*BufferMdl = mdl;
	*AllocatedBufferSize = mdl->ByteCount;
	*OffsetFromFirstPage = 0;
	*StreamId = stream->streamTag;
	*FifoSize = 0;

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s: Requested %lld, got %lld bytes\n", __func__, RequestedBufferSize, *AllocatedBufferSize);
	mdelay(1000);

	//TODO: Program DMA to device

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

	PHDAC_STREAM stream = Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	if (stream->prepared || stream->running) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (!stream->mdlBuf) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	MmFreePagesFromMdl(stream->mdlBuf);
	stream->mdlBuf = NULL;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s done!\n", __func__);
	mdelay(1000);

	//TODO: Deprogram DMA from device

	return STATUS_SUCCESS;
}

NTSTATUS HDA_RegisterNotificationEvent(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PKEVENT NotificationEvent
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);
	return STATUS_NO_SUCH_DEVICE;
}

NTSTATUS HDA_UnregisterNotificationEvent(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ PKEVENT NotificationEvent
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);
	return STATUS_NO_SUCH_DEVICE;
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
	busInterface.AllocateCaptureDmaEngine = HDA_AllocateCaptureDmaEngine;
	busInterface.AllocateRenderDmaEngine = HDA_AllocateRenderDmaEngine;
	busInterface.ChangeBandwidthAllocation = HDA_ChangeBandwidthAllocation;
	busInterface.AllocateDmaBuffer = HDA_AllocateDmaBuffer; //TODO
	busInterface.FreeDmaBuffer = HDA_FreeDmaBuffer; //TODO
	busInterface.FreeDmaEngine = HDA_FreeDmaEngine;
	busInterface.SetDmaEngineState = HDA_SetDmaEngineState; //TODO
	busInterface.GetWallClockRegister = HDA_GetWallClockRegister;
	busInterface.GetLinkPositionRegister = HDA_GetLinkPositionRegister;
	busInterface.RegisterEventCallback = HDA_RegisterEventCallback;
	busInterface.UnregisterEventCallback = HDA_UnregisterEventCallback;
	busInterface.GetDeviceInformation = HDA_GetDeviceInformation;
	busInterface.GetResourceInformation = HDA_GetResourceInformation;
	busInterface.AllocateDmaBufferWithNotification = HDA_AllocateDmaBufferWithNotification; //TODO
	busInterface.FreeDmaBufferWithNotification = HDA_FreeDmaBufferWithNotification; //TODO
	busInterface.RegisterNotificationEvent = HDA_RegisterNotificationEvent; //TODO
	busInterface.UnregisterNotificationEvent = HDA_UnregisterNotificationEvent; //TODO

	return busInterface;
}