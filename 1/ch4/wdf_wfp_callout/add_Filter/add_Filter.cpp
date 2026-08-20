#include <windows.h>
#include <fwpmu.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "Fwpuclnt")

int main() {
    DWORD ret;
    HANDLE hEngine;
    FWPM_FILTER filter = { 0, };
    WCHAR filterName[] = L"Blocker Internet(hajesoft)";
    FWPM_FILTER_CONDITION cond;

    // open a handle to the WFP engine
    ret = FwpmEngineOpen(nullptr, RPC_C_AUTHN_DEFAULT, nullptr, nullptr, &hEngine);

    /// Specify IP address as condition
    FWP_V4_ADDR_AND_MASK ipv4 = { 0x08'08'08'08 /*8.8.8.8*/,
                                 0xFF'FF'FF'FF /*255.255.255.255*/ };

    cond.fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS; // field
    cond.matchType = FWP_MATCH_EQUAL;
    cond.conditionValue.type = FWP_V4_ADDR_MASK;
    cond.conditionValue.v4AddrMask = &ipv4;
    filter.filterCondition = &cond;
    filter.numFilterConditions = 1;
    filter.displayData.name = filterName;
    filter.action.type = FWP_ACTION_BLOCK; // Ban
//    filter.action.type = FWP_ACTION_PERMIT; // Permit
    filter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4; 
    ret = FwpmFilterAdd(hEngine, &filter, nullptr, nullptr);

    // close engine handle
    FwpmEngineClose(hEngine);
    return 0;
}