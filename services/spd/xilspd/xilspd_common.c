/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <string.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <lib/el3_runtime/context_mgmt.h>

#include "xilspd_private.h"

/*******************************************************************************
 * Given a XILSP entrypoint info pointer, entry point PC, register width,
 * cpu id & pointer to a context data structure, this function will
 * initialize xilsp context and entry point info for XILSP
 ******************************************************************************/
void xilspd_init_xilsp_ep_state(struct entry_point_info *xilsp_entry_point,
				uint32_t rw,
				uint64_t pc,
				xilsp_context_t *xilsp_ctx)
{
	uint32_t ep_attr;

	/* Passing a NULL context is a critical programming error */
	assert(xilsp_ctx);
	assert(xilsp_entry_point);
	assert(pc);

	/* Associate this context with the cpu specified */
	xilsp_ctx->mpidr = read_mpidr_el1();
	xilsp_ctx->state = 0;
	set_xilsp_pstate(xilsp_ctx->state, XILSP_PSTATE_OFF);

	cm_set_context(&xilsp_ctx->cpu_ctx, SECURE);

	/* initialise an entrypoint to set up the CPU context */
	ep_attr = SECURE | EP_ST_ENABLE;
	if (EP_GET_EE(xilsp_entry_point->h.attr))
		ep_attr |= EP_EE_BIG;
	SET_PARAM_HEAD(xilsp_entry_point, PARAM_EP, VERSION_1, ep_attr);

	xilsp_entry_point->pc = pc;
	xilsp_entry_point->spsr = SPSR_64(MODE_EL1,
					MODE_SP_ELX,
					DISABLE_ALL_EXCEPTIONS);
	memset(&xilsp_entry_point->args, 0, sizeof(xilsp_entry_point->args));
}

/*******************************************************************************
 * This function takes a XILSP context pointer and:
 * 1. Applies the S-EL1 system register context from xilsp_ctx->cpu_ctx.
 * 2. Saves the current C runtime state (callee saved registers) on the stack
 *    frame and saves a reference to this state.
 * 3. Calls el3_exit() so that the EL3 system and general purpose registers
 *    from the xilsp_ctx->cpu_ctx are used to enter the XILSP image.
 ******************************************************************************/
uint64_t xilspd_synchronous_sp_entry(xilsp_context_t *xilsp_ctx)
{
	uint64_t rc;

	assert(xilsp_ctx != NULL);
	assert(xilsp_ctx->c_rt_ctx == 0);

	/* Apply the Secure EL1 system register context and switch to it */
	assert(cm_get_context(SECURE) == &xilsp_ctx->cpu_ctx);
	cm_el1_sysregs_context_restore(SECURE);
	cm_set_next_eret_context(SECURE);

	rc = xilspd_enter_sp(&xilsp_ctx->c_rt_ctx);
#if DEBUG
	xilsp_ctx->c_rt_ctx = 0;
#endif

	return rc;
}


/*******************************************************************************
 * This function takes an SP context pointer and:
 * 1. Saves the S-EL1 system register context tp xilsp_ctx->cpu_ctx.
 * 2. Restores the current C runtime state (callee saved registers) from the
 *    stack frame using the reference to this state saved in xilspd_enter_sp().
 * 3. It does not need to save any general purpose or EL3 system register state
 *    as the generic smc entry routine should have saved those.
 ******************************************************************************/
void xilspd_synchronous_sp_exit(xilsp_context_t *xilsp_ctx, uint64_t ret)
{
	assert(xilsp_ctx != NULL);
	/* Save the Secure EL1 system register context */
	assert(cm_get_context(SECURE) == &xilsp_ctx->cpu_ctx);
	cm_el1_sysregs_context_save(SECURE);

	assert(xilsp_ctx->c_rt_ctx != 0);
	xilspd_exit_sp(xilsp_ctx->c_rt_ctx, ret);

	/* Should never reach here */
	assert(0);
}
