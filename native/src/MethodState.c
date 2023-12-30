// Copyright (c) 2012 DotNetAnywhere
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "Compat.h"
#include "Sys.h"

#include "Thread.h"
#include "MethodState.h"
#include "JIT.h"

tMethodState* MethodState_Direct(tThread *pThread, tMD_MethodDef *pMethod, tMethodState *pCaller, U32 isInternalNewObjCall) {
	tMethodState *pThis;

	if (!pMethod->isFilled) {
		tMD_TypeDef *pTypeDef;

		pTypeDef = MetaData_GetTypeDefFromMethodDef(pMethod);
		MetaData_Fill_TypeDef(pTypeDef, NULL, NULL);
	}

	pThis = (tMethodState*)Thread_StackAlloc(pThread, sizeof(tMethodState));
	pThis->finalizerThis = NULL;
	pThis->pCaller = pCaller;
	pThis->pMetaData = pMethod->pMetaData;
	pThis->pMethod = pMethod;
	if (pMethod->pJITted == NULL) {
		// If method has not already been JITted
		JIT_Prepare(pMethod);
	}
	pThis->pJIT = pMethod->pJITted;
	pThis->ipOffset = 0;
	pThis->pEvalStack = (PTR)Thread_StackAlloc(pThread, pThis->pMethod->pJITted->maxStack);
	pThis->stackOfs = 0;
	pThis->isInternalNewObjCall = isInternalNewObjCall;
	pThis->pNextDelegate = NULL;
	pThis->pDelegateParams = NULL;

	pThis->pParamsLocals = (PTR)Thread_StackAlloc(pThread, pMethod->parameterStackSize + pMethod->pJITted->localsStackSize);
	memset(pThis->pParamsLocals, 0, pMethod->parameterStackSize + pMethod->pJITted->localsStackSize);

#ifdef DIAG_METHOD_CALLS
	// Keep track of the number of times this method is called
	pMethod->callCount++;
	pThis->startTime = microTime();
#endif

	return pThis;
}

tMethodState* MethodState(tThread *pThread, tMetaData *pMetaData, IDX_TABLE methodToken, tMethodState *pCaller) {
	tMD_MethodDef *pMethod;

	pMethod = MetaData_GetMethodDefFromDefRefOrSpec(pMetaData, methodToken, NULL, NULL);
	return MethodState_Direct(pThread, pMethod, pCaller, 0);
}

void MethodState_Delete(tThread *pThread, tMethodState **ppMethodState) {
	tMethodState *pThis = *ppMethodState;


#ifdef DIAG_METHOD_CALLS
	pThis->pMethod->totalTime += microTime() - pThis->startTime;
#endif

	// If this MethodState is a Finalizer, then let the heap know this Finalizer has been run
	if (pThis->finalizerThis != NULL) {
		Heap_UnmarkFinalizer(pThis->finalizerThis);
	}

	if (pThis->pDelegateParams != NULL) {
		free(pThis->pDelegateParams);
	}

	// Note that the way the stack free funtion works means that only the 1st allocated chunk
	// needs to be free'd, as this function just sets the current allocation offset to the address given.
	Thread_StackFree(pThread, pThis);

	*ppMethodState = NULL;
}