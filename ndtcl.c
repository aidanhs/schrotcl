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
	char *str1 = "notme";
	char *str2 = "aidan";
	int len = 5;

	Interp *iPtr = (Interp *)interp;
	CmdFrame *frame = iPtr->cmdFramePtr;

	assert(frame->type == TCL_LOCATION_BC);

	ByteCode *bc = frame->data.tebc.codePtr;
	unsigned char *pc = frame->data.tebc.pc;
	printf("bc %p\n", bc);
	printf("pc %p\n", pc);
	unsigned pcOffset = pc - bc->codeStart;
	printf("pco %d\n", pcOffset);

	Tcl_Obj *tBc = Tcl_NewObj();
	tBc->internalRep.twoPtrValue.ptr1 = bc;
	printf("dis %s\n", Tcl_GetString(TclDisassembleByteCodeObj(tBc)));
	printf("%s\n", bc->source);

	printf("=============\n");
	Tcl_Obj *tRet = Tcl_NewByteArrayObj(str2, len);
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





/* All of these are taken directly from the TCL source and unmodified */

static int
FormatInstruction(ByteCode *codePtr, const unsigned char *pc, Tcl_Obj *bufferObj);
static void
PrintSourceToObj(Tcl_Obj *appendObj, const char *stringPtr, int maxChars);

/*
 * Given an object which is of bytecode type, return a disassembled
 * version of the bytecode (in a new refcount 0 object). No guarantees
 * are made about the details of the contents of the result.
 */
