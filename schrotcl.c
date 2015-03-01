#include <assert.h>
#include "tcl.h"
#include "tclInt.h"
#include "tclCompile.h"

static int
schrocmd(
	ClientData clientData,  /* Not used. */
	Tcl_Interp *interp,     /* Current interpreter */
	int objc,               /* Number of arguments */
	Tcl_Obj *const objv[]   /* Argument strings */
	)
{
	Interp *iPtr = (Interp *)interp;
	CmdFrame *frame = iPtr->cmdFramePtr;
	int stackLevel = iPtr->framePtr->level;
	int evalLevel = iPtr->numLevels;

	assert(frame->type == TCL_LOCATION_BC);

	// Find our current position in the bytecode
	const ByteCode *bc = frame->data.tebc.codePtr;
	const unsigned char *pc = frame->data.tebc.pc;
	unsigned char opCode = *pc;
	// Find how big this instruction is
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
	// Go to the next instruction
	const unsigned char *nextPc = pc + numbytes;
	unsigned char nextOpCode = *nextPc;

	// If the next instruction will discard the value, it'll either be
	// discarded (so it doesn't matter what we return) or will be printed on
	// the repl!
	int retLen;
	char *retVal;
	if (stackLevel == 0 && evalLevel == 0 &&
			(nextOpCode == INST_DONE || nextOpCode == INST_POP)) {
		// Possible repl candidate
		retVal = "cat!";
	} else {
		// Value could be used...
		retVal = "box";
	}

	Tcl_Obj *tRet = Tcl_NewByteArrayObj(retVal, strlen(retVal));
	Tcl_SetObjResult(interp, tRet);
	return TCL_OK;
}

int
Schro_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, "8.6", 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgRequire(interp, "Tcl", "8.6", 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgProvide(interp, "schro", "0.1") != TCL_OK)
		return TCL_ERROR;

	Tcl_Command cmd = Tcl_CreateObjCommand(interp, "schro",
		(Tcl_ObjCmdProc *) schrocmd, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	if (cmd == NULL)
		return TCL_ERROR;

	return TCL_OK;
}
