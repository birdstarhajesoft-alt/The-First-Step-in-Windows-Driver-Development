#include "ExampleCallout.h"

#define FORMAT_ADDR(x) (x>>24)&0xFF, (x>>16)&0xFF, (x>>8)&0xFF, x&0xFF

/*************************
	ClassifyFn Function
**************************/
void example_classify(
	const FWPS_INCOMING_VALUES* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT* classifyOut)
{
	UINT32 local_address = inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_LOCAL_ADDRESS].value.uint32;
	UINT32 remote_address = inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32;
	UINT16 local_port = inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_LOCAL_PORT].value.uint16;
	UINT16 remote_port = inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_PORT].value.uint16;

	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(flowContext);
	UNREFERENCED_PARAMETER(filter);

	// If the packet is destined for remote port 21, block the packet
	if (remote_port == 21) {
		DbgPrint("Blocking Packet to port 21\n");
		classifyOut->actionType = FWP_ACTION_BLOCK;
		return;
	}
	// Otherwise, print its TCP 4-tuple
	else {
		DbgPrint("Example Classify found a packet: %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu\n",
			FORMAT_ADDR(local_address), local_port, FORMAT_ADDR(remote_address), remote_port);
	}

	classifyOut->actionType = FWP_ACTION_PERMIT;
	return;
}

/*************************
	NotifyFn Function
**************************/
NTSTATUS example_notify(
	FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	const GUID* filterKey,
	const FWPS_FILTER* filter)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	switch (notifyType) {
	case FWPS_CALLOUT_NOTIFY_ADD_FILTER:
		DbgPrint("A new filter has registered Example Callout as its action");
		break;
	case FWPS_CALLOUT_NOTIFY_DELETE_FILTER:
		DbgPrint("A filter that uses Example Callout has just been deleted");
		break;
	}
	return status;
}