Tcl_Obj *
TclDisassembleByteCodeObj(
	Tcl_Obj *objPtr) /* The bytecode object to disassemble. */
{
	ByteCode *codePtr = objPtr->internalRep.twoPtrValue.ptr1;
	unsigned char *codeStart, *codeLimit, *pc;
	unsigned char *codeDeltaNext, *codeLengthNext;
	unsigned char *srcDeltaNext, *srcLengthNext;
	int codeOffset, codeLen, srcOffset, srcLen, numCmds, delta, i;
	Interp *iPtr = (Interp *) *codePtr->interpHandle;
	Tcl_Obj *bufferObj;
	char ptrBuf1[20], ptrBuf2[20];

	bufferObj = Tcl_NewObj();
	if (codePtr->refCount <= 0) {
		return bufferObj;/* Already freed. */
	}

	codeStart = codePtr->codeStart;
	codeLimit = codeStart + codePtr->numCodeBytes;
	numCmds = codePtr->numCommands;

	/* Print header lines describing the ByteCode. */

	sprintf(ptrBuf1, "%p", codePtr);
	sprintf(ptrBuf2, "%p", iPtr);
	Tcl_AppendPrintfToObj(bufferObj,
		"ByteCode 0x%s, refCt %u, epoch %u, interp 0x%s (epoch %u)\n",
		ptrBuf1, codePtr->refCount, codePtr->compileEpoch, ptrBuf2,
		iPtr->compileEpoch);
	Tcl_AppendToObj(bufferObj, "  Source ", -1);
	PrintSourceToObj(bufferObj, codePtr->source,
		TclMin(codePtr->numSrcBytes, 55));
	Tcl_AppendPrintfToObj(bufferObj,
		"\n  Cmds %d, src %d, inst %d, litObjs %u, aux %d, stkDepth %u, code/src %.2f\n",
		numCmds, codePtr->numSrcBytes, codePtr->numCodeBytes,
		codePtr->numLitObjects, codePtr->numAuxDataItems,
		codePtr->maxStackDepth,
		0.0);

	/*
	 * If the ByteCode is the compiled body of a Tcl procedure, print
	 * information about that procedure. Note that we don't know the
	 * procedure's name since ByteCode's can be shared among procedures.
	 */

	if (codePtr->procPtr != NULL) {
		Proc *procPtr = codePtr->procPtr;
		int numCompiledLocals = procPtr->numCompiledLocals;

		sprintf(ptrBuf1, "%p", procPtr);
		Tcl_AppendPrintfToObj(bufferObj,
			"  Proc 0x%s, refCt %d, args %d, compiled locals %d\n",
			ptrBuf1, procPtr->refCount, procPtr->numArgs,
			numCompiledLocals);
		if (numCompiledLocals > 0) {
			CompiledLocal *localPtr = procPtr->firstLocalPtr;

			for (i = 0;  i < numCompiledLocals;  i++) {
			Tcl_AppendPrintfToObj(bufferObj,
				"      slot %d%s%s%s%s%s%s", i,
				(localPtr->flags & (VAR_ARRAY|VAR_LINK)) ? "" : ", scalar",
				(localPtr->flags & VAR_ARRAY) ? ", array" : "",
				(localPtr->flags & VAR_LINK) ? ", link" : "",
				(localPtr->flags & VAR_ARGUMENT) ? ", arg" : "",
				(localPtr->flags & VAR_TEMPORARY) ? ", temp" : "",
				(localPtr->flags & VAR_RESOLVED) ? ", resolved" : "");
			if (TclIsVarTemporary(localPtr)) {
				Tcl_AppendToObj(bufferObj, "\n", -1);
			} else {
				Tcl_AppendPrintfToObj(bufferObj, ", \"%s\"\n",
					localPtr->name);
			}
			localPtr = localPtr->nextPtr;
			}
		}
	}

	/*
	 * Print the ExceptionRange array.
	 */

	if (codePtr->numExceptRanges > 0) {
		Tcl_AppendPrintfToObj(bufferObj, "  Exception ranges %d, depth %d:\n",
			codePtr->numExceptRanges, codePtr->maxExceptDepth);
		for (i = 0;  i < codePtr->numExceptRanges;  i++) {
			ExceptionRange *rangePtr = &codePtr->exceptArrayPtr[i];

			Tcl_AppendPrintfToObj(bufferObj,
				"      %d: level %d, %s, pc %d-%d, ",
				i, rangePtr->nestingLevel,
				(rangePtr->type==LOOP_EXCEPTION_RANGE ? "loop" : "catch"),
				rangePtr->codeOffset,
				(rangePtr->codeOffset + rangePtr->numCodeBytes - 1));
			switch (rangePtr->type) {
			case LOOP_EXCEPTION_RANGE:
			Tcl_AppendPrintfToObj(bufferObj, "continue %d, break %d\n",
				rangePtr->continueOffset, rangePtr->breakOffset);
			break;
			case CATCH_EXCEPTION_RANGE:
			Tcl_AppendPrintfToObj(bufferObj, "catch %d\n",
				rangePtr->catchOffset);
			break;
			default:
			Tcl_Panic("TclDisassembleByteCodeObj: bad ExceptionRange type %d",
				rangePtr->type);
			}
		}
	}

	/*
	 * If there were no commands (e.g., an expression or an empty string was
	 * compiled), just print all instructions and return.
	 */

	if (numCmds == 0) {
		pc = codeStart;
		while (pc < codeLimit) {
			Tcl_AppendToObj(bufferObj, "    ", -1);
			pc += FormatInstruction(codePtr, pc, bufferObj);
		}
		return bufferObj;
	}

	/*
	 * Print table showing the code offset, source offset, and source length
	 * for each command. These are encoded as a sequence of bytes.
	 */

	Tcl_AppendPrintfToObj(bufferObj, "  Commands %d:", numCmds);
	codeDeltaNext = codePtr->codeDeltaStart;
	codeLengthNext = codePtr->codeLengthStart;
	srcDeltaNext = codePtr->srcDeltaStart;
	srcLengthNext = codePtr->srcLengthStart;
	codeOffset = srcOffset = 0;
	for (i = 0;  i < numCmds;  i++) {
		if ((unsigned) *codeDeltaNext == (unsigned) 0xFF) {
			codeDeltaNext++;
			delta = TclGetInt4AtPtr(codeDeltaNext);
			codeDeltaNext += 4;
		} else {
			delta = TclGetInt1AtPtr(codeDeltaNext);
			codeDeltaNext++;
		}
		codeOffset += delta;

		if ((unsigned) *codeLengthNext == (unsigned) 0xFF) {
			codeLengthNext++;
			codeLen = TclGetInt4AtPtr(codeLengthNext);
			codeLengthNext += 4;
		} else {
			codeLen = TclGetInt1AtPtr(codeLengthNext);
			codeLengthNext++;
		}

		if ((unsigned) *srcDeltaNext == (unsigned) 0xFF) {
			srcDeltaNext++;
			delta = TclGetInt4AtPtr(srcDeltaNext);
			srcDeltaNext += 4;
		} else {
			delta = TclGetInt1AtPtr(srcDeltaNext);
			srcDeltaNext++;
		}
		srcOffset += delta;

		if ((unsigned) *srcLengthNext == (unsigned) 0xFF) {
			srcLengthNext++;
			srcLen = TclGetInt4AtPtr(srcLengthNext);
			srcLengthNext += 4;
		} else {
			srcLen = TclGetInt1AtPtr(srcLengthNext);
			srcLengthNext++;
		}

		Tcl_AppendPrintfToObj(bufferObj, "%s%4d: pc %d-%d, src %d-%d",
			((i % 2)? "     " : "\n   "),
			(i+1), codeOffset, (codeOffset + codeLen - 1),
			srcOffset, (srcOffset + srcLen - 1));
	}
	if (numCmds > 0) {
		Tcl_AppendToObj(bufferObj, "\n", -1);
	}

	/*
	 * Print each instruction. If the instruction corresponds to the start of
	 * a command, print the command's source. Note that we don't need the code
	 * length here.
	 */

	codeDeltaNext = codePtr->codeDeltaStart;
	srcDeltaNext = codePtr->srcDeltaStart;
	srcLengthNext = codePtr->srcLengthStart;
	codeOffset = srcOffset = 0;
	pc = codeStart;
	for (i = 0;  i < numCmds;  i++) {
		if ((unsigned) *codeDeltaNext == (unsigned) 0xFF) {
			codeDeltaNext++;
			delta = TclGetInt4AtPtr(codeDeltaNext);
			codeDeltaNext += 4;
		} else {
			delta = TclGetInt1AtPtr(codeDeltaNext);
			codeDeltaNext++;
		}
		codeOffset += delta;

		if ((unsigned) *srcDeltaNext == (unsigned) 0xFF) {
			srcDeltaNext++;
			delta = TclGetInt4AtPtr(srcDeltaNext);
			srcDeltaNext += 4;
		} else {
			delta = TclGetInt1AtPtr(srcDeltaNext);
			srcDeltaNext++;
		}
		srcOffset += delta;

		if ((unsigned) *srcLengthNext == (unsigned) 0xFF) {
			srcLengthNext++;
			srcLen = TclGetInt4AtPtr(srcLengthNext);
			srcLengthNext += 4;
		} else {
			srcLen = TclGetInt1AtPtr(srcLengthNext);
			srcLengthNext++;
		}

		/*
		 * Print instructions before command i.
		 */

		while ((pc-codeStart) < codeOffset) {
			Tcl_AppendToObj(bufferObj, "    ", -1);
			pc += FormatInstruction(codePtr, pc, bufferObj);
		}

		Tcl_AppendPrintfToObj(bufferObj, "  Command %d: ", i+1);
		PrintSourceToObj(bufferObj, (codePtr->source + srcOffset),
			TclMin(srcLen, 55));
		Tcl_AppendToObj(bufferObj, "\n", -1);
		}
		if (pc < codeLimit) {
		/*
		 * Print instructions after the last command.
		 */

		while (pc < codeLimit) {
			Tcl_AppendToObj(bufferObj, "    ", -1);
			pc += FormatInstruction(codePtr, pc, bufferObj);
		}
	}
	return bufferObj;
}

