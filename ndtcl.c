#include <assert.h>
#include "tcl.h"
#include "tclInt.h"
#include "tclCompile.h"

static int
ndcmd(
	ClientData clientData,  /* Not used. */
	Tcl_Interp *interp,     /* Current interpreter */
	int objc,               /* Number of arguments */
	Tcl_Obj *const objv[]   /* Argument strings */
	)
{
	Interp *iPtr = (Interp *)interp;
	CmdFrame *frame = iPtr->cmdFramePtr;

	assert(frame->type == TCL_LOCATION_BC);

	ByteCode *bc = frame->data.tebc.codePtr;
	unsigned char *pc = frame->data.tebc.pc;
	unsigned char opCode = *pc;
	const InstructionDesc *instDesc = //&tclInstructionTable[opCode];
		&(((const InstructionDesc *)TclGetInstructionTable())[opCode]);
	int i, numbytes = 1;
	for (i = 0;  i < instDesc->numOperands;  i++) {
		switch (instDesc->opTypes[i]) {
		case OPERAND_INT1:
		case OPERAND_UINT1:
		case OPERAND_LVT1:
			numbytes++;
			break;
		case OPERAND_INT4:
		case OPERAND_AUX4:
		case OPERAND_UINT4:
		case OPERAND_IDX4:
		case OPERAND_LVT4:
			numbytes += 4;
			break;
		case OPERAND_NONE:
		default:
			break;
		}
	}
	unsigned char *nextPc = pc + numbytes;
	unsigned char nextOpCode = *nextPc;

	int retLen;
	char *retVal;
	if (nextOpCode == INST_DONE || nextOpCode == INST_POP) {
		retVal = "XXXX";
	} else {
		retVal = "mypassword";
	}

	Tcl_Obj *tRet = Tcl_NewByteArrayObj(retVal, strlen(retVal));
	Tcl_SetObjResult(interp, tRet);
	return TCL_OK;
}

int
_Ndtcl_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, "8.5", 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgRequire(interp, "Tcl", "8.5", 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgProvide(interp, "nd", "0.1") != TCL_OK)
		return TCL_ERROR;

	Tcl_Command cmd = Tcl_CreateObjCommand(interp, "nd",
		(Tcl_ObjCmdProc *) ndcmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	if (cmd == NULL)
		return TCL_ERROR;

	return TCL_OK;
}
