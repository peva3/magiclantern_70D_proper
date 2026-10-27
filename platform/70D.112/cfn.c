#include <dryos.h>
#include <property.h>
#include <cfn-generic.h>

// on 70D these are not CFn, but in main menus
GENERIC_GET_ALO
GENERIC_GET_HTP
GENERIC_GET_MLU
GENERIC_SET_MLU

// PROP_CFN_TAB (0x80010007), length 0x1c on this body.
// Byte 7 holds the AF-button assignment: 0 = AF, 1 = metering, 2 = AeL.
// (The 6D port uses index 8 with a 0x1d-byte table - do not copy that here.)
static int8_t some_cfn[0x1c];	// length 0x1c for 70D
PROP_HANDLER(0x80010007)
{
    ASSERT(len == 0x1c);
    memcpy(some_cfn, buf, 0x1c);
}

// verify with GUI_GetCfnButtonSwap
int cfn_get_af_button_assignment() { return some_cfn[7]; }
void cfn_set_af_button(int value) 
{  
    some_cfn[7] = COERCE(value, 0, 2);
    prop_request_change(0x80010007, some_cfn, 0x1c);
} 