// Appends a representation of a bytecode instruction to a Tcl_Obj.
static int
FormatInstruction(
	ByteCode *codePtr,       /* Bytecode containing the instruction. */
	const unsigned char *pc, /* Points to first byte of instruction. */
	Tcl_Obj *bufferObj)      /* Object to append instruction info to. */
{
	Proc *procPtr = codePtr->procPtr;
	unsigned char opCode = *pc;
	register const InstructionDesc *instDesc = //&tclInstructionTable[opCode];

		&(((const InstructionDesc *)TclGetInstructionTable())[opCode]);
	unsigned char *codeStart = codePtr->codeStart;
	unsigned pcOffset = pc - codeStart;
	int opnd = 0, i, j, numBytes = 1;
	int localCt = procPtr ? procPtr->numCompiledLocals : 0;
	CompiledLocal *localPtr = procPtr ? procPtr->firstLocalPtr : NULL;
	/* Additional info to print after main opcode and immediates. */
	char suffixBuffer[128];
	char *suffixSrc = NULL;
	Tcl_Obj *suffixObj = NULL;
	AuxData *auxPtr = NULL;

	suffixBuffer[0] = '\0';
	Tcl_AppendPrintfToObj(bufferObj, "(%u) %s ", pcOffset, instDesc->name);
	for (i = 0;  i < instDesc->numOperands;  i++) {
		switch (instDesc->opTypes[i]) {
		case OPERAND_INT1:
			opnd = TclGetInt1AtPtr(pc+numBytes); numBytes++;
			if (opCode == INST_JUMP1 || opCode == INST_JUMP_TRUE1 || opCode == INST_JUMP_FALSE1) {
				sprintf(suffixBuffer, "pc %u", pcOffset+opnd);
			}
			Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
			break;
		case OPERAND_INT4:
			opnd = TclGetInt4AtPtr(pc+numBytes); numBytes += 4;
			if (opCode == INST_JUMP4 || opCode == INST_JUMP_TRUE4 || opCode == INST_JUMP_FALSE4) {
				sprintf(suffixBuffer, "pc %u", pcOffset+opnd);
			} else if (opCode == INST_START_CMD) {
				sprintf(suffixBuffer, "next cmd at pc %u", pcOffset+opnd);
			}
			Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
			break;
		case OPERAND_UINT1:
			opnd = TclGetUInt1AtPtr(pc+numBytes); numBytes++;
			if (opCode == INST_PUSH1) {
				suffixObj = codePtr->objArrayPtr[opnd];
			}
			Tcl_AppendPrintfToObj(bufferObj, "%u ", (unsigned) opnd);
			break;
		case OPERAND_AUX4:
		case OPERAND_UINT4:
			opnd = TclGetUInt4AtPtr(pc+numBytes); numBytes += 4;
			if (opCode == INST_PUSH4) {
				suffixObj = codePtr->objArrayPtr[opnd];
			} else if (opCode == INST_START_CMD && opnd != 1) {
				sprintf(suffixBuffer+strlen(suffixBuffer), ", %u cmds start here", opnd);
			}
			Tcl_AppendPrintfToObj(bufferObj, "%u ", (unsigned) opnd);
			if (instDesc->opTypes[i] == OPERAND_AUX4) {
				auxPtr = &codePtr->auxDataArrayPtr[opnd];
			}
			break;
		case OPERAND_IDX4:
			opnd = TclGetInt4AtPtr(pc+numBytes); numBytes += 4;
			if (opnd >= -1) {
				Tcl_AppendPrintfToObj(bufferObj, "%d ", opnd);
			} else if (opnd == -2) {
				Tcl_AppendPrintfToObj(bufferObj, "end ");
			} else {
				Tcl_AppendPrintfToObj(bufferObj, "end-%d ", -2-opnd);
			}
			break;
		case OPERAND_LVT1:
			opnd = TclGetUInt1AtPtr(pc+numBytes);
			numBytes++;
			goto printLVTindex;
		case OPERAND_LVT4:
			opnd = TclGetUInt4AtPtr(pc+numBytes);
			numBytes += 4;
		printLVTindex:
			if (localPtr != NULL) {
				if (opnd >= localCt) {
					Tcl_Panic("FormatInstruction: bad local var index %u (%u locals)",
						(unsigned) opnd, localCt);
				}
				for (j = 0;  j < opnd;  j++) {
					localPtr = localPtr->nextPtr;
				}
				if (TclIsVarTemporary(localPtr)) {
					sprintf(suffixBuffer, "temp var %u", (unsigned) opnd);
				} else {
					sprintf(suffixBuffer, "var ");
					suffixSrc = localPtr->name;
				}
			}
			Tcl_AppendPrintfToObj(bufferObj, "%%v%u ", (unsigned) opnd);
			break;
		case OPERAND_NONE:
		default:
			break;
		}
	}
	if (suffixObj) {
		const char *bytes;
		int length;

		Tcl_AppendToObj(bufferObj, "\t# ", -1);
		bytes = Tcl_GetStringFromObj(codePtr->objArrayPtr[opnd], &length);
		PrintSourceToObj(bufferObj, bytes, TclMin(length, 40));
	} else if (suffixBuffer[0]) {
		Tcl_AppendPrintfToObj(bufferObj, "\t# %s", suffixBuffer);
		if (suffixSrc) {
			PrintSourceToObj(bufferObj, suffixSrc, 40);
		}
	}
	Tcl_AppendToObj(bufferObj, "\n", -1);
	if (auxPtr && auxPtr->type->printProc) {
		Tcl_AppendToObj(bufferObj, "\t\t[", -1);
		auxPtr->type->printProc(auxPtr->clientData, bufferObj, codePtr, pcOffset);
		Tcl_AppendToObj(bufferObj, "]\n", -1);
	}
	return numBytes;
}

/* Appends a quoted representation of a string to a Tcl_Obj. */
static void
PrintSourceToObj(
	Tcl_Obj *appendObj,    /* The object to print the source to. */
	const char *stringPtr, /* The string to print. */
	int maxChars)          /* Maximum number of chars to print. */
{
	register const char *p;
	register int i = 0;

	if (stringPtr == NULL) {
	Tcl_AppendToObj(appendObj, "\"\"", -1);
	return;
	}

	Tcl_AppendToObj(appendObj, "\"", -1);
	p = stringPtr;
	for (;  (*p != '\0') && (i < maxChars);  p++, i++) {
		switch (*p) {
		case '"':
			Tcl_AppendToObj(appendObj, "\\\"", -1);
			continue;
		case '\f':
			Tcl_AppendToObj(appendObj, "\\f", -1);
			continue;
		case '\n':
			Tcl_AppendToObj(appendObj, "\\n", -1);
			continue;
		case '\r':
			Tcl_AppendToObj(appendObj, "\\r", -1);
			continue;
		case '\t':
			Tcl_AppendToObj(appendObj, "\\t", -1);
			continue;
		case '\v':
			Tcl_AppendToObj(appendObj, "\\v", -1);
			continue;
		default:
			Tcl_AppendPrintfToObj(appendObj, "%c", *p);
			continue;
		}
	}
	Tcl_AppendToObj(appendObj, "\"", -1);
}
